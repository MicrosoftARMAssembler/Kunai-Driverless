#pragma once

namespace cache {
    class c_timer {
    private:
        std::chrono::high_resolution_clock::time_point m_start{};
        double  m_avg_us{ 0.0 };
        int     m_count{ 0 };

    public:
        c_timer( ) = default;
        ~c_timer( ) = default;

        void start( ) {
            m_start = std::chrono::high_resolution_clock::now( );
        }

        void end( const std::string& function, int max_ittr, bool debug = false ) {
            const auto end_time = std::chrono::high_resolution_clock::now( );
            const auto duration = std::chrono::duration_cast< std::chrono::microseconds >(
                end_time - m_start ).count( );

            m_avg_us = ( m_avg_us * m_count + static_cast< double >( duration ) )
                / static_cast< double >( m_count + 1 );
            ++m_count;

            if ( debug )
                logging::print(
                    oxorany( "[%s] avg: %.3f ms, last: %.3f ms, samples: %d" ),
                    function.c_str( ),
                    m_avg_us / 1000.0,
                    static_cast< double >( duration ) / 1000.0,
                    m_count
                );

            if ( m_count >= max_ittr )
                m_count = 0;
        }

        void        reset( ) { m_avg_us = 0.0; m_count = 0; }
        double      get_average( )    const { return m_avg_us; }
        double      get_average_ms( ) const { return m_avg_us / 1000.0; }
        int         count( )          const { return m_count; }
    };

    template<typename T>
    class c_double_buffer {
    public:
        c_double_buffer( )
            : m_buffers( std::make_unique<std::array<std::vector<T>, 2>>( ) )
            , m_front_index( 0 ) {
        }

        std::vector<T> get_front_buffer_copy( ) const {
            int front_idx = m_front_index.load( std::memory_order_relaxed );
            return ( *m_buffers )[ front_idx ];
        }

        std::vector<T>& get_back_buffer( ) {
            int front_idx = m_front_index.load( std::memory_order_relaxed );
            return ( *m_buffers )[ 1 - front_idx ];
        }

        void swap_buffers( ) {
            int current_front = m_front_index.load( std::memory_order_relaxed );
            m_front_index.store( 1 - current_front, std::memory_order_release );
        }

        void reset( ) {
            m_buffers = std::make_unique<std::array<std::vector<T>, 2>>( );
            m_front_index = 0;
        }

        void update_buffer( std::vector<T>&& new_data ) {
            auto& back_buffer = get_back_buffer( );
            back_buffer.clear( );
            back_buffer = std::move( new_data );
            swap_buffers( );
        }

        void update_buffer( const std::vector<T>& new_data ) {
            auto& back_buffer = get_back_buffer( );
            back_buffer.clear( );
            back_buffer = new_data;
            swap_buffers( );
        }

        size_t size( ) const {
            int front_idx = m_front_index.load( std::memory_order_relaxed );
            return ( *m_buffers )[ front_idx ].size( );
        }

        bool empty( ) const {
            int front_idx = m_front_index.load( std::memory_order_relaxed );
            return ( *m_buffers )[ front_idx ].empty( );
        }

        bool has_data( ) const {
            return !empty( );
        }

    private:
        std::unique_ptr<std::array<std::vector<T>, 2>> m_buffers;
        std::atomic<int> m_front_index;
    };

    enum class actor_tag_t : std::uint32_t {
        unknown = 0,
        player = 1,
    };

    class c_actor {
    public:
        actor_tag_t tag{ actor_tag_t::unknown };

        virtual bool update( game::a_actor* actor ) { return true; }
        virtual ~c_actor( ) = default;
    };

    struct bounds_t {
        game::f_vector min{};
        game::f_vector max{};
        game::f_vector center( ) const noexcept {
            return { ( min.x + max.x ) * 0.5f, ( min.y + max.y ) * 0.5f, ( min.z + max.z ) * 0.5f };
        }
        game::f_vector extent( ) const noexcept {
            return { ( max.x - min.x ), ( max.y - min.y ), ( max.z - min.z ) };
        }
    };

    class c_player : public c_actor {
    public:
        c_player( game::a_shooter_character* shooter_character ) : m_player( shooter_character ) {
            tag = actor_tag_t::player;
        }

