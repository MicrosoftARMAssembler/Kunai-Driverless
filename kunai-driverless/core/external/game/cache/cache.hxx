#pragma once

namespace cache {
    static constexpr auto world_thread_sleep = ch::milliseconds( 1 );
    static constexpr auto camera_thread_sleep = ch::milliseconds( 1 );
    static constexpr auto actor_thread_sleep = ch::milliseconds( 1 );
    static constexpr auto player_thread_sleep = ch::milliseconds( 1 );
    static constexpr auto player_stale_grace = ch::milliseconds( 250 );

    struct world_frame_t {
        std::vector<std::shared_ptr<c_player>> players;
    };

    struct tarray_header_t {
        std::uint64_t data;
        std::uint32_t count;
        std::uint32_t max_count;
    };

    struct camera_t {
        game::f_vector  location{};
        game::f_rotator rotation{};
        float           fov{ 90.f };
    };

    class c_cache {
    public:
        c_cache( ) = default;
        ~c_cache( ) { stop( ); }

        void start( ) {
            if ( m_running.exchange( true, std::memory_order_acq_rel ) )
                return;

            m_world_thread = std::thread( [ this ] { update_world( ); } );
            m_actor_thread = std::thread( [ this ] { update_actors( ); } );
            m_player_thread = std::thread( [ this ] { update_players( ); } );
        }

        void stop( ) {
            if ( !m_running.exchange( false, std::memory_order_acq_rel ) )
                return;

            if ( m_world_thread.joinable( ) )  m_world_thread.join( );
            if ( m_actor_thread.joinable( ) )  m_actor_thread.join( );
            if ( m_player_thread.joinable( ) ) m_player_thread.join( );

            m_world.store( nullptr, std::memory_order_relaxed );
            m_game_instance.store( nullptr, std::memory_order_relaxed );
            m_local_player.store( nullptr, std::memory_order_relaxed );
            m_player_controller.store( nullptr, std::memory_order_relaxed );
            m_player_camera_manager.store( nullptr, std::memory_order_relaxed );
            m_local_pawn.store( nullptr, std::memory_order_relaxed );
            m_local_vtable.store( 0, std::memory_order_relaxed );

            {
                mutex::exclusive_lock lock( m_player_mutex );
                m_players.clear( );
            }

            m_player_cache.clear( );
            m_player_last_seen.clear( );
        }

        std::shared_ptr<camera_t> get_camera( ) const noexcept {
            auto* camera_manager = m_player_camera_manager.load( std::memory_order_acquire );
            return read_camera( camera_manager );
        }

        game::a_player_controller* get_player_controller( ) const noexcept {
            return m_player_controller.load( std::memory_order_acquire );
        }

        std::vector<std::shared_ptr<c_player>> get_snapshot( ) const noexcept {
            mutex::shared_lock lock( m_player_mutex );
            return m_players;
        }

    private:
        std::atomic<bool> m_running{ false };
        std::thread       m_world_thread;
        std::thread       m_cam_thread;
        std::thread       m_actor_thread;
        std::thread       m_player_thread;

        std::atomic<game::u_world*>                    m_world{};
        std::atomic<game::u_game_instance*>            m_game_instance{};
        std::atomic<game::u_localplayer*>              m_local_player{};
        std::atomic<game::a_player_controller*>        m_player_controller{};
        std::atomic<game::a_player_camera_manager*>    m_player_camera_manager{};
        std::atomic<game::a_shooter_character*>        m_local_pawn{};
        std::atomic<std::uintptr_t>                    m_local_vtable{ 0 };

        std::shared_ptr<camera_t>                      m_camera{};
        std::shared_ptr<std::vector<game::a_shooter_character*>> m_actors{};

        mutable mutex::c_shared_mutex                  m_player_mutex{};
        std::vector<std::shared_ptr<c_player>>         m_players{};

        std::unordered_map<std::uint64_t, std::shared_ptr<c_player>>          m_player_cache{};
        std::unordered_map<std::uint64_t, ch::steady_clock::time_point>       m_player_last_seen{};

        std::vector<std::uintptr_t> collect_actors( game::u_world* world ) {
            if ( !world )
                return {};

            auto level_array = world->levels( );
            if ( !level_array.is_valid( ) ) return {};

            const int level_count = level_array.size( );
            if ( level_count <= 0 || level_count > 64 ) return {};

            std::vector<std::uintptr_t> level_ptrs( level_count );
            g_process->read_memory( level_array.get_addr( ), level_ptrs.data( ), level_count * sizeof( std::uintptr_t ) );

            std::vector<std::uintptr_t> raw_actors;
            raw_actors.reserve( 512 );

            for ( int i = 0; i < level_count; ++i ) {
                if ( !utility::is_valid_pointer( level_ptrs[ i ] ) ) continue;

                auto* level = reinterpret_cast< game::u_level* >( level_ptrs[ i ] );
                auto  actor_array = level->actor_clustor( );
                if ( !actor_array.is_valid( ) ) continue;

                const int actor_count = actor_array.size( );
                if ( actor_count <= 0 ) continue;

                const std::size_t prev_size = raw_actors.size( );
                raw_actors.resize( prev_size + actor_count );
                g_process->read_memory( actor_array.get_addr( ), raw_actors.data( ) + prev_size, actor_count * sizeof( std::uintptr_t ) );
            }

            raw_actors.erase(
                std::remove_if( raw_actors.begin( ), raw_actors.end( ),
                    [ ]( std::uintptr_t addr ) { return !utility::is_valid_pointer( addr ); } ),
                raw_actors.end( )
            );

            return raw_actors;
        }

        std::shared_ptr<camera_t> read_camera( game::a_player_camera_manager* camera_manager ) const {
            if ( !camera_manager )
                return nullptr;

            const auto base = reinterpret_cast< std::uint64_t >( camera_manager )
                + game::offsets::view_target
                + game::offsets::pov;

            struct camera_raw_t {
                game::f_vector  location;
                game::f_rotator rotation;
                float           fov;
            } raw{};

            process::batch_read_t requests[ 3 ] = {
                { base + game::offsets::camera_location, &raw.location, sizeof( raw.location ), false },
                { base + game::offsets::camera_rotation, &raw.rotation, sizeof( raw.rotation ), false },
                { base + game::offsets::camera_fov,      &raw.fov,      sizeof( raw.fov ),      false },
            };

            g_process->read_memory_batch( requests, 3 );
            if ( !requests[ 0 ].success || !requests[ 1 ].success || !requests[ 2 ].success )
                return nullptr;

            auto camera = std::make_shared<camera_t>( );
            camera->location = raw.location;
            camera->rotation = raw.rotation;
            camera->fov = raw.fov;
            return camera;
        }

        void update_world( ) {
            while ( m_running.load( std::memory_order_acquire ) ) {
                game::u_world* world = game::u_world::get( );
                game::u_game_instance* game_instance = nullptr;
                game::u_localplayer* local_player = nullptr;
                game::a_player_controller* player_controller = nullptr;
                game::a_player_camera_manager* player_camera_manager = nullptr;
                game::a_shooter_character* local_pawn = nullptr;
                std::uintptr_t                 local_vtable = 0;

                if ( world ) {
                    game_instance = world->owning_game_instance( );
                    if ( game_instance ) {
                        local_player = game_instance->get_localplayer( );
                        if ( local_player ) {
                            player_controller = local_player->player_controller( );
                            if ( player_controller ) {
                                player_camera_manager = player_controller->player_camera_manager( );
                                local_pawn = player_controller->acknowledged_pawn( );
                                if ( local_pawn ) {
                                    local_vtable = g_process->read<std::uintptr_t>(
                                        reinterpret_cast< std::uintptr_t >( local_pawn ) );
                                }
                            }
                        }
                    }
                }

                m_world.store( world, std::memory_order_release );
                m_game_instance.store( game_instance, std::memory_order_release );
                m_local_player.store( local_player, std::memory_order_release );
                m_player_controller.store( player_controller, std::memory_order_release );
                m_player_camera_manager.store( player_camera_manager, std::memory_order_release );
                m_local_pawn.store( local_pawn, std::memory_order_release );
                m_local_vtable.store( local_vtable, std::memory_order_release );

                std::this_thread::sleep_for( world_thread_sleep );
            }
        }

        void update_cam( ) {
            while ( m_running.load( std::memory_order_acquire ) ) {
                auto* camera_manager = m_player_camera_manager.load( std::memory_order_acquire );

                auto camera = read_camera( camera_manager );
                std::atomic_store_explicit( &m_camera, std::move( camera ), std::memory_order_release );

                std::this_thread::sleep_for( camera_thread_sleep );
            }
        }

        void update_actors( ) {
            while ( m_running.load( std::memory_order_acquire ) ) {
                auto* world = m_world.load( std::memory_order_acquire );
                auto* local_pawn = m_local_pawn.load( std::memory_order_acquire );

                if ( !world || !local_pawn ) {
                    std::atomic_store_explicit( &m_actors, std::shared_ptr<std::vector<game::a_shooter_character*>>{}, std::memory_order_release );
                    std::this_thread::sleep_for( actor_thread_sleep );
                    continue;
                }

                const auto local_vtable = m_local_vtable.load( std::memory_order_acquire );
                auto raw_actors = collect_actors( world );

                auto characters = std::make_shared<std::vector<game::a_shooter_character*>>( );
                characters->reserve( raw_actors.size( ) );

                for ( const auto addr : raw_actors ) {
                    if ( !local_vtable )
                        break;

                    if ( g_process->read<std::uintptr_t>( addr ) != local_vtable )
                        continue;

                    auto* character = reinterpret_cast< game::a_shooter_character* >( addr );
                    if ( character == local_pawn )
                        continue;

                    characters->push_back( character );
                }

                std::atomic_store_explicit( &m_actors, std::move( characters ), std::memory_order_release );

                std::this_thread::sleep_for( actor_thread_sleep );
            }
        }

        void update_players( ) {
            while ( m_running.load( std::memory_order_acquire ) ) {
                const auto now = ch::steady_clock::now( );
                auto* local_pawn = m_local_pawn.load( std::memory_order_acquire );
                auto       actors = std::atomic_load_explicit( &m_actors, std::memory_order_acquire );

                if ( local_pawn && actors && !actors->empty( ) ) {
                    for ( auto* character : *actors ) {
                        if ( !utility::is_valid_pointer( reinterpret_cast< std::uintptr_t >( character ) ) )
                            continue;

                        auto player = std::make_shared<c_player>( character );
                        if ( !player->update( local_pawn ) )
                            continue;

                        const auto actor_key = reinterpret_cast< std::uint64_t >( character );
                        m_player_cache[ actor_key ] = std::move( player );
                        m_player_last_seen[ actor_key ] = now;
                    }
                }

                for ( auto it = m_player_last_seen.begin( ); it != m_player_last_seen.end( ); ) {
                    if ( now - it->second > player_stale_grace ) {
                        m_player_cache.erase( it->first );
                        it = m_player_last_seen.erase( it );
                    }
                    else {
                        ++it;
                    }
                }

                std::vector<std::shared_ptr<c_player>> players;
                players.reserve( m_player_cache.size( ) );
                for ( const auto& entry : m_player_cache ) {
                    if ( entry.second )
                        players.push_back( entry.second );
                }

                {
                    mutex::exclusive_lock lock( m_player_mutex );
                    m_players = std::move( players );
                }

                std::this_thread::sleep_for( player_thread_sleep );
            }
        }
    };
}