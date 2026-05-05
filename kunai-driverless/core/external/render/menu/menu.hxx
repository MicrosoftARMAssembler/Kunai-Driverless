#pragma once

namespace menu {
    struct aimbot_panel {
        kunai::ui::checkbox enabled;
        kunai::ui::slider fov;
        kunai::ui::checkbox smoothing;
        kunai::ui::slider smooth_factor;
        kunai::ui::checkbox target_visible;
        kunai::ui::combobox target_bone;
    };

    struct weapon_panel {
        kunai::ui::checkbox auto_fire;
        kunai::ui::slider rpm;
        kunai::ui::checkbox predict;
        kunai::ui::combobox priority;
    };

    struct esp_panel {
        kunai::ui::checkbox boxes;
        kunai::ui::checkbox names;
        kunai::ui::checkbox health;
        kunai::ui::slider max_distance;
        kunai::ui::checkbox team_check;
    };

    struct chams_panel {
        kunai::ui::checkbox enabled;
        kunai::ui::combobox material;
    };

    struct glow_panel {
        kunai::ui::checkbox enabled;
        kunai::ui::slider intensity;
    };

    struct chams_advanced {
        kunai::ui::checkbox ignore_smoke;
        kunai::ui::slider fade_distance;
        kunai::ui::combobox material_override;
    };

    struct aimbot_advanced {
        kunai::ui::checkbox auto_aim;
        kunai::ui::slider reaction_time;
        kunai::ui::combobox hit_group_priority;
    };

    struct misc_panel {
        kunai::ui::combobox ui_scale;
    };

    struct search_entry {
        const char* caption;
        int tab_index;
    };

    const search_entry search_index[ ] = {
        { "Aimbot",        0 },
        { "Visuals",       1 },
        { "Players",       2 },
        { "World",         3 },
        { "Misc",          4 },
        { "Enable",        0 },
        { "FOV",           0 },
        { "Smoothing",     0 },
        { "Smooth Factor", 0 },
        { "Visible Only",  0 },
        { "Target Bone",   0 },
        { "Auto Fire",     0 },
        { "RPM",           0 },
        { "Prediction",    0 },
        { "Priority",      0 },
        { "Boxes",         1 },
        { "Names",         1 },
        { "Health Bar",    1 },
        { "Max Distance",  1 },
        { "Team Check",    1 },
        { "Enabled",       1 },
        { "Material",      1 },
        { "Intensity",     1 },
    };

    constexpr int search_index_count = ( int )( sizeof( search_index ) / sizeof( search_index[ 0 ] ) );

    inline char ascii_lower( char c ) {
        if ( c >= 'A' && c <= 'Z' ) return ( char )( c - 'A' + 'a' );
        return c;
    }

    bool contains_ci( std::string_view hay, std::string_view needle ) {
        if ( needle.empty( ) ) return true;
        if ( needle.size( ) > hay.size( ) ) return false;
        for ( size_t i = 0; i + needle.size( ) <= hay.size( ); ++i ) {
            bool ok = true;
            for ( size_t j = 0; j < needle.size( ); ++j ) {
                if ( ascii_lower( hay[ i + j ] ) != ascii_lower( needle[ j ] ) ) {
                    ok = false;
                    break;
                }
            }
            if ( ok ) return true;
        }
        return false;
    }

    void search_dropdown(
        kunai::ui::context& ctx,
        const kunai::rect& search_rect,
        kunai::ui::text_edit& search_state,
        kunai::ui::tabs& tabs_state,
        const kunai::ui::tab_item* tab_items,
        bool was_focused ) {
        if ( !was_focused || search_state.text.empty( ) ) return;

        std::vector< int > matches;
        matches.reserve( 8 );
        for ( int i = 0; i < search_index_count && ( int )matches.size( ) < 8; ++i ) {
            if ( contains_ci( search_index[ i ].caption, search_state.text ) )
                matches.push_back( i );
        }

        if ( matches.empty( ) ) return;

        kunai::text_renderer& tr = *ctx.tr;
        kunai::window& w = *ctx.w;
        kunai::font& tf = *ctx.text_font;
        kunai::font& ifnt = *ctx.icon_font;

        const float item_h = 30.0f;
        const float panel_x = search_rect.x;
        const float panel_y = search_rect.y + search_rect.h + 4.0f;
        const float panel_w = search_rect.w;
        const float panel_h = item_h * ( float )matches.size( );

        float mx = ( float )w.mouse_x( );
        float my = ( float )w.mouse_y( );

        int hover = -1;
        for ( int i = 0; i < ( int )matches.size( ); ++i ) {
            float iy = panel_y + ( float )i * item_h;
            if ( mx >= panel_x && mx < panel_x + panel_w && my >= iy && my < iy + item_h ) {
                hover = i;
                break;
            }
        }

        tr.draw_rect( { panel_x, panel_y, panel_w, panel_h }, ctx.th.panel_bg );

        for ( int i = 0; i < ( int )matches.size( ); ++i ) {
            float iy = panel_y + ( float )i * item_h;
            const search_entry& e = search_index[ matches[ i ] ];

            if ( hover == i )
                tr.draw_rect( { panel_x + 1.0f, iy, panel_w - 2.0f, item_h }, ctx.th.item_hover_bg );

            kunai::color label_col = ( hover == i ) ? ctx.th.accent : ctx.th.text_dim;

            float icon_cy = iy + item_h * 0.5f;
            kunai::ui::draw_icon_centered( ctx, ifnt, tab_items[ e.tab_index ].icon, panel_x + 16.0f, icon_cy, label_col );

            float tyi = iy + ( item_h - tf.line_height( ) ) * 0.5f;
            tr.draw_text_clipped( tf, e.caption, panel_x + 32.0f, tyi, panel_x + panel_w - 74.0f, label_col, ctx.th.panel_bg, 8.0f );

            const char* tab_label = tab_items[ e.tab_index ].label;
            kunai::vec2 tab_sz = tr.measure_text( tf, tab_label );
            kunai::color tab_col = ctx.th.placeholder;
            if ( hover == i ) tab_col = ctx.th.text_dim;
            tr.draw_text( tf, tab_label, panel_x + panel_w - 10.0f - tab_sz.x, tyi, tab_col );
        }

        tr.draw_rect_outline( { panel_x, panel_y, panel_w, panel_h }, 1.0f, ctx.th.line );

        if ( hover >= 0 && w.mouse_left_clicked( ) ) {
            tabs_state.selected = search_index[ matches[ hover ] ].tab_index;
            search_state.text.clear( );
            search_state.cursor = 0;
            search_state.anchor = 0;
            search_state.focused = false;
            search_state.dragging = false;
        }
    }

	void render( kunai::window& win, kunai::device& dev, float dt ) {
        kunai::font text_fnt( dev, kunai::embedded::roboto_medium_ttf, kunai::embedded::roboto_medium_ttf_size, 15 );
        kunai::font icon_fnt( dev,
            kunai::embedded::font_awesome_solid_ttf, kunai::embedded::font_awesome_solid_ttf_size, 16,
            {
                { 0xF002, 0xF002 },
                { 0xF007, 0xF007 },
                { 0xF00C, 0xF00C },
                { 0xF013, 0xF013 },
                { 0xF053, 0xF053 },
                { 0xF054, 0xF054 },
                { 0xF05B, 0xF05B },
                { 0xF067, 0xF067 },
                { 0xF06E, 0xF06E },
                { 0xF078, 0xF078 },
                { 0xF07B, 0xF07B },
                { 0xF0B0, 0xF0B0 },
                { 0xF0C2, 0xF0C2 },
                { 0xF0C5, 0xF0C5 },
                { 0xF0C7, 0xF0C7 },
                { 0xF0EA, 0xF0EA },
                { 0xF185, 0xF185 },
                { 0xF1B2, 0xF1B2 },
                { 0xF1DE, 0xF1DE },
                { 0xF1FC, 0xF1FC },
            }
            );

        kunai::ui::theme th;
        th.child_bg = utility::from_hex( 0x0B0F1A );
        th.child_title_bg = utility::from_hex( 0x0E1322 );
        th.line = utility::from_hex( 0x1B1F2A );
        th.text_dim = utility::from_hex( 0xA5A9B3 );
        th.text = utility::from_hex( 0xFFFFFF );
        th.accent = utility::from_hex( 0x3B7BFB );
        th.accent_dim = utility::from_hex( 0x2D5FD0 );
        th.placeholder = utility::from_hex( 0x6B7280 );
        th.highlight_bg = utility::from_hex( 0x0E1322 );
        th.highlight_border = utility::from_hex( 0x2D5FD0 );
        th.panel_bg = utility::from_hex( 0x0B0F1A );
        th.item_hover_bg = utility::from_hex( 0x131826 );
        th.selection = utility::from_hex( 0x2D5FD0, 0.45f );
        th.track = utility::from_hex( 0x1B1F2A );


        kunai::image logo( dev, kunai::embedded::kunai_logo_png, kunai::embedded::kunai_logo_png_size );
        kunai::text_renderer tr( dev, text_fnt );

        kunai::ui::context ctx;
        ctx.w = &win;
        ctx.d = &dev;
        ctx.tr = &tr;
        ctx.text_font = &text_fnt;
        ctx.icon_font = &icon_fnt;
        ctx.th = th;
        ctx.dt = dt;

        constexpr float sidebar_x = 200.0f;
        constexpr float navbar_y = 60.0f;
        constexpr float line_thickness = 1.0f;

        const float logo_w = 154.0f;
        const float logo_h = 90.0f;
        const float logo_x = ( sidebar_x - logo_w ) * 0.5f;
        const float logo_y = 50.0f;

        const float nav_pad = 16.0f;
        const float ctrl_h = 36.0f;
        const float search_w = 360.0f;
        const float plus_size = 36.0f;
        const float ctrl_gap = 12.0f;
        const float ctrl_y = ( navbar_y - ctrl_h ) * 0.5f;
        const float search_x = sidebar_x + line_thickness + nav_pad;
        const float plus_y = ( navbar_y - plus_size ) * 0.5f;

        float config_width_cached = 160.0f;

        misc_panel misc = {};
        const char* ui_scale_options[ ] = { "80%", "100%", "125%", "150%", "200%" };
        const float ui_scale_values[ ] = { 0.80f, 1.00f, 1.25f, 1.50f, 2.00f };
        misc.ui_scale.options = ui_scale_options;
        misc.ui_scale.count = 5;
        misc.ui_scale.selected = 1;

        const kunai::ui::tab_item tab_items[ ] = {
            { "Aimbot",  0xF05B },
            { "Visuals", 0xF06E },
            { "Players", 0xF007 },
            { "World",   0xF1B2 },
            { "Misc",    0xF1DE },
        };
        kunai::ui::tabs tabs_state;
        tabs_state.items = tab_items;
        tabs_state.count = ( int )( sizeof( tab_items ) / sizeof( tab_items[ 0 ] ) );

        kunai::ui::config_selector config_state;
        config_state.configs = { "Default" };

        kunai::ui::text_edit search_state;

        aimbot_panel aim = {};
        aim.fov.value = 60.0f; aim.fov.min_val = 0.0f; aim.fov.max_val = 180.0f;
        aim.smoothing.value = true;
        aim.smooth_factor.value = 4.0f; aim.smooth_factor.min_val = 1.0f; aim.smooth_factor.max_val = 10.0f;
        aim.enabled.value = true;
        aim.target_visible.value = true;
        const char* bone_options[ ] = { "Head", "Neck", "Chest", "Nearest" };
        aim.target_bone.options = bone_options;
        aim.target_bone.count = 4;

        weapon_panel wpn = {};
        wpn.rpm.value = 600.0f; wpn.rpm.min_val = 0.0f; wpn.rpm.max_val = 1200.0f;
        const char* priority_options[ ] = { "Closest", "Lowest HP", "Visible First", "Manual" };
        wpn.priority.options = priority_options;
        wpn.priority.count = 4;

        esp_panel esp = {};
        esp.boxes.value = true; esp.names.value = true; esp.health.value = true;
        esp.max_distance.value = 400.0f; esp.max_distance.min_val = 0.0f; esp.max_distance.max_val = 1000.0f;

        chams_panel chams = {};
        const char* material_options[ ] = { "Flat", "Glow", "Wireframe", "Glass" };
        chams.material.options = material_options;
        chams.material.count = 4;

        glow_panel glow = {};
        glow.intensity.value = 0.6f;

        kunai::ui::color_picker esp_color = {};
        esp_color.value = utility::from_hex( 0x3B7BFB );

        chams_advanced chams_adv = {};
        const char* material_override_options[ ] = { "None", "Metal", "Plastic", "Energy" };
        chams_adv.material_override.options = material_override_options;
        chams_adv.material_override.count = 4;
        chams_adv.fade_distance.value = 120.0f;
        chams_adv.fade_distance.min_val = 0.0f;
        chams_adv.fade_distance.max_val = 500.0f;

        aimbot_advanced aim_adv = {};
        const char* hit_group_options[ ] = { "Head", "Chest", "Limbs", "Balanced" };
        aim_adv.hit_group_priority.options = hit_group_options;
        aim_adv.hit_group_priority.count = 4;
        aim_adv.reaction_time.value = 0.12f;
        aim_adv.reaction_time.min_val = 0.0f;
        aim_adv.reaction_time.max_val = 0.5f;

        kunai::ui::expander chams_exp;
        kunai::ui::expander aim_exp;
        kunai::rect chams_enable_row_rect = {};
        kunai::rect aim_enable_row_rect = {};

        kunai::ui::content_anim canim;
        tr.set_alpha( 1.0f );

        float scale = ctx.ui_scale;
        float win_w = ( float )dev.width( ) / scale;
        float win_h = ( float )dev.height( ) / scale;

        const float plus_x = win_w - nav_pad - plus_size;
        float config_x = plus_x - ctrl_gap - config_width_cached;
        float search_dd_h = 30.0f * 8.0f + 8.0f;

        win.set_drag_region( { ( sidebar_x + line_thickness ) * scale, 0.0f, 10000.0f, navbar_y * scale } );
        win.set_drag_exclusions( {
            { search_x * scale, ctrl_y * scale, search_w * scale, ctrl_h * scale },
            { search_x * scale, ( ctrl_y + ctrl_h ) * scale, search_w * scale, search_dd_h * scale },
            { config_x * scale, ctrl_y * scale, config_width_cached * scale, ctrl_h * scale },
            { plus_x * scale, plus_y * scale, plus_size * scale, plus_size * scale },
            } );

        tr.draw_rect( { sidebar_x, 0.0f, line_thickness, win_h }, th.line );
        tr.draw_rect( { sidebar_x + line_thickness, navbar_y, win_w - sidebar_x - line_thickness, line_thickness }, th.line );

        tr.draw_image( logo, { logo_x, logo_y, logo_w, logo_h } );

        kunai::ui::tabs_widget( ctx, 12.0f, 180.0f, sidebar_x - 24.0f, 48.0f, tabs_state );
        bool search_was_focused = search_state.focused;
        kunai::ui::search_box( ctx, { search_x, ctrl_y, search_w, ctrl_h }, search_state, "Search..." );
        kunai::ui::icon_button( ctx, { plus_x, plus_y, plus_size, plus_size }, 0xF067, th.accent, th.text );

        canim.update( tabs_state.selected, dt );
        float ay = canim.offset_y( );
        float aa = canim.alpha( );
        tr.set_alpha( aa );

        const float content_pad = 20.0f;
        const float content_gap = 12.0f;
        kunai::rect content = {
            sidebar_x + line_thickness + content_pad,
            navbar_y + content_pad + ay,
            win_w - sidebar_x - line_thickness - content_pad * 2.0f,
            win_h - navbar_y - content_pad * 2.0f - ay
        };

        {
            float mxf = ( float )win.mouse_x( );
            float myf = ( float )win.mouse_y( );
            auto in_r = [ & ]( const kunai::rect& rr ) {
                return rr.w > 0.0f && rr.h > 0.0f && mxf >= rr.x && mxf < rr.x + rr.w && myf >= rr.y && myf < rr.y + rr.h;
                };
            if ( aim_exp.open && in_r( aim_exp.popup_rect ) ) win.consume_click( );
            if ( chams_exp.open && in_r( chams_exp.popup_rect ) ) win.consume_click( );
            if ( esp_color.open && in_r( esp_color.popup_rect ) ) win.consume_click( );
        }

        auto fill_aimbot = [ & ]( const kunai::rect& left, const kunai::rect& right ) {
            kunai::rect c = kunai::ui::child( ctx, left, "Targeting", 0xF05B );
            aim_enable_row_rect = c;
            kunai::ui::checkbox_ctrl( ctx, kunai::ui::row_with_expander( ctx, c, "Enable", 24.0f, aim_exp ), aim.enabled );
            aim_enable_row_rect.h = 36.0f;
            kunai::ui::slider_ctrl( ctx, kunai::ui::row( ctx, c, "FOV", 160.0f ), aim.fov );
            kunai::ui::checkbox_ctrl( ctx, kunai::ui::row( ctx, c, "Smoothing", 24.0f ), aim.smoothing );
            kunai::ui::slider_ctrl( ctx, kunai::ui::row( ctx, c, "Smooth Factor", 160.0f ), aim.smooth_factor );
            kunai::ui::checkbox_ctrl( ctx, kunai::ui::row( ctx, c, "Visible Only", 24.0f ), aim.target_visible );
            kunai::ui::combobox_widget( ctx, kunai::ui::row( ctx, c, "Target Bone", 140.0f ), aim.target_bone );

            kunai::rect c2 = kunai::ui::child( ctx, right, "Weapon", 0xF1DE );
            kunai::ui::checkbox_ctrl( ctx, kunai::ui::row( ctx, c2, "Auto Fire", 24.0f ), wpn.auto_fire );
            kunai::ui::slider_ctrl( ctx, kunai::ui::row( ctx, c2, "RPM", 160.0f ), wpn.rpm );
            kunai::ui::checkbox_ctrl( ctx, kunai::ui::row( ctx, c2, "Prediction", 24.0f ), wpn.predict );
            kunai::ui::combobox_widget( ctx, kunai::ui::row( ctx, c2, "Priority", 140.0f ), wpn.priority );
            };

        auto fill_visuals = [ & ]( const kunai::rect& left, const kunai::rect& right_top, const kunai::rect& right_bot ) {
            kunai::rect c = kunai::ui::child( ctx, left, "ESP", 0xF06E );
            kunai::ui::checkbox_ctrl( ctx, kunai::ui::row( ctx, c, "Boxes", 24.0f ), esp.boxes );
            kunai::ui::checkbox_ctrl( ctx, kunai::ui::row_with_color_picker( ctx, c, "Box Color", 24.0f, esp_color ), esp.boxes );
            kunai::ui::checkbox_ctrl( ctx, kunai::ui::row( ctx, c, "Names", 24.0f ), esp.names );
            kunai::ui::checkbox_ctrl( ctx, kunai::ui::row( ctx, c, "Health Bar", 24.0f ), esp.health );
            kunai::ui::slider_ctrl( ctx, kunai::ui::row( ctx, c, "Max Distance", 180.0f ), esp.max_distance );
            kunai::ui::checkbox_ctrl( ctx, kunai::ui::row( ctx, c, "Team Check", 24.0f ), esp.team_check );

            kunai::rect c2 = kunai::ui::child( ctx, right_top, "Chams", 0xF1B2 );
            chams_enable_row_rect = c2;
            kunai::ui::checkbox_ctrl( ctx, kunai::ui::row_with_expander( ctx, c2, "Enabled", 24.0f, chams_exp ), chams.enabled );
            chams_enable_row_rect.h = 36.0f;
            kunai::ui::combobox_widget( ctx, kunai::ui::row( ctx, c2, "Material", 140.0f ), chams.material );

            kunai::rect c3 = kunai::ui::child( ctx, right_bot, "Glow", 0xF185 );
            kunai::ui::checkbox_ctrl( ctx, kunai::ui::row( ctx, c3, "Enabled", 24.0f ), glow.enabled );
            kunai::ui::slider_ctrl( ctx, kunai::ui::row( ctx, c3, "Intensity", 180.0f ), glow.intensity );
            };

        if ( tabs_state.selected == 0 ) {
            kunai::rect left = kunai::ui::split_h_left( content, 0.5f, content_gap );
            kunai::rect right = kunai::ui::split_h_right( content, 0.5f, content_gap );
            fill_aimbot( left, right );
        }
        else if ( tabs_state.selected == 1 ) {
            kunai::rect left = kunai::ui::split_h_left( content, 0.5f, content_gap );
            kunai::rect right = kunai::ui::split_h_right( content, 0.5f, content_gap );
            kunai::rect right_top = kunai::ui::split_v_top( right, 0.5f, content_gap );
            kunai::rect right_bot = kunai::ui::split_v_bottom( right, 0.5f, content_gap );
            fill_visuals( left, right_top, right_bot );
        }
        else if ( tabs_state.selected == 2 ) {
            kunai::rect c = kunai::ui::child( ctx, content, "Players", 0xF007 );
            ( void )c;
        }
        else if ( tabs_state.selected == 4 ) {
            kunai::rect left = kunai::ui::split_h_left( content, 0.5f, content_gap );
            kunai::rect c = kunai::ui::child( ctx, left, "Display", 0xF013 );
            int prev_scale_sel = misc.ui_scale.selected;
            kunai::ui::combobox_widget( ctx, kunai::ui::row( ctx, c, "UI Scale", 140.0f ), misc.ui_scale );
            if ( misc.ui_scale.selected != prev_scale_sel ) {
                int idx = misc.ui_scale.selected;
                if ( idx < 0 ) idx = 0;
                if ( idx > 4 ) idx = 4;
                float new_scale = ui_scale_values[ idx ];
                ctx.ui_scale = new_scale;
                win.set_ui_scale( new_scale );
                tr.set_ui_scale( new_scale );
            }
        }
        else {
            kunai::rect c = kunai::ui::child( ctx, content, tab_items[ tabs_state.selected ].label, tab_items[ tabs_state.selected ].icon );
            ( void )c;
        }

        tr.set_alpha( 1.0f );

        kunai::ui::config_result cr = kunai::ui::config_selector_widget( ctx, config_x, ctrl_y, ctrl_h, config_state );
        config_width_cached = cr.rendered_width;

        if ( cr.action == kunai::ui::config_action::new_config ) {
            static int counter = 1;
            std::string name = "New Config " + std::to_string( counter++ );
            config_state.configs.push_back( name );
            config_state.selected = ( int )config_state.configs.size( ) - 1;
        }

        if ( tabs_state.selected == 0 && ( aim_exp.open || aim_exp.anim > 0.002f ) ) {
            kunai::rect ab = kunai::ui::expander_popup_begin( ctx, aim_enable_row_rect, "Aimbot Advanced", 0xF05B, aim_exp );
            kunai::rect ab_cursor = ab;
            kunai::ui::checkbox_ctrl( ctx, kunai::ui::row( ctx, ab_cursor, "Auto Aim", 24.0f ), aim_adv.auto_aim );
            kunai::ui::slider_ctrl( ctx, kunai::ui::row( ctx, ab_cursor, "Reaction Time", 140.0f ), aim_adv.reaction_time );
            kunai::ui::combobox_widget( ctx, kunai::ui::row( ctx, ab_cursor, "Hit Group Priority", 140.0f ), aim_adv.hit_group_priority );
            kunai::ui::expander_popup_end( ctx, aim_exp );
        }

        if ( tabs_state.selected == 1 && ( chams_exp.open || chams_exp.anim > 0.002f ) ) {
            kunai::rect cb = kunai::ui::expander_popup_begin( ctx, chams_enable_row_rect, "Chams Advanced", 0xF1B2, chams_exp );
            kunai::rect cb_cursor = cb;
            kunai::ui::checkbox_ctrl( ctx, kunai::ui::row( ctx, cb_cursor, "Ignore Smoke", 24.0f ), chams_adv.ignore_smoke );
            kunai::ui::slider_ctrl( ctx, kunai::ui::row( ctx, cb_cursor, "Fade Distance", 140.0f ), chams_adv.fade_distance );
            kunai::ui::combobox_widget( ctx, kunai::ui::row( ctx, cb_cursor, "Material Override", 140.0f ), chams_adv.material_override );
            kunai::ui::expander_popup_end( ctx, chams_exp );
        }

        if ( tabs_state.selected == 1 && ( esp_color.open || esp_color.anim > 0.002f ) ) {
            kunai::ui::color_picker_popup_begin( ctx, esp_color );
            kunai::ui::color_picker_popup_end( ctx, esp_color );
        }

        search_dropdown( ctx, { search_x, ctrl_y, search_w, ctrl_h }, search_state, tabs_state, tab_items, search_was_focused );
        tr.flush( );
	}
}