        game::a_shooter_character* m_player;
        game::a_ares_player_state* m_player_state;
        game::u_scene_component* m_root_component;
        game::u_skeletal_mesh_component* m_mesh;
        game::u_damageable_component* m_damage_handler;
        game::u_ares_inventory* m_inventory;
        game::a_ares_equippable* m_equippable;
        game::bone_gender m_gender = game::bone_gender::unknown;
        game::f_vector m_head;
        game::f_vector m_chest;
        game::f_rotator m_rotation;
        bounds_t m_bounds;
        float m_health;
        float m_max_health = 100.f;
        float m_shield;
        float m_max_shield;
        bool m_alive;
        bool m_teammate;
        int m_ping;
        std::string m_player_name;
        std::string m_weapon_name;
        std::vector<game::f_vector> m_skeleton;

        bool update( game::a_shooter_character* local_pawn ) {
            const auto fname_key = g_process->read< std::int32_t >(
                reinterpret_cast< std::uint64_t >( m_player ) + 0x18
            );

            this->m_player_name = game::get_fname( static_cast< std::uint32_t >( fname_key ) );
            if ( !this->m_player_name.empty( ) ) {
                auto it = game::agent_map.find( this->m_player_name );

                if ( it == game::agent_map.end( ) ) {
                    constexpr const char* default_prefix = "Default__";
                    if ( this->m_player_name.rfind( default_prefix, 0 ) == 0 ) {
                        const std::string trimmed = this->m_player_name.substr( 9 );
                        it = game::agent_map.find( trimmed );
                    }
                }

                if ( it == game::agent_map.end( ) ) {
                    for ( const auto& [agent_class, agent_name] : game::agent_map ) {
                        if ( this->m_player_name.find( agent_class ) != std::string::npos ) {
                            this->m_player_name = agent_name;
                            it = game::agent_map.find( agent_class );
                            break;
                        }
                    }
                }

                if ( it != game::agent_map.end( ) )
                    this->m_player_name = it->second;
            }

            this->m_player_state = m_player->player_state( );
            if ( !m_player_state ) return false;

            this->m_root_component = m_player->root_component( );
            if ( !m_root_component ) return false;

            this->m_mesh = m_player->mesh( );
            if ( !m_mesh ) return false;

            this->m_damage_handler = m_player->damage_handler( );
            if ( !m_damage_handler ) return false;

            this->m_gender = m_mesh->get_gender( );
            if ( m_gender == game::bone_gender::unknown ) return false;

            this->m_teammate = m_player->was_ally( );
            if ( !m_teammate ) {
                const auto team_component = local_pawn->player_state( )->team_component( );
                if ( m_player_state->team_component( ) == team_component )
                    this->m_teammate = true;
            }

            this->m_ping = m_player_state->ping( );
            this->m_health = m_damage_handler->health( );
            this->m_shield = m_damage_handler->shield( );
            this->m_max_shield = m_damage_handler->max_shield( );
            this->m_alive = this->m_health > 0.f;
            this->m_head = m_mesh->get_bone_location( m_mesh->get_bone( m_gender, head ) );
            this->m_chest = m_mesh->get_bone_location( m_mesh->get_bone( m_gender, chest ) );
            this->m_rotation = m_root_component->relative_rotation( );
            this->m_bounds = this->get_bounds( );

            this->m_inventory = m_player->inventory( );
            if ( m_inventory ) {
                this->m_equippable = m_inventory->current_equippable( );
                if ( m_equippable ) {
                    const auto obj_name = game::get_fname( m_equippable );
                    const auto it = game::weapon_map.find( obj_name );
                    if ( it != game::weapon_map.end( ) ) {
                        this->m_weapon_name = it->second;
                    }
                    else if ( obj_name.find( "Totem" ) != std::string::npos ) {
                        this->m_weapon_name = "Totem";
                    }
                    else if ( obj_name.find( "Defuser" ) != std::string::npos ) {
                        this->m_weapon_name = "Defusing";
                    }
                    else if ( obj_name.find( "Ability" ) != std::string::npos ) {
                        this->m_weapon_name = "Ability";
                    }
                    else {
                        this->m_weapon_name = obj_name;
                    }
                }
            }

            //this->get_skeleton( );
            return true;
        }

    private:
        bounds_t get_bounds( ) {
            auto bone_array = m_mesh->get_bone_array( );
            if ( !bone_array.is_valid( ) || bone_array.count <= 0 ) return {};

            auto ctw_matrix = m_mesh->component_to_world( ).to_matrix( );

            const std::uint32_t relevant_bones[ ] = {
                m_mesh->resolve_bone( m_gender, game::female_bones::head,        game::male_bones::head ),
                m_mesh->resolve_bone( m_gender, game::female_bones::pelvis,      game::male_bones::pelvis ),
                m_mesh->resolve_bone( m_gender, game::female_bones::left_elbow,  game::male_bones::left_elbow ),
                m_mesh->resolve_bone( m_gender, game::female_bones::left_hand,   game::male_bones::left_hand ),
                m_mesh->resolve_bone( m_gender, game::female_bones::right_elbow, game::male_bones::right_elbow ),
                m_mesh->resolve_bone( m_gender, game::female_bones::right_hand,  game::male_bones::right_hand ),
                m_mesh->resolve_bone( m_gender, game::female_bones::left_thigh,  game::male_bones::left_thigh ),
                m_mesh->resolve_bone( m_gender, game::female_bones::left_foot,   game::male_bones::left_foot ),
                m_mesh->resolve_bone( m_gender, game::female_bones::right_thigh, game::male_bones::right_thigh ),
                m_mesh->resolve_bone( m_gender, game::female_bones::right_foot,  game::male_bones::right_foot ),
            };
            constexpr auto bone_count = std::size( relevant_bones );
            const auto     head_idx = relevant_bones[ 0 ];

            const auto base_addr = reinterpret_cast< std::uintptr_t >( bone_array.data );
            game::f_transform transforms[ bone_count ]{};
            process::batch_read_t requests[ bone_count ]{};

            for ( std::size_t i = 0; i < bone_count; ++i ) {
                requests[ i ].va = base_addr + relevant_bones[ i ] * sizeof( game::f_transform );
                requests[ i ].buf = &transforms[ i ];
                requests[ i ].size = sizeof( game::f_transform );
                requests[ i ].success = false;
            }

            g_process->read_memory_batch( requests, bone_count );

            bounds_t bounds{};
            bounds.min = { FLT_MAX,  FLT_MAX,  FLT_MAX };
            bounds.max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

            for ( std::size_t i = 0; i < bone_count; ++i ) {
                if ( !requests[ i ].success || relevant_bones[ i ] >= static_cast< std::uint32_t >( bone_array.count ) )
                    continue;

                const auto& t = transforms[ i ];
                game::f_vector world_pos{};
                world_pos.x = ctw_matrix.x_plane.x * t.translation.x + ctw_matrix.y_plane.x * t.translation.y + ctw_matrix.z_plane.x * t.translation.z + ctw_matrix.w_plane.x;
                world_pos.y = ctw_matrix.x_plane.y * t.translation.x + ctw_matrix.y_plane.y * t.translation.y + ctw_matrix.z_plane.y * t.translation.z + ctw_matrix.w_plane.y;
                world_pos.z = ctw_matrix.x_plane.z * t.translation.x + ctw_matrix.y_plane.z * t.translation.y + ctw_matrix.z_plane.z * t.translation.z + ctw_matrix.w_plane.z;

                if ( relevant_bones[ i ] == head_idx )
                    world_pos.z += 12.f;

                if ( world_pos.x < bounds.min.x ) bounds.min.x = world_pos.x;
                if ( world_pos.y < bounds.min.y ) bounds.min.y = world_pos.y;
                if ( world_pos.z < bounds.min.z ) bounds.min.z = world_pos.z;
                if ( world_pos.x > bounds.max.x ) bounds.max.x = world_pos.x;
                if ( world_pos.y > bounds.max.y ) bounds.max.y = world_pos.y;
                if ( world_pos.z > bounds.max.z ) bounds.max.z = world_pos.z;
            }

            return bounds;
        }

        void get_skeleton( ) {
            const auto& bone_array = m_mesh->get_bone_array( );
            if ( !bone_array.is_valid( ) || bone_array.count == 0 )
                return;

            auto ctw_matrix = m_mesh->component_to_world( ).to_matrix( );

            enum b : std::uint8_t {
                head, neck, chest, pelvis,
                l_shoulder, l_elbow, l_hand,
                r_shoulder, r_elbow, r_hand,
                l_thigh, l_knee, l_foot,
                r_thigh, r_knee, r_foot,
                count
            };

            const std::uint32_t unique_bones[ b::count ] = {
                m_mesh->resolve_bone( m_gender, game::female_bones::head,           game::male_bones::head ),
                m_mesh->resolve_bone( m_gender, game::female_bones::neck,           game::male_bones::neck ),
                m_mesh->resolve_bone( m_gender, game::female_bones::chest,          game::male_bones::chest ),
                m_mesh->resolve_bone( m_gender, game::female_bones::pelvis,         game::male_bones::pelvis ),
                m_mesh->resolve_bone( m_gender, game::female_bones::left_shoulder,  game::male_bones::left_shoulder ),
                m_mesh->resolve_bone( m_gender, game::female_bones::left_elbow,     game::male_bones::left_elbow ),
                m_mesh->resolve_bone( m_gender, game::female_bones::left_hand,      game::male_bones::left_hand ),
                m_mesh->resolve_bone( m_gender, game::female_bones::right_shoulder, game::male_bones::right_shoulder ),
                m_mesh->resolve_bone( m_gender, game::female_bones::right_elbow,    game::male_bones::right_elbow ),
                m_mesh->resolve_bone( m_gender, game::female_bones::right_hand,     game::male_bones::right_hand ),
                m_mesh->resolve_bone( m_gender, game::female_bones::left_thigh,     game::male_bones::left_thigh ),
                m_mesh->resolve_bone( m_gender, game::female_bones::left_knee,      game::male_bones::left_knee ),
                m_mesh->resolve_bone( m_gender, game::female_bones::left_foot,      game::male_bones::left_foot ),
                m_mesh->resolve_bone( m_gender, game::female_bones::right_thigh,    game::male_bones::right_thigh ),
                m_mesh->resolve_bone( m_gender, game::female_bones::right_knee,     game::male_bones::right_knee ),
                m_mesh->resolve_bone( m_gender, game::female_bones::right_foot,     game::male_bones::right_foot ),
            };

            const auto base_addr = reinterpret_cast< std::uintptr_t >( bone_array.data );
            game::f_transform transforms[ b::count ]{};
            process::batch_read_t requests[ b::count ]{};

            for ( std::uint8_t i = 0; i < b::count; ++i ) {
                requests[ i ].va = base_addr + unique_bones[ i ] * sizeof( game::f_transform );
                requests[ i ].buf = &transforms[ i ];
                requests[ i ].size = sizeof( game::f_transform );
                requests[ i ].success = false;
            }

            g_process->read_memory_batch( requests, b::count );

            game::f_vector pos[ b::count ]{};
            for ( std::uint8_t i = 0; i < b::count; ++i ) {
                if ( !requests[ i ].success || unique_bones[ i ] >= static_cast< std::uint32_t >( bone_array.count ) )
                    continue;

                const auto& t = transforms[ i ];
                pos[ i ] = {
                    ctw_matrix.x_plane.x * t.translation.x + ctw_matrix.y_plane.x * t.translation.y + ctw_matrix.z_plane.x * t.translation.z + ctw_matrix.w_plane.x,
                    ctw_matrix.x_plane.y * t.translation.x + ctw_matrix.y_plane.y * t.translation.y + ctw_matrix.z_plane.y * t.translation.z + ctw_matrix.w_plane.y,
                    ctw_matrix.x_plane.z * t.translation.x + ctw_matrix.y_plane.z * t.translation.y + ctw_matrix.z_plane.z * t.translation.z + ctw_matrix.w_plane.z,
                };
            }

            constexpr std::pair<b, b> pairs[ ] = {
                { pelvis,     chest      },
                { chest,      neck       },
                { neck,       head       },
                { chest,      l_shoulder },
                { l_shoulder, l_elbow    },
                { l_elbow,    l_hand     },
                { chest,      r_shoulder },
                { r_shoulder, r_elbow    },
                { r_elbow,    r_hand     },
                { pelvis,     l_thigh    },
                { l_thigh,    l_knee     },
                { l_knee,     l_foot     },
                { pelvis,     r_thigh    },
                { r_thigh,    r_knee     },
                { r_knee,     r_foot     },
            };

            m_skeleton.clear( );
            m_skeleton.reserve( std::size( pairs ) * 2 );

            for ( const auto& [a, b_bone] : pairs ) {
                m_skeleton.push_back( pos[ a ] );
                m_skeleton.push_back( pos[ b_bone ] );
            }
        }
    };
}