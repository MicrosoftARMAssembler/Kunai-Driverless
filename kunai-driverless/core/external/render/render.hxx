#pragma once

namespace render {
    struct player_candidate_t {
        cache::c_player* player;
        game::f_vector2d head_position;
        game::f_vector2d root_position;
        double           world_distance;
        bool             is_visible;
    };

    void start( ) {
        kunai::screen_w = GetSystemMetrics( SM_CXSCREEN );
        kunai::screen_h = GetSystemMetrics( SM_CYSCREEN );

        kunai::center_w = kunai::screen_w / 2;
        kunai::center_h = kunai::screen_h / 2;

        kunai::window win( kunai::screen_w, kunai::screen_h );
        kunai::device dev( win );

        kunai::font text_fnt( dev, kunai::embedded::roboto_medium_ttf, kunai::embedded::roboto_medium_ttf_size, 12 );
        kunai::text_renderer tr( dev, text_fnt );

        struct smoothed_bars_t {
            float health{ 1.f };
            float shield{ 1.f };
        };
        std::unordered_map<std::uint64_t, smoothed_bars_t> bar_state;

        struct fade_state_t {
            float alpha{ 1.f };
        };
        std::unordered_map<std::uint64_t, fade_state_t> fade_state;

        struct hitmarker_state_t {
            float last_health{ 0.f };
        };
        std::unordered_map<std::uint64_t, hitmarker_state_t> hitmarker_state;

        struct active_hitmarker_t {
            float         damage{ 0.f };
            float         time_created{ 0.f };
            std::uint64_t player_key{ 0 };
        };
        std::vector<active_hitmarker_t> active_hitmarkers;

        constexpr float hitmarker_lifetime = 2.0f;
        constexpr float hitmarker_rise_speed = 30.0f;
        constexpr float bar_lerp_speed = 4.f;

        static constexpr int   m_max_candidates = 100;
        player_candidate_t     candidates[ m_max_candidates ];

        auto t_prev = std::chrono::steady_clock::now( );
        while ( win.poll( ) ) {
            auto t_now = std::chrono::steady_clock::now( );
            float dt = std::chrono::duration<float>( t_now - t_prev ).count( );
            t_prev = t_now;
            if ( dt > 0.1f ) dt = 0.1f;

            // reset candidates every frame
            int candidate_count = 0;

            dev.begin_frame( kunai::color( 0.0f, 0.0f, 0.0f, 0.0f ) );

            const auto camera = g_cache->get_camera( );
            if ( camera ) {
                const auto players = g_cache->get_snapshot( );
                const auto fov_radius = ( 15.f * kunai::center_w / camera->fov ) / 2.0f;

                const float cy = std::cos( camera->rotation.yaw * ( std::numbers::pi_v<float> / 180.f ) );
                const float sy = std::sin( camera->rotation.yaw * ( std::numbers::pi_v<float> / 180.f ) );
                const float cp = std::cos( camera->rotation.pitch * ( std::numbers::pi_v<float> / 180.f ) );
                const float sp = std::sin( camera->rotation.pitch * ( std::numbers::pi_v<float> / 180.f ) );
                const float forward_x = cy * cp;
                const float forward_y = sy * cp;
                const float forward_z = sp;

                // prune stale per-player state maps
                {
                    std::unordered_set<std::uint64_t> active_keys;
                    active_keys.reserve( players.size( ) );
                    for ( const auto& player : players )
                        if ( player && player->m_mesh )
                            active_keys.insert( reinterpret_cast< std::uint64_t >( player->m_mesh ) );

                    for ( auto it = bar_state.begin( ); it != bar_state.end( ); )
                        it = active_keys.count( it->first ) ? std::next( it ) : bar_state.erase( it );
                    for ( auto it = fade_state.begin( ); it != fade_state.end( ); )
                        it = active_keys.count( it->first ) ? std::next( it ) : fade_state.erase( it );
                    for ( auto it = hitmarker_state.begin( ); it != hitmarker_state.end( ); )
                        it = active_keys.count( it->first ) ? std::next( it ) : hitmarker_state.erase( it );
                }

                const float now_s = std::chrono::duration<float>( t_now.time_since_epoch( ) ).count( );
                std::unordered_map<std::uint64_t, game::f_vector> player_head_positions;
                player_head_positions.reserve( players.size( ) );

                for ( const auto& player : players ) {
                    if ( !player || !player->m_mesh ) continue;

                    const auto player_key = reinterpret_cast< std::uint64_t >( player->m_mesh );
                    if ( !player_key ) continue;

                    const double distance = camera->location.distance_to( player->m_head );
                    const float raw_health = player->m_health;
                    const float raw_shield = player->m_shield;

                    player_head_positions[ player_key ] = player->m_head;

                    // hitmarker tracking
                    {
                        auto [hm_it, hm_inserted] = hitmarker_state.emplace( player_key, hitmarker_state_t{} );
                        auto& hm = hm_it->second;
                        if ( hm_inserted ) {
                            hm.last_health = raw_health;
                        }
                        else if ( hm.last_health > raw_health ) {
                            const float damage = hm.last_health - raw_health;
                            if ( damage > 0.5f && damage < 200.f )
                                active_hitmarkers.push_back( { damage, now_s, player_key } );
                        }
                        hm.last_health = raw_health;
                    }

                    // fade in/out
                    auto [fade_it, fade_inserted] = fade_state.emplace( player_key, fade_state_t{ 1.f } );
                    auto& fade = fade_it->second;
                    fade.alpha = player->m_alive
                        ? min( 1.f, fade.alpha + dt * 2.f )
                        : max( 0.f, fade.alpha - dt * 0.8f );

                    if ( fade.alpha <= 0.f ) continue;
                    const float fa = fade.alpha;

                    auto faded = [ & ]( kunai::color c ) -> kunai::color {
                        c.a *= fa;
                        return c;
                        };

                    // project head + root
                    const auto head_screen = game::world_to_screen( player->m_head, camera );
                    const auto root_screen = game::world_to_screen( player->m_chest, camera );
                    const bool head_on_screen = head_screen.x > 0.f && head_screen.y > 0.f;
                    const bool root_on_screen = root_screen.x > 0.f && root_screen.y > 0.f;

                    // bounding box from corners
                    const auto& bounds = player->m_bounds;
                    const game::f_vector corners[ 8 ] = {
                        { bounds.min.x, bounds.min.y, bounds.min.z },
                        { bounds.max.x, bounds.min.y, bounds.min.z },
                        { bounds.min.x, bounds.max.y, bounds.min.z },
                        { bounds.max.x, bounds.max.y, bounds.min.z },
                        { bounds.min.x, bounds.min.y, bounds.max.z },
                        { bounds.max.x, bounds.min.y, bounds.max.z },
                        { bounds.min.x, bounds.max.y, bounds.max.z },
                        { bounds.max.x, bounds.max.y, bounds.max.z },
                    };

                    float screen_min_x = FLT_MAX, screen_min_y = FLT_MAX;
                    float screen_max_x = -FLT_MAX, screen_max_y = -FLT_MAX;
                    int   visible_count = 0;

                    for ( const auto& corner : corners ) {
                        const float dot = ( corner.x - camera->location.x ) * forward_x
                            + ( corner.y - camera->location.y ) * forward_y
                            + ( corner.z - camera->location.z ) * forward_z;
                        if ( dot <= 0.f ) continue;

                        auto screen = game::world_to_screen( corner, camera );
                        if ( screen.x <= 0.f && screen.y <= 0.f ) continue;

                        screen_min_x = min( screen_min_x, ( float )screen.x );
                        screen_min_y = min( screen_min_y, ( float )screen.y );
                        screen_max_x = max( screen_max_x, ( float )screen.x );
                        screen_max_y = max( screen_max_y, ( float )screen.y );
                        ++visible_count;
                    }

                    if ( visible_count < 2 ) continue;

                    float box_w = screen_max_x - screen_min_x;
                    float box_h = screen_max_y - screen_min_y;
                    if ( box_w <= 0.f || box_h <= 0.f ) continue;
                    if ( box_w > 960.f || box_h > 810.f ) continue;

                    const float cx_box = ( screen_min_x + screen_max_x ) * 0.5f;
                    const float cy_box = ( screen_min_y + screen_max_y ) * 0.5f;
                    box_w *= 1.1f;
                    box_h *= 1.1f;

                    const float box_left = cx_box - box_w * 0.5f;
                    const float box_top = cy_box - box_h * 0.5f;

                    // smoothed health / shield bars
                    const float target_health = std::clamp( raw_health / player->m_max_health, 0.f, 1.f );
                    const float target_shield = player->m_max_shield > 0.f
                        ? std::clamp( raw_shield / player->m_max_shield, 0.f, 1.f )
                        : 0.f;

                    auto [bar_it, bar_inserted] = bar_state.emplace( player_key, smoothed_bars_t{ target_health, target_shield } );
                    auto& state = bar_it->second;
                    if ( !bar_inserted ) {
                        const float a = 1.f - std::exp( -bar_lerp_speed * dt );
                        state.health += ( target_health - state.health ) * a;
                        state.shield += ( target_shield - state.shield ) * a;
                    }

                    // skeleton
                    //{
                    //    const int skel_size = ( int )player->m_skeleton.size( );
                    //    for ( int idx = 0; idx + 1 < skel_size; idx += 2 ) {
                    //        auto sa = game::world_to_screen( player->m_skeleton[ idx ], camera );
                    //        auto sb = game::world_to_screen( player->m_skeleton[ idx + 1 ], camera );
                    //        if ( sa.x > 0.f && sa.y > 0.f && sb.x > 0.f && sb.y > 0.f )
                    //            tr.draw_line(
                    //                kunai::vec2{ ( float )sa.x, ( float )sa.y },
                    //                kunai::vec2{ ( float )sb.x, ( float )sb.y },
                    //                faded( utility::from_hex( 0x2596be ) ), 2.f );
                    //    }
                    //}

                    // box
                    const bool is_visible = player->m_mesh->was_recently_rendered( 0.06f );
                    const auto box_color = is_visible ? utility::from_hex( 0x7474B3 ) : utility::from_hex( 0x2596be );
                    const auto fill_color = is_visible
                        ? kunai::color( 0x74 / 255.f, 0x74 / 255.f, 0xB3 / 255.f, 0.15f )
                        : kunai::color( 0x25 / 255.f, 0x96 / 255.f, 0xBE / 255.f, 0.15f );

                    const kunai::rect box_rect{ box_left, box_top, box_w, box_h };
                    tr.draw_rect( box_rect, faded( fill_color ) );
                    tr.draw_rect_outline( box_rect, 1.5f, faded( box_color ) );

                    // view direction line
                    if ( player->m_alive && head_on_screen ) {
                        const auto direction = player->m_rotation.get_forward_vector( );
                        const auto view_world = player->m_head + ( direction * 180.0 );
                        const auto view_screen = game::world_to_screen( view_world, camera );
                        if ( view_screen.x > 0.f && view_screen.y > 0.f )
                            tr.draw_line(
                                kunai::vec2{ ( float )head_screen.x, ( float )head_screen.y },
                                kunai::vec2{ ( float )view_screen.x, ( float )view_screen.y },
                                faded( utility::from_hex( 0x2596be ) ), 2.f );
                    }

                    // name + distance
                    if ( !player->m_player_name.empty( ) ) {
                        constexpr float name_gap = 2.0f;
                        const std::string name_text = player->m_player_name + " " + std::format( "({:.1f}m)", distance );
                        const auto  name_sz = tr.measure_text( text_fnt, name_text.c_str( ) );
                        const float name_x = cx_box - name_sz.x * 0.5f;
                        const float name_y = box_top - name_sz.y - name_gap;

                        tr.draw_text( text_fnt, name_text.c_str( ), name_x - 1.f, name_y, faded( kunai::color( 0.f, 0.f, 0.f, 0.85f ) ) );
                        tr.draw_text( text_fnt, name_text.c_str( ), name_x + 1.f, name_y, faded( kunai::color( 0.f, 0.f, 0.f, 0.85f ) ) );
                        tr.draw_text( text_fnt, name_text.c_str( ), name_x, name_y - 1.f, faded( kunai::color( 0.f, 0.f, 0.f, 0.85f ) ) );
                        tr.draw_text( text_fnt, name_text.c_str( ), name_x, name_y + 1.f, faded( kunai::color( 0.f, 0.f, 0.f, 0.85f ) ) );
                        tr.draw_text( text_fnt, name_text.c_str( ), name_x, name_y, faded( utility::from_hex( 0xC4BCEE ) ) );
                    }

                    // shield bar (left side, vertical)
                    if ( raw_shield > 0.f && player->m_max_shield > 0.f ) {
                        constexpr float bar_width = 3.f;
                        constexpr float bar_gap = 3.f;
                        const float bar_x = box_left - bar_width - bar_gap;
                        const float filled_h = box_h * state.shield;
                        const float empty_h = box_h - filled_h;

                        tr.draw_rect( { bar_x, box_top, bar_width, box_h }, faded( kunai::color( 0.f, 0.f, 0.f, 0.6f ) ) );
                        if ( filled_h > 0.f )
                            tr.draw_rect_gradient_v(
                                { bar_x, box_top + empty_h, bar_width, filled_h },
                                faded( utility::from_hex( 0xBABAF8 ) ),
                                faded( utility::from_hex( 0x6B8EE8 ) ) );
                    }

                    // health bar (bottom, horizontal)
                    {
                        constexpr float bar_h = 3.f;
                        constexpr float bar_gap = 4.f;
                        const float bar_y = box_top + box_h + bar_gap;
                        const float filled_w = box_w * state.health;

                        tr.draw_rect( { box_left, bar_y, box_w, bar_h }, faded( kunai::color( 0.f, 0.f, 0.f, 0.6f ) ) );
                        if ( filled_w > 0.f )
                            tr.draw_rect_gradient_h(
                                { box_left, bar_y, filled_w, bar_h },
                                faded( utility::from_hex( 0x7FB77E ) ),
                                faded( utility::from_hex( 0xB3D8A8 ) ) );
                    }

                    // weapon name
                    if ( !player->m_weapon_name.empty( ) ) {
                        constexpr float bar_h = 3.f;
                        constexpr float bar_gap = 4.f;
                        constexpr float text_gap = 2.f;
                        const float text_y = box_top + box_h + bar_gap + bar_h + text_gap;
                        const auto  text_sz = tr.measure_text( text_fnt, player->m_weapon_name.c_str( ) );
                        const float text_x = cx_box - text_sz.x * 0.5f;

                        tr.draw_text( text_fnt, player->m_weapon_name.c_str( ), text_x - 1.f, text_y, faded( kunai::color( 0.f, 0.f, 0.f, 0.85f ) ) );
                        tr.draw_text( text_fnt, player->m_weapon_name.c_str( ), text_x + 1.f, text_y, faded( kunai::color( 0.f, 0.f, 0.f, 0.85f ) ) );
                        tr.draw_text( text_fnt, player->m_weapon_name.c_str( ), text_x, text_y - 1.f, faded( kunai::color( 0.f, 0.f, 0.f, 0.85f ) ) );
                        tr.draw_text( text_fnt, player->m_weapon_name.c_str( ), text_x, text_y + 1.f, faded( kunai::color( 0.f, 0.f, 0.f, 0.85f ) ) );
                        tr.draw_text( text_fnt, player->m_weapon_name.c_str( ), text_x, text_y, faded( utility::from_hex( 0x8494FF ) ) );
                    }

                    // candidate collection for aimbot
                    if ( player->m_alive && head_on_screen ) {
                        const double dx = head_screen.x - static_cast< double >( kunai::center_w );
                        const double dy = head_screen.y - static_cast< double >( kunai::center_h );
                        const double crosshair_dist = std::sqrt( dx * dx + dy * dy );

                        if ( crosshair_dist < static_cast< double >( fov_radius ) && candidate_count < m_max_candidates ) {
                            candidates[ candidate_count++ ] = {
                                player.get( ),
                                game::f_vector2d( head_screen.x, head_screen.y ),
                                game::f_vector2d( root_on_screen ? root_screen.x : head_screen.x,
                                                  root_on_screen ? root_screen.y : head_screen.y ),
                                distance,
                                is_visible
                            };
                        }
                    }
                }

                // hitmarkers
                for ( auto it = active_hitmarkers.begin( ); it != active_hitmarkers.end( ); ) {
                    const float age = now_s - it->time_created;
                    if ( age < 0.f || age > hitmarker_lifetime ) { it = active_hitmarkers.erase( it ); continue; }

                    const auto head_it = player_head_positions.find( it->player_key );
                    if ( head_it == player_head_positions.end( ) ) { it = active_hitmarkers.erase( it ); continue; }

                    const auto screen = game::world_to_screen( head_it->second, camera );
                    if ( screen.x <= 0.f || screen.y <= 0.f ) { ++it; continue; }

                    const float alpha = 1.f - ( age / hitmarker_lifetime );
                    const float y_offset = age * hitmarker_rise_speed;

                    const std::string text = "-" + std::to_string( static_cast< int >( it->damage + 0.5f ) );
                    const auto        text_sz = tr.measure_text( text_fnt, text );
                    const float       draw_x = ( float )screen.x - text_sz.x * 0.5f;
                    const float       draw_y = ( float )screen.y - y_offset;
                    const kunai::color outline( 0.f, 0.f, 0.f, alpha * 0.85f );

                    tr.draw_text( text_fnt, text, draw_x - 1.f, draw_y, outline );
                    tr.draw_text( text_fnt, text, draw_x + 1.f, draw_y, outline );
                    tr.draw_text( text_fnt, text, draw_x, draw_y - 1.f, outline );
                    tr.draw_text( text_fnt, text, draw_x, draw_y + 1.f, outline );
                    tr.draw_text( text_fnt, text, draw_x, draw_y, kunai::color( 1.f, 0.3f, 0.1f, alpha ) );
                    ++it;
                }

                // aimbot target selection + input
                {
                    cache::c_player* target_player = nullptr;
                    double           target_distance = 0.0;

                    if ( candidate_count > 0 ) {
                        double total_dist = 0.0;
                        for ( int i = 0; i < candidate_count; i++ )
                            total_dist += candidates[ i ].world_distance;

                        const double avg_dist = total_dist / static_cast< double >( candidate_count );
                        const double dist_high = avg_dist * 1.75;
                        double best_score = DBL_MAX;

                        for ( int i = 0; i < candidate_count; i++ ) {
                            const auto& c = candidates[ i ];
                            if ( !c.player ) continue;
                            if ( c.world_distance > dist_high ) continue;

                            const double cx_score = std::sqrt(
                                ( c.head_position.x - kunai::center_w ) * ( c.head_position.x - kunai::center_w ) +
                                ( c.head_position.y - kunai::center_h ) * ( c.head_position.y - kunai::center_h )
                            );
                            const double dist_factor = c.world_distance / dist_high;
                            double score = ( cx_score * 0.65 ) + ( dist_factor * 400.0 * 0.35 );
                            if ( c.is_visible ) score *= 0.15;

                            if ( score < best_score ) {
                                best_score = score;
                                target_distance = c.world_distance;
                                target_player = c.player;
                            }
                        }
                    }

                    const bool key_pressed = ( GetAsyncKeyState( VK_SHIFT ) & 0x8000 ) != 0;
                    const auto fov_color = key_pressed && target_player
                        ? kunai::color( 0x74 / 255.f, 0x74 / 255.f, 0xB3 / 255.f, 1.f )
                        : kunai::color( 0x25 / 255.f, 0x96 / 255.f, 0xBE / 255.f, 0.35f );

                    tr.draw_circle(
                        kunai::vec2{ ( float )kunai::center_w, ( float )kunai::center_h },
                        fov_radius, fov_color, 64, 1.5f );

                    if ( target_player && key_pressed ) {
                        if ( target_player->m_mesh && target_player->m_alive ) {
                            const auto head_screen = game::world_to_screen( target_player->m_head, camera );
                            if ( head_screen.x > 0.f && head_screen.y > 0.f ) {
                                // target dot
                                tr.draw_circle_filled(
                                    kunai::vec2{ ( float )head_screen.x, ( float )head_screen.y },
                                    5.f, kunai::color( 0.f, 0.f, 0.f, 0.9f ), 16 );
                                tr.draw_circle_filled(
                                    kunai::vec2{ ( float )head_screen.x, ( float )head_screen.y },
                                    3.f, kunai::color( 1.f, 0.2f, 0.2f, 1.f ), 16 );

                                const double dx = head_screen.x - kunai::center_w;
                                const double dy = head_screen.y - kunai::center_h;
                                const double screen_dist = std::sqrt( dx * dx + dy * dy );

                                if ( screen_dist > 2.0 ) {
                                    auto aim_rotation = game::find_look_at_rotation( camera->location, target_player->m_head );
                                    auto delta = aim_rotation - camera->rotation;
                                    delta.normalize( );

                                    // cubic bezier curve
                                    //constexpr double p1 = 0.0;
                                    //constexpr double p2 = 0.85;
                                    //const double     t = std::clamp( screen_dist / static_cast< double >( fov_radius ), 0.0, 1.0 );
                                    //const double inv_t = 1.0 - t;
                                    //const double curve = 3.0 * inv_t * inv_t * t * p1
                                    //    + 3.0 * inv_t * t * t * p2
                                    //    + t * t * t;

                                    //const double scale = 0.15 * curve;
                                    delta.pitch *= 0.15;
                                    delta.yaw *= 0.15;

                                    const auto controller = g_cache->get_player_controller( );
                                    if ( controller )
                                        controller->rotation_input( delta );
                                }
                            }
                        }
                    }
                }
            }

            tr.flush( );
            dev.end_frame( );
        }
    }
}