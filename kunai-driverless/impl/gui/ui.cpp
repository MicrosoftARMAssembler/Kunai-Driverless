#include "ui.h"
#include "window.h"
#include "device.h"
#include "font.h"
#include "image.h"
#include "text_renderer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace kunai::ui
{
    static inline bool point_in_rect( float px, float py, float rx, float ry, float rw, float rh )
    {
        return px >= rx && px < rx + rw && py >= ry && py < ry + rh;
    }

    static inline float clamp01( float v ) { return v < 0.0f ? 0.0f : ( v > 1.0f ? 1.0f : v ); }
    static inline float lerp( float a, float b, float t ) { return a + ( b - a ) * t; }
    static inline color lerp_color( const color& a, const color& b, float t )
    {
        return { lerp( a.r, b.r, t ), lerp( a.g, b.g, t ), lerp( a.b, b.b, t ), lerp( a.a, b.a, t ) };
    }

    void draw_icon_centered( context& ctx, font& f, u32 cp, float cx, float cy, const color& c )
    {
        const glyph* g = f.find( cp );
        if ( !g ) return;
        float x = cx - g->width * 0.5f - g->bearing_x;
        float y = cy - g->height * 0.5f - f.ascent( ) + g->bearing_y;
        ctx.tr->draw_codepoint( f, cp, x, y, c );
    }

    rect split_h_left( const rect& r, float ratio, float gap )
    {
        float w = ( r.w - gap ) * ratio;
        return { r.x, r.y, w, r.h };
    }

    rect split_h_right( const rect& r, float ratio, float gap )
    {
        float left_w = ( r.w - gap ) * ratio;
        float right_w = r.w - gap - left_w;
        return { r.x + left_w + gap, r.y, right_w, r.h };
    }

    rect split_v_top( const rect& r, float ratio, float gap )
    {
        float h = ( r.h - gap ) * ratio;
        return { r.x, r.y, r.w, h };
    }

    rect split_v_bottom( const rect& r, float ratio, float gap )
    {
        float top_h = ( r.h - gap ) * ratio;
        float bot_h = r.h - gap - top_h;
        return { r.x, r.y + top_h + gap, r.w, bot_h };
    }

    rect child( context& ctx, const rect& area, const char* title, u32 icon )
    {
        text_renderer& tr = *ctx.tr;
        font& tf = *ctx.text_font;
        font& ifnt = *ctx.icon_font;

        const float title_h = 34.0f;
        const float inner_pad_x = 14.0f;
        const float inner_pad_y = 10.0f;

        tr.draw_rect( area, ctx.th.child_bg );
        tr.draw_rect( { area.x, area.y, area.w, title_h }, ctx.th.child_title_bg );
        tr.draw_rect( { area.x, area.y + title_h, area.w, 1.0f }, ctx.th.line );
        tr.draw_rect_outline( area, 1.0f, ctx.th.line );

        float title_cy = area.y + title_h * 0.5f;
        float icon_x = area.x + 14.0f;
        if ( icon != 0 )
        {
            draw_icon_centered( ctx, ifnt, icon, icon_x + 8.0f, title_cy, ctx.th.accent );
            icon_x += 24.0f;
        }

        float text_y = area.y + ( title_h - tf.line_height( ) ) * 0.5f;
        tr.draw_text( tf, title, icon_x + 2.0f, text_y, ctx.th.text );

        return {
            area.x + inner_pad_x,
            area.y + title_h + inner_pad_y,
            area.w - inner_pad_x * 2.0f,
            area.h - title_h - inner_pad_y * 2.0f
        };
    }

    rect row( context& ctx, rect& cursor, const char* caption, float control_w, float row_h, float control_h )
    {
        text_renderer& tr = *ctx.tr;
        font& tf = *ctx.text_font;

        float text_y = cursor.y + ( row_h - tf.line_height( ) ) * 0.5f;
        float control_x = cursor.x + cursor.w - control_w;
        float caption_clip = control_x - 10.0f;
        tr.draw_text_clipped( tf, caption, cursor.x + 2.0f, text_y, caption_clip, ctx.th.text_dim, ctx.th.child_bg, 12.0f );

        rect ctrl = { control_x, cursor.y + ( row_h - control_h ) * 0.5f, control_w, control_h };
        cursor.y += row_h;
        return ctrl;
    }

    bool checkbox_ctrl( context& ctx, const rect& r, checkbox& state )
    {
        window& w = *ctx.w;
        text_renderer& tr = *ctx.tr;
        font& ifnt = *ctx.icon_font;

        float mx = ( float ) w.mouse_x( );
        float my = ( float ) w.mouse_y( );

        const float box_size = 18.0f;
        rect box = { r.x + r.w - box_size, r.y + ( r.h - box_size ) * 0.5f, box_size, box_size };

        bool in_rect = point_in_rect( mx, my, box.x, box.y, box.w, box.h );
        bool changed = false;
        if ( in_rect && w.mouse_left_clicked( ) )
        {
            state.value = !state.value;
            changed = true;
        }

        float target = state.value ? 1.0f : 0.0f;
        state.anim += ( target - state.anim ) * ( 1.0f - std::exp( -20.0f * ctx.dt ) );

        color fill = { ctx.th.accent.r, ctx.th.accent.g, ctx.th.accent.b, state.anim };
        tr.draw_rect( box, fill );

        color border_off = ctx.th.line;
        if ( in_rect && state.anim < 0.5f ) border_off = lerp_color( ctx.th.line, ctx.th.text_dim, 0.8f );
        color border = lerp_color( border_off, ctx.th.accent, state.anim );
        tr.draw_rect_outline( box, 1.0f, border );

        if ( state.anim > 0.15f )
        {
            float a = ( state.anim - 0.15f ) / 0.85f;
            if ( a > 1.0f ) a = 1.0f;
            color check_c = { 1.0f, 1.0f, 1.0f, a };
            draw_icon_centered( ctx, ifnt, 0xF00C, box.x + box.w * 0.5f, box.y + box.h * 0.5f, check_c );
        }

        return changed;
    }

    bool slider_ctrl( context& ctx, const rect& r, slider& state )
    {
        window& w = *ctx.w;
        text_renderer& tr = *ctx.tr;
        font& tf = *ctx.text_font;

        float mx = ( float ) w.mouse_x( );
        float my = ( float ) w.mouse_y( );
        bool in_rect = point_in_rect( mx, my, r.x, r.y, r.w, r.h );

        bool changed = false;

        if ( in_rect && w.mouse_left_clicked( ) )
            state.dragging = true;
        if ( !w.mouse_left_pressed( ) )
            state.dragging = false;

        if ( state.dragging )
        {
            float t = clamp01( ( mx - r.x ) / r.w );
            float new_value = state.min_val + t * ( state.max_val - state.min_val );
            if ( new_value != state.value )
            {
                state.value = new_value;
                changed = true;
            }
        }

        float hover_target = ( in_rect || state.dragging ) ? 1.0f : 0.0f;
        state.hover_anim += ( hover_target - state.hover_anim ) * ( 1.0f - std::exp( -14.0f * ctx.dt ) );

        float t = clamp01( ( state.value - state.min_val ) / ( state.max_val - state.min_val ) );

        float track_h = 4.0f;
        float track_y = r.y + ( r.h - track_h ) * 0.5f;
        float fill_w = r.w * t;

        tr.draw_rect( { r.x, track_y, r.w, track_h }, ctx.th.track );
        tr.draw_rect( { r.x, track_y, fill_w, track_h }, ctx.th.accent );

        float handle_r = 7.0f + state.hover_anim * 1.5f;
        float handle_x = r.x + fill_w - handle_r;
        float handle_y = r.y + ( r.h - handle_r * 2.0f ) * 0.5f;
        tr.draw_rect( { handle_x, handle_y, handle_r * 2.0f, handle_r * 2.0f }, ctx.th.text );
        tr.draw_rect_outline( { handle_x, handle_y, handle_r * 2.0f, handle_r * 2.0f }, 1.0f, ctx.th.accent );

        if ( state.hover_anim > 0.05f )
        {
            char buf[ 32 ];
            snprintf( buf, sizeof( buf ), "%.2f", state.value );
            kunai::vec2 sz = tr.measure_text( tf, buf );
            float label_x = r.x + fill_w - sz.x * 0.5f;
            if ( label_x < r.x ) label_x = r.x;
            if ( label_x + sz.x > r.x + r.w ) label_x = r.x + r.w - sz.x;
            float label_y = r.y - tf.line_height( ) - 2.0f;
            color c = ctx.th.text;
            c.a *= state.hover_anim;
            tr.draw_text( tf, buf, label_x, label_y, c );
        }

        return changed;
    }

    void text_edit::delete_selection( )
    {
        if ( !has_selection( ) ) return;
        int s = sel_start( );
        int e = sel_end( );
        text.erase( ( size_t ) s, ( size_t ) ( e - s ) );
        cursor = anchor = s;
        cursor_blink = 0.0f;
    }

    void text_edit::insert_text( std::string_view s )
    {
        if ( has_selection( ) ) delete_selection( );
        size_t room = max_length > text.size( ) ? max_length - text.size( ) : 0;
        size_t n = s.size( ) > room ? room : s.size( );
        text.insert( ( size_t ) cursor, s.data( ), n );
        cursor += ( int ) n;
        anchor = cursor;
        cursor_blink = 0.0f;
    }

    void text_edit::select_all( )
    {
        anchor = 0;
        cursor = ( int ) text.size( );
        cursor_blink = 0.0f;
    }

    bool search_box( context& ctx, const rect& r, text_edit& state, const char* placeholder )
    {
        window& w = *ctx.w;
        text_renderer& tr = *ctx.tr;
        font& tf = *ctx.text_font;
        font& ifnt = *ctx.icon_font;

        const float text_inset_x = r.x + 38.0f;
        const float text_clip_x = r.x + r.w - 10.0f;

        float mx = ( float ) w.mouse_x( );
        float my = ( float ) w.mouse_y( );
        bool in_rect = point_in_rect( mx, my, r.x, r.y, r.w, r.h );

        if ( w.mouse_left_clicked( ) )
        {
            if ( in_rect )
            {
                state.focused = true;
                state.dragging = true;
                state.cursor_blink = 0.0f;
                int idx = tr.index_from_x( tf, state.text, mx - text_inset_x );
                state.cursor = idx;
                state.anchor = idx;
            }
            else
            {
                state.focused = false;
                state.dragging = false;
            }
        }

        if ( state.focused && state.dragging && w.mouse_left_pressed( ) )
            state.cursor = tr.index_from_x( tf, state.text, mx - text_inset_x );
        if ( !w.mouse_left_pressed( ) ) state.dragging = false;

        bool changed = false;

        if ( state.focused )
        {
            for ( char c : w.text_input( ) )
            {
                unsigned char uc = ( unsigned char ) c;
                if ( c == '\b' )
                {
                    if ( state.has_selection( ) )
                    {
                        state.delete_selection( );
                        changed = true;
                    }
                    else if ( state.cursor > 0 )
                    {
                        state.text.erase( ( size_t ) ( state.cursor - 1 ), 1 );
                        state.cursor--;
                        state.anchor = state.cursor;
                        state.cursor_blink = 0.0f;
                        changed = true;
                    }
                }
                else if ( c == '\r' || c == '\n' )
                {
                    state.focused = false;
                }
                else if ( uc >= 32 && uc < 127 )
                {
                    char buf[ 2 ] = { c, 0 };
                    state.insert_text( std::string_view( buf, 1 ) );
                    changed = true;
                }
            }

            for ( const key_event& ke : w.key_events( ) )
            {
                if ( ke.vk == VK_LEFT )
                {
                    int nc = state.cursor;
                    if ( ke.shift ) nc = ( std::max )( 0, state.cursor - 1 );
                    else
                    {
                        if ( state.has_selection( ) ) nc = state.sel_start( );
                        else nc = ( std::max )( 0, state.cursor - 1 );
                    }
                    state.cursor = nc;
                    if ( !ke.shift ) state.anchor = state.cursor;
                    state.cursor_blink = 0.0f;
                }
                else if ( ke.vk == VK_RIGHT )
                {
                    int nc = state.cursor;
                    if ( ke.shift ) nc = ( std::min )( ( int ) state.text.size( ), state.cursor + 1 );
                    else
                    {
                        if ( state.has_selection( ) ) nc = state.sel_end( );
                        else nc = ( std::min )( ( int ) state.text.size( ), state.cursor + 1 );
                    }
                    state.cursor = nc;
                    if ( !ke.shift ) state.anchor = state.cursor;
                    state.cursor_blink = 0.0f;
                }
                else if ( ke.vk == VK_HOME )
                {
                    state.cursor = 0;
                    if ( !ke.shift ) state.anchor = state.cursor;
                    state.cursor_blink = 0.0f;
                }
                else if ( ke.vk == VK_END )
                {
                    state.cursor = ( int ) state.text.size( );
                    if ( !ke.shift ) state.anchor = state.cursor;
                    state.cursor_blink = 0.0f;
                }
                else if ( ke.vk == VK_DELETE )
                {
                    if ( state.has_selection( ) )
                    {
                        state.delete_selection( );
                        changed = true;
                    }
                    else if ( state.cursor < ( int ) state.text.size( ) )
                    {
                        state.text.erase( ( size_t ) state.cursor, 1 );
                        state.cursor_blink = 0.0f;
                        changed = true;
                    }
                }
                else if ( ke.ctrl && ke.vk == 'A' ) state.select_all( );
                else if ( ke.ctrl && ke.vk == 'C' )
                {
                    if ( state.has_selection( ) )
                        w.set_clipboard_text( std::string_view( state.text.data( ) + state.sel_start( ), ( size_t ) ( state.sel_end( ) - state.sel_start( ) ) ) );
                }
                else if ( ke.ctrl && ke.vk == 'X' )
                {
                    if ( state.has_selection( ) )
                    {
                        w.set_clipboard_text( std::string_view( state.text.data( ) + state.sel_start( ), ( size_t ) ( state.sel_end( ) - state.sel_start( ) ) ) );
                        state.delete_selection( );
                        changed = true;
                    }
                }
                else if ( ke.ctrl && ke.vk == 'V' )
                {
                    std::string paste = w.get_clipboard_text( );
                    if ( !paste.empty( ) )
                    {
                        std::string filtered;
                        filtered.reserve( paste.size( ) );
                        for ( char c : paste )
                        {
                            unsigned char uc = ( unsigned char ) c;
                            if ( uc >= 32 && uc < 127 ) filtered += c;
                        }
                        if ( !filtered.empty( ) )
                        {
                            state.insert_text( filtered );
                            changed = true;
                        }
                    }
                }
            }

            state.cursor_blink += ctx.dt;
        }

        color border = state.focused ? ctx.th.accent : ctx.th.line;
        tr.draw_rect_outline( r, 1.0f, border );

        float icon_col_y = r.y + r.h * 0.5f;
        color icon_col = state.focused ? ctx.th.text_dim : ctx.th.placeholder;
        draw_icon_centered( ctx, ifnt, 0xF002, r.x + 18.0f, icon_col_y, icon_col );

        float text_y = r.y + ( r.h - tf.line_height( ) ) * 0.5f;

        if ( state.has_selection( ) && state.focused )
        {
            float sx0 = text_inset_x + tr.advance_to_index( tf, state.text, state.sel_start( ) );
            float sx1 = text_inset_x + tr.advance_to_index( tf, state.text, state.sel_end( ) );
            tr.draw_rect( { sx0, r.y + 6.0f, sx1 - sx0, r.h - 12.0f }, ctx.th.selection );
        }

        if ( !state.text.empty( ) )
            tr.draw_text_clipped( tf, state.text, text_inset_x, text_y, text_clip_x, ctx.th.text, ctx.th.bg, 12.0f );
        else if ( placeholder )
            tr.draw_text( tf, placeholder, text_inset_x, text_y, ctx.th.placeholder );

        if ( state.focused )
        {
            bool blink_on = std::fmod( state.cursor_blink, 1.0f ) < 0.5f;
            if ( blink_on )
            {
                float cx = text_inset_x + tr.advance_to_index( tf, state.text, state.cursor );
                if ( cx < text_clip_x )
                    tr.draw_rect( { cx, r.y + 8.0f, 1.5f, r.h - 16.0f }, ctx.th.text );
            }
        }

        return changed;
    }

    bool combobox_widget( context& ctx, const rect& r, combobox& state )
    {
        window& w = *ctx.w;
        text_renderer& tr = *ctx.tr;
        font& tf = *ctx.text_font;
        font& ifnt = *ctx.icon_font;

        const float fixed_h = 36.0f;
        rect vr_outer = r;
        if ( vr_outer.h < fixed_h )
        {
            vr_outer.y = r.y + ( r.h - fixed_h ) * 0.5f;
            vr_outer.h = fixed_h;
        }

        const char* label = ( state.selected >= 0 && state.selected < state.count ) ? state.options[ state.selected ] : "";

        const float pad_left = 14.0f;
        const float pad_right = 14.0f;
        const float chevron_w = 12.0f;
        const float gap_text_chevron = 8.0f;
        const float min_w = 70.0f;

        float label_w = tr.measure_text( tf, label ).x;
        float target_btn_w = pad_left + label_w + gap_text_chevron + chevron_w + pad_right;
        if ( target_btn_w < min_w ) target_btn_w = min_w;
        if ( target_btn_w > vr_outer.w ) target_btn_w = vr_outer.w;

        if ( state.width_anim <= 0.0f ) state.width_anim = target_btn_w;
        state.width_anim += ( target_btn_w - state.width_anim ) * ( 1.0f - std::exp( -14.0f * ctx.dt ) );

        rect vr = { vr_outer.x + vr_outer.w - state.width_anim, vr_outer.y, state.width_anim, vr_outer.h };

        float max_opt_w = label_w;
        for ( int i = 0; i < state.count; ++i )
        {
            float ow = tr.measure_text( tf, state.options[ i ] ).x;
            if ( ow > max_opt_w ) max_opt_w = ow;
        }
        float panel_w = max_opt_w + 24.0f;
        if ( panel_w < state.width_anim ) panel_w = state.width_anim;
        float panel_x = vr.x + vr.w - panel_w;
        if ( panel_x < 2.0f ) panel_x = 2.0f;

        float mx = ( float ) w.mouse_x( );
        float my = ( float ) w.mouse_y( );
        bool in_button = point_in_rect( mx, my, vr.x, vr.y, vr.w, vr.h );

        const float item_h = 30.0f;
        const float panel_y = vr.y + vr.h + 4.0f;

        state.hover_item = -1;
        if ( state.anim > 0.4f )
        {
            for ( int i = 0; i < state.count; ++i )
            {
                float iy = panel_y + ( float ) i * item_h;
                if ( point_in_rect( mx, my, panel_x, iy, panel_w, item_h ) )
                {
                    state.hover_item = i;
                    break;
                }
            }
        }

        bool changed = false;
        if ( w.mouse_left_clicked( ) )
        {
            if ( in_button )
            {
                state.open = !state.open;
            }
            else if ( state.hover_item >= 0 )
            {
                if ( state.selected != state.hover_item )
                {
                    state.selected = state.hover_item;
                    changed = true;
                }
                state.open = false;
            }
            else
            {
                state.open = false;
            }
        }

        float target = state.open ? 1.0f : 0.0f;
        state.anim += ( target - state.anim ) * ( 1.0f - std::exp( -18.0f * ctx.dt ) );

        const float label_inset_x = vr.x + pad_left;
        const float label_clip_x = vr.x + vr.w - pad_right - chevron_w + 2.0f;

        tr.draw_rect( vr, ctx.th.bg );
        color dd_border = state.open ? ctx.th.accent : ctx.th.line;
        tr.draw_rect_outline( vr, 1.0f, dd_border );

        float text_y = vr.y + ( vr.h - tf.line_height( ) ) * 0.5f;
        tr.draw_text_clipped( tf, label, label_inset_x, text_y, label_clip_x, ctx.th.text_dim, ctx.th.bg, 8.0f );
        draw_icon_centered( ctx, ifnt, 0xF078, vr.x + vr.w - pad_right, vr.y + vr.h * 0.5f, ctx.th.text_dim );

        if ( state.anim > 0.002f )
        {
            float full_h = item_h * ( float ) state.count;
            float panel_h = full_h * state.anim;

            tr.draw_rect( { panel_x, panel_y, panel_w, panel_h }, ctx.th.panel_bg );

            for ( int i = 0; i < state.count; ++i )
            {
                float iy = panel_y + ( float ) i * item_h;
                float visible = item_h * state.anim * ( float ) state.count - ( float ) i * item_h;
                float reveal = visible / item_h;
                if ( reveal > 1.0f ) reveal = 1.0f;
                if ( reveal <= 0.01f ) continue;

                bool is_hover = ( state.hover_item == i );
                if ( is_hover )
                {
                    color hb = ctx.th.item_hover_bg;
                    hb.a *= reveal;
                    tr.draw_rect( { panel_x + 1.0f, iy, panel_w - 2.0f, item_h }, hb );
                }

                color col = ( i == state.selected ) ? ctx.th.accent : ctx.th.text_dim;
                col.a *= reveal;

                const char* opt = state.options[ i ];
                float item_clip_x = panel_x + panel_w - 14.0f;
                float tyi = iy + ( item_h - tf.line_height( ) ) * 0.5f;
                tr.draw_text_clipped( tf, opt, panel_x + 14.0f, tyi, item_clip_x, col, ctx.th.panel_bg, 10.0f );
            }

            tr.draw_rect_outline( { panel_x, panel_y, panel_w, panel_h }, 1.0f, ctx.th.line );
        }

        return changed;
    }

    int tabs_widget( context& ctx, float x, float y, float w, float item_h, tabs& state )
    {
        window& win = *ctx.w;
        text_renderer& tr = *ctx.tr;
        font& tf = *ctx.text_font;
        font& ifnt = *ctx.icon_font;

        float mx = ( float ) win.mouse_x( );
        float my = ( float ) win.mouse_y( );

        int hovered = -1;
        for ( int i = 0; i < state.count; ++i )
        {
            float ty = y + ( float ) i * item_h;
            if ( point_in_rect( mx, my, x, ty, w, item_h ) )
                hovered = i;
        }

        if ( hovered >= 0 && win.mouse_left_clicked( ) )
            state.selected = hovered;

        float target_y = y + ( float ) state.selected * item_h;
        state.highlight_y += ( target_y - state.highlight_y ) * ( 1.0f - std::exp( -15.0f * ctx.dt ) );

        rect hl = { x, state.highlight_y + 3.0f, w, item_h - 6.0f };
        tr.draw_rect( hl, ctx.th.highlight_bg );
        tr.draw_rect_outline( hl, 1.0f, ctx.th.highlight_border );

        for ( int i = 0; i < state.count; ++i )
        {
            float ty = y + ( float ) i * item_h;
            color col = ( i == state.selected ) ? ctx.th.accent : ctx.th.text_dim;

            float icon_y = ty + ( item_h - ifnt.line_height( ) ) * 0.5f;
            tr.draw_codepoint( ifnt, state.items[ i ].icon, x + 16.0f, icon_y, col );

            float label_y = ty + ( item_h - tf.line_height( ) ) * 0.5f;
            tr.draw_text( tf, state.items[ i ].label, x + 44.0f, label_y, col );
        }

        return state.selected;
    }

    bool icon_button( context& ctx, const rect& r, u32 icon, const color& bg, const color& icon_col )
    {
        window& w = *ctx.w;
        float mx = ( float ) w.mouse_x( );
        float my = ( float ) w.mouse_y( );
        bool in_rect = point_in_rect( mx, my, r.x, r.y, r.w, r.h );

        color use_bg = bg;
        if ( in_rect )
        {
            use_bg.r = ( std::min )( 1.0f, bg.r * 1.15f );
            use_bg.g = ( std::min )( 1.0f, bg.g * 1.15f );
            use_bg.b = ( std::min )( 1.0f, bg.b * 1.15f );
        }

        ctx.tr->draw_rect( r, use_bg );
        draw_icon_centered( ctx, *ctx.icon_font, icon, r.x + r.w * 0.5f, r.y + r.h * 0.5f, icon_col );

        return in_rect && w.mouse_left_clicked( );
    }

    void content_anim::update( int new_tab, float dt )
    {
        if ( new_tab != current )
        {
            current = new_tab;
            progress = 0.0f;
        }
        progress += ( 1.0f - progress ) * ( 1.0f - std::exp( -11.0f * dt ) );
        if ( progress > 0.999f ) progress = 1.0f;
    }

    rect row_with_expander( context& ctx, rect& cursor, const char* caption, float control_w, expander& exp, float row_h, float control_h )
    {
        window& w = *ctx.w;
        text_renderer& tr = *ctx.tr;
        font& tf = *ctx.text_font;
        font& ifnt = *ctx.icon_font;

        float text_y = cursor.y + ( row_h - tf.line_height( ) ) * 0.5f;
        float control_x = cursor.x + cursor.w - control_w;

        const float knob_size = 22.0f;
        const float knob_offset = 24.0f;
        float knob_cx = control_x - knob_offset;
        float knob_cy = cursor.y + row_h * 0.5f;
        rect knob_rect = { knob_cx - knob_size * 0.5f, knob_cy - knob_size * 0.5f, knob_size, knob_size };
        exp.knob_rect = knob_rect;

        float caption_clip = knob_rect.x - 6.0f;
        tr.draw_text_clipped( tf, caption, cursor.x + 2.0f, text_y, caption_clip, ctx.th.text_dim, ctx.th.child_bg, 12.0f );

        float mx = ( float ) w.mouse_x( );
        float my = ( float ) w.mouse_y( );
        bool in_knob = point_in_rect( mx, my, knob_rect.x, knob_rect.y, knob_rect.w, knob_rect.h );

        float hover_target = ( in_knob || exp.open ) ? 1.0f : 0.0f;
        exp.hover_anim += ( hover_target - exp.hover_anim ) * ( 1.0f - std::exp( -14.0f * ctx.dt ) );

        if ( in_knob && w.mouse_left_clicked( ) )
            exp.open = !exp.open;

        color knob_col = exp.open ? ctx.th.accent : lerp_color( ctx.th.text_dim, ctx.th.accent, exp.hover_anim );
        draw_icon_centered( ctx, ifnt, 0xF013, knob_cx, knob_cy, knob_col );

        rect ctrl = { control_x, cursor.y + ( row_h - control_h ) * 0.5f, control_w, control_h };
        cursor.y += row_h;
        return ctrl;
    }

    rect expander_popup_begin( context& ctx, const rect& anchor_rect, const char* title, u32 icon, expander& exp, float w, float h )
    {
        text_renderer& tr = *ctx.tr;
        font& tf = *ctx.text_font;
        font& ifnt = *ctx.icon_font;

        ctx.w->restore_click( );

        float target = exp.open ? 1.0f : 0.0f;
        exp.anim += ( target - exp.anim ) * ( 1.0f - std::exp( -18.0f * ctx.dt ) );

        float px = anchor_rect.x + anchor_rect.w + 8.0f;
        float py = anchor_rect.y;

        float screen_w = ( float ) ctx.d->width( );
        float screen_h = ( float ) ctx.d->height( );
        if ( px + w > screen_w ) px = anchor_rect.x - w - 8.0f;
        if ( px < 4.0f ) px = 4.0f;
        if ( py + h > screen_h ) py = screen_h - h - 4.0f;
        if ( py < 4.0f ) py = 4.0f;

        rect popup = { px, py, w, h };
        exp.popup_rect = popup;

        if ( exp.anim <= 0.002f )
            return { popup.x + 14.0f, popup.y + 40.0f, popup.w - 28.0f, popup.h - 48.0f };

        float prev_alpha = tr.alpha( );
        tr.set_alpha( prev_alpha * exp.anim );

        const float title_h = 32.0f;

        tr.draw_rect( popup, ctx.th.child_bg );
        tr.draw_rect( { popup.x, popup.y, popup.w, title_h }, ctx.th.child_title_bg );
        tr.draw_rect( { popup.x, popup.y + title_h, popup.w, 1.0f }, ctx.th.line );
        tr.draw_rect_outline( popup, 1.0f, ctx.th.line );

        float title_cy = popup.y + title_h * 0.5f;
        float icon_x = popup.x + 12.0f;
        if ( icon != 0 )
        {
            draw_icon_centered( ctx, ifnt, icon, icon_x + 8.0f, title_cy, ctx.th.accent );
            icon_x += 24.0f;
        }

        if ( title )
        {
            float text_y = popup.y + ( title_h - tf.line_height( ) ) * 0.5f;
            tr.draw_text( tf, title, icon_x + 2.0f, text_y, ctx.th.text );
        }

        return { popup.x + 14.0f, popup.y + title_h + 8.0f, popup.w - 28.0f, popup.h - title_h - 16.0f };
    }

    void expander_popup_end( context& ctx, expander& exp )
    {
        window& w = *ctx.w;
        text_renderer& tr = *ctx.tr;

        if ( exp.anim > 0.002f )
            tr.set_alpha( 1.0f );

        if ( exp.open && w.mouse_left_clicked( ) )
        {
            float mx = ( float ) w.mouse_x( );
            float my = ( float ) w.mouse_y( );
            bool in_popup = point_in_rect( mx, my, exp.popup_rect.x, exp.popup_rect.y, exp.popup_rect.w, exp.popup_rect.h );
            bool in_knob = point_in_rect( mx, my, exp.knob_rect.x, exp.knob_rect.y, exp.knob_rect.w, exp.knob_rect.h );
            if ( !in_popup && !in_knob )
                exp.open = false;
        }
    }

    config_result config_selector_widget( context& ctx, float x, float y, float h, config_selector& state )
    {
        window& w = *ctx.w;
        text_renderer& tr = *ctx.tr;
        font& tf = *ctx.text_font;
        font& ifnt = *ctx.icon_font;

        config_result result;

        const char* label = ( state.selected >= 0 && state.selected < ( int ) state.configs.size( ) )
            ? state.configs[ state.selected ].c_str( ) : "";
        float label_w = tr.measure_text( tf, label ).x;

        const float pad_left = 14.0f;
        const float cloud_size = 16.0f;
        const float gap_after_cloud = 10.0f;
        const float gap_before_chevron = 10.0f;
        const float chevron_size = 12.0f;
        const float pad_right = 14.0f;
        const float min_label = 40.0f;

        float target_w = pad_left + cloud_size + gap_after_cloud + ( label_w < min_label ? min_label : label_w ) + gap_before_chevron + chevron_size + pad_right;

        if ( state.width_anim <= 0.0f ) state.width_anim = target_w;
        state.width_anim += ( target_w - state.width_anim ) * ( 1.0f - std::exp( -14.0f * ctx.dt ) );

        float width = state.width_anim;
        result.rendered_width = width;

        rect btn = { x, y, width, h };

        float mx = ( float ) w.mouse_x( );
        float my = ( float ) w.mouse_y( );
        bool in_btn = point_in_rect( mx, my, btn.x, btn.y, btn.w, btn.h );

        const float item_h = 32.0f;
        const float panel_w = 200.0f;
        const float panel_x = x;
        const float panel_y = y + h + 6.0f;
        const float action_h = 34.0f;
        const int action_count = 2;
        const int item_count = ( int ) state.configs.size( );
        float sep_h = 6.0f;
        float full_h = ( float ) item_count * item_h + sep_h + ( float ) action_count * action_h;

        state.hover_item = -1;
        state.hover_action = -1;

        if ( state.anim > 0.4f )
        {
            for ( int i = 0; i < item_count; ++i )
            {
                float iy = panel_y + ( float ) i * item_h;
                if ( point_in_rect( mx, my, panel_x, iy, panel_w, item_h ) )
                {
                    state.hover_item = i;
                    break;
                }
            }
            if ( state.hover_item < 0 )
            {
                float ay = panel_y + ( float ) item_count * item_h + sep_h;
                for ( int a = 0; a < action_count; ++a )
                {
                    if ( point_in_rect( mx, my, panel_x, ay + ( float ) a * action_h, panel_w, action_h ) )
                    {
                        state.hover_action = a;
                        break;
                    }
                }
            }
        }

        if ( w.mouse_left_clicked( ) )
        {
            if ( in_btn )
            {
                state.open = !state.open;
            }
            else if ( state.hover_item >= 0 )
            {
                if ( state.selected != state.hover_item )
                {
                    state.selected = state.hover_item;
                    result.action = config_action::select;
                    result.index = state.hover_item;
                }
                state.open = false;
            }
            else if ( state.hover_action >= 0 )
            {
                result.action = ( state.hover_action == 0 ) ? config_action::save : config_action::new_config;
                state.open = false;
            }
            else
            {
                state.open = false;
            }
        }

        float target_anim = state.open ? 1.0f : 0.0f;
        state.anim += ( target_anim - state.anim ) * ( 1.0f - std::exp( -18.0f * ctx.dt ) );

        color bd = state.open ? ctx.th.accent : ctx.th.line;
        tr.draw_rect( btn, ctx.th.bg );
        tr.draw_rect_outline( btn, 1.0f, bd );

        float cy = y + h * 0.5f;
        draw_icon_centered( ctx, ifnt, 0xF0C2, x + pad_left + cloud_size * 0.5f, cy, ctx.th.accent );

        float text_y = y + ( h - tf.line_height( ) ) * 0.5f;
        float text_x = x + pad_left + cloud_size + gap_after_cloud;
        float text_clip_x = x + width - pad_right - chevron_size - gap_before_chevron + 4.0f;
        tr.draw_text_clipped( tf, label, text_x, text_y, text_clip_x, ctx.th.text, ctx.th.bg, 8.0f );

        draw_icon_centered( ctx, ifnt, 0xF078, x + width - pad_right - chevron_size * 0.5f, cy, ctx.th.text_dim );

        if ( state.anim > 0.002f )
        {
            float panel_h = full_h * state.anim;
            tr.draw_rect( { panel_x, panel_y, panel_w, panel_h }, ctx.th.panel_bg );

            for ( int i = 0; i < item_count; ++i )
            {
                float iy = panel_y + ( float ) i * item_h;
                float visible = item_h * state.anim * ( float ) ( item_count + 1 ) - ( float ) i * item_h;
                float reveal = visible / item_h;
                if ( reveal > 1.0f ) reveal = 1.0f;
                if ( reveal <= 0.01f ) continue;

                bool is_hover = ( state.hover_item == i );
                if ( is_hover )
                {
                    color hb = ctx.th.item_hover_bg;
                    hb.a *= reveal;
                    tr.draw_rect( { panel_x + 1.0f, iy, panel_w - 2.0f, item_h }, hb );
                }

                color col = ( i == state.selected ) ? ctx.th.accent : ctx.th.text_dim;
                col.a *= reveal;

                float tyi = iy + ( item_h - tf.line_height( ) ) * 0.5f;
                tr.draw_text_clipped( tf, state.configs[ i ].c_str( ), panel_x + 14.0f, tyi, panel_x + panel_w - 10.0f, col, ctx.th.panel_bg, 10.0f );

                if ( i == state.selected )
                {
                    color chk = ctx.th.accent;
                    chk.a *= reveal;
                    draw_icon_centered( ctx, ifnt, 0xF00C, panel_x + panel_w - 18.0f, iy + item_h * 0.5f, chk );
                }
            }

            float sep_y = panel_y + ( float ) item_count * item_h + sep_h * 0.5f;
            if ( state.anim > 0.6f )
            {
                color sep_c = ctx.th.line;
                sep_c.a *= ( state.anim - 0.6f ) / 0.4f;
                tr.draw_rect( { panel_x + 10.0f, sep_y, panel_w - 20.0f, 1.0f }, sep_c );
            }

            const u32 action_icons[ 2 ] = { 0xF0C7, 0xF067 };
            const char* action_labels[ 2 ] = { "Save Current", "New Config" };

            for ( int a = 0; a < action_count; ++a )
            {
                float ay = panel_y + ( float ) item_count * item_h + sep_h + ( float ) a * action_h;
                float reveal = ( state.anim - 0.5f ) / 0.5f;
                if ( reveal < 0.0f ) reveal = 0.0f;
                if ( reveal > 1.0f ) reveal = 1.0f;
                if ( reveal <= 0.01f ) continue;

                bool is_hover = ( state.hover_action == a );
                if ( is_hover )
                {
                    color hb = ctx.th.item_hover_bg;
                    hb.a *= reveal;
                    tr.draw_rect( { panel_x + 1.0f, ay, panel_w - 2.0f, action_h }, hb );
                }

                color col = is_hover ? ctx.th.text : ctx.th.text_dim;
                col.a *= reveal;
                draw_icon_centered( ctx, ifnt, action_icons[ a ], panel_x + 20.0f, ay + action_h * 0.5f, col );

                float tyi = ay + ( action_h - tf.line_height( ) ) * 0.5f;
                tr.draw_text( tf, action_labels[ a ], panel_x + 40.0f, tyi, col );
            }

            tr.draw_rect_outline( { panel_x, panel_y, panel_w, panel_h }, 1.0f, ctx.th.line );
        }

        return result;
    }

    static void draw_rgb_channel( context& ctx, const rect& r, float value, const color& fill_c, const char* label )
    {
        text_renderer& tr = *ctx.tr;
        font& tf = *ctx.text_font;

        const float track_h = 4.0f;
        float track_y = r.y + ( r.h - track_h ) * 0.5f;

        tr.draw_rect( { r.x, track_y, r.w, track_h }, ctx.th.track );
        tr.draw_rect( { r.x, track_y, r.w * value, track_h }, fill_c );

        float handle_r = 6.0f;
        float handle_x = r.x + r.w * value - handle_r;
        float handle_y = r.y + ( r.h - handle_r * 2.0f ) * 0.5f;
        tr.draw_rect( { handle_x, handle_y, handle_r * 2.0f, handle_r * 2.0f }, ctx.th.text );
        tr.draw_rect_outline( { handle_x, handle_y, handle_r * 2.0f, handle_r * 2.0f }, 1.0f, fill_c );

        tr.draw_text( tf, label, r.x - 18.0f, r.y + ( r.h - tf.line_height( ) ) * 0.5f, ctx.th.text_dim );
    }

    bool color_picker_widget( context& ctx, const rect& r, color_picker& state )
    {
        window& w = *ctx.w;
        text_renderer& tr = *ctx.tr;
        font& tf = *ctx.text_font;

        if ( !state.initialized )
        {
            state.r_slider.min_val = 0.0f; state.r_slider.max_val = 1.0f; state.r_slider.value = state.value.r;
            state.g_slider.min_val = 0.0f; state.g_slider.max_val = 1.0f; state.g_slider.value = state.value.g;
            state.b_slider.min_val = 0.0f; state.b_slider.max_val = 1.0f; state.b_slider.value = state.value.b;
            state.a_slider.min_val = 0.0f; state.a_slider.max_val = 1.0f; state.a_slider.value = state.value.a;
            state.initialized = true;
        }

        bool changed = false;

        const float swatch_w = 72.0f;
        const float swatch_h = r.h;
        const float gap = 14.0f;

        rect swatch = { r.x, r.y, swatch_w, swatch_h };

        tr.draw_rect( swatch, { 0.08f, 0.09f, 0.12f, 1.0f } );
        const float checker = 8.0f;
        int rows = ( int ) ( swatch.h / checker ) + 1;
        int cols = ( int ) ( swatch.w / checker ) + 1;
        for ( int yi = 0; yi < rows; ++yi )
        {
            for ( int xi = 0; xi < cols; ++xi )
            {
                if ( ( ( xi + yi ) & 1 ) == 0 ) continue;
                float cx = swatch.x + ( float ) xi * checker;
                float cy = swatch.y + ( float ) yi * checker;
                float cw = ( cx + checker > swatch.x + swatch.w ) ? ( swatch.x + swatch.w - cx ) : checker;
                float ch = ( cy + checker > swatch.y + swatch.h ) ? ( swatch.y + swatch.h - cy ) : checker;
                if ( cw <= 0.0f || ch <= 0.0f ) continue;
                tr.draw_rect( { cx, cy, cw, ch }, { 0.16f, 0.17f, 0.20f, 1.0f } );
            }
        }
        tr.draw_rect( swatch, state.value );
        tr.draw_rect_outline( swatch, 1.0f, ctx.th.line );

        float sliders_x = r.x + swatch_w + gap + 16.0f;
        float sliders_w = r.x + r.w - sliders_x;
        if ( sliders_w < 40.0f ) sliders_w = 40.0f;

        const int channel_count = 4;
        float channel_h = 22.0f;
        float total_h = ( float ) channel_count * channel_h + ( float ) ( channel_count - 1 ) * 8.0f;
        float sy = r.y + ( r.h - total_h ) * 0.5f;

        slider* sliders[ 4 ] = { &state.r_slider, &state.g_slider, &state.b_slider, &state.a_slider };
        const char* labels[ 4 ] = { "R", "G", "B", "A" };
        const color fill_colors[ 4 ] = {
            { 0.95f, 0.30f, 0.35f, 1.0f },
            { 0.35f, 0.85f, 0.45f, 1.0f },
            { 0.35f, 0.55f, 0.95f, 1.0f },
            { 0.85f, 0.85f, 0.85f, 1.0f },
        };

        for ( int i = 0; i < channel_count; ++i )
        {
            rect row_rect = { sliders_x, sy + ( float ) i * ( channel_h + 8.0f ), sliders_w - 44.0f, channel_h };
            float mx = ( float ) w.mouse_x( );
            float my = ( float ) w.mouse_y( );
            bool in_rect = point_in_rect( mx, my, row_rect.x, row_rect.y, row_rect.w, row_rect.h );

            if ( in_rect && w.mouse_left_clicked( ) )
                sliders[ i ]->dragging = true;
            if ( !w.mouse_left_pressed( ) )
                sliders[ i ]->dragging = false;

            if ( sliders[ i ]->dragging )
            {
                float t = clamp01( ( mx - row_rect.x ) / row_rect.w );
                if ( t != sliders[ i ]->value )
                {
                    sliders[ i ]->value = t;
                    changed = true;
                }
            }

            draw_rgb_channel( ctx, row_rect, sliders[ i ]->value, fill_colors[ i ], labels[ i ] );

            char buf[ 16 ];
            std::snprintf( buf, sizeof( buf ), "%d", ( int ) ( sliders[ i ]->value * 255.0f + 0.5f ) );
            float vx = row_rect.x + row_rect.w + 8.0f;
            float vy = row_rect.y + ( row_rect.h - tf.line_height( ) ) * 0.5f;
            tr.draw_text( tf, buf, vx, vy, ctx.th.text );
        }

        state.value.r = state.r_slider.value;
        state.value.g = state.g_slider.value;
        state.value.b = state.b_slider.value;
        state.value.a = state.a_slider.value;

        return changed;
    }

    static inline int hex_digit( char c )
    {
        if ( c >= '0' && c <= '9' ) return c - '0';
        if ( c >= 'a' && c <= 'f' ) return c - 'a' + 10;
        if ( c >= 'A' && c <= 'F' ) return c - 'A' + 10;
        return -1;
    }

    static inline void color_to_hex( const color& c, char out[ 8 ] )
    {
        int r = ( int ) ( clamp01( c.r ) * 255.0f + 0.5f );
        int g = ( int ) ( clamp01( c.g ) * 255.0f + 0.5f );
        int b = ( int ) ( clamp01( c.b ) * 255.0f + 0.5f );
        snprintf( out, 8, "#%02X%02X%02X", r, g, b );
    }

    static inline bool hex_to_color( std::string_view s, color& out )
    {
        std::string_view t = s;
        while ( !t.empty( ) && ( t.front( ) == ' ' || t.front( ) == '\t' || t.front( ) == '#' ) )
            t.remove_prefix( 1 );
        while ( !t.empty( ) && ( t.back( ) == ' ' || t.back( ) == '\t' || t.back( ) == '\n' || t.back( ) == '\r' ) )
            t.remove_suffix( 1 );
        if ( t.size( ) != 6 && t.size( ) != 8 ) return false;
        int d[ 8 ] = {};
        for ( size_t i = 0; i < t.size( ); ++i )
        {
            int v = hex_digit( t[ i ] );
            if ( v < 0 ) return false;
            d[ i ] = v;
        }
        out.r = ( float ) ( ( d[ 0 ] << 4 ) | d[ 1 ] ) / 255.0f;
        out.g = ( float ) ( ( d[ 2 ] << 4 ) | d[ 3 ] ) / 255.0f;
        out.b = ( float ) ( ( d[ 4 ] << 4 ) | d[ 5 ] ) / 255.0f;
        if ( t.size( ) == 8 )
            out.a = ( float ) ( ( d[ 6 ] << 4 ) | d[ 7 ] ) / 255.0f;
        return true;
    }

    rect row_with_color_picker( context& ctx, rect& cursor, const char* caption, float control_w, color_picker& cp, float row_h, float control_h )
    {
        window& w = *ctx.w;
        text_renderer& tr = *ctx.tr;
        font& tf = *ctx.text_font;

        float text_y = cursor.y + ( row_h - tf.line_height( ) ) * 0.5f;
        float control_x = cursor.x + cursor.w - control_w;

        const float swatch_size = 22.0f;
        const float swatch_offset = 24.0f;
        float swatch_cx = control_x - swatch_offset;
        float swatch_cy = cursor.y + row_h * 0.5f;
        rect swatch = { swatch_cx - swatch_size * 0.5f, swatch_cy - swatch_size * 0.5f, swatch_size, swatch_size };
        cp.swatch_rect = swatch;

        float caption_clip = swatch.x - 6.0f;
        tr.draw_text_clipped( tf, caption, cursor.x + 2.0f, text_y, caption_clip, ctx.th.text_dim, ctx.th.child_bg, 12.0f );

        float mx = ( float ) w.mouse_x( );
        float my = ( float ) w.mouse_y( );
        bool in_swatch = point_in_rect( mx, my, swatch.x, swatch.y, swatch.w, swatch.h );

        float hover_target = ( in_swatch || cp.open ) ? 1.0f : 0.0f;
        cp.hover_anim += ( hover_target - cp.hover_anim ) * ( 1.0f - std::exp( -14.0f * ctx.dt ) );

        if ( in_swatch && w.mouse_left_clicked( ) )
            cp.open = !cp.open;

        color fill = cp.value;
        fill.a = 1.0f;
        tr.draw_rect( swatch, fill );
        color border = cp.open ? ctx.th.accent : lerp_color( ctx.th.line, ctx.th.accent, cp.hover_anim );
        tr.draw_rect_outline( swatch, 1.0f, border );

        rect ctrl = { control_x, cursor.y + ( row_h - control_h ) * 0.5f, control_w, control_h };
        cursor.y += row_h;
        return ctrl;
    }

    void color_picker_popup_begin( context& ctx, color_picker& cp )
    {
        window& w = *ctx.w;
        text_renderer& tr = *ctx.tr;
        font& tf = *ctx.text_font;
        font& ifnt = *ctx.icon_font;

        ctx.w->restore_click( );

        float target = cp.open ? 1.0f : 0.0f;
        cp.anim += ( target - cp.anim ) * ( 1.0f - std::exp( -18.0f * ctx.dt ) );

        if ( !cp.initialized )
        {
            cp.r_slider.value = cp.value.r;
            cp.g_slider.value = cp.value.g;
            cp.b_slider.value = cp.value.b;
            cp.a_slider.value = cp.value.a;
            cp.r_slider.min_val = 0.0f; cp.r_slider.max_val = 1.0f;
            cp.g_slider.min_val = 0.0f; cp.g_slider.max_val = 1.0f;
            cp.b_slider.min_val = 0.0f; cp.b_slider.max_val = 1.0f;
            cp.a_slider.min_val = 0.0f; cp.a_slider.max_val = 1.0f;
            cp.initialized = true;
        }

        const float popup_w = 240.0f;
        const float popup_h = 230.0f;

        float px = cp.swatch_rect.x + cp.swatch_rect.w + 10.0f;
        float py = cp.swatch_rect.y;

        float screen_w = ( float ) ctx.d->width( );
        float screen_h = ( float ) ctx.d->height( );
        if ( screen_w <= 0.0f ) screen_w = 1024.0f;
        if ( screen_h <= 0.0f ) screen_h = 685.0f;
        float logical_w = screen_w / tr.ui_scale( );
        float logical_h = screen_h / tr.ui_scale( );
        if ( px + popup_w > logical_w ) px = cp.swatch_rect.x - popup_w - 10.0f;
        if ( px < 4.0f ) px = 4.0f;
        if ( py + popup_h > logical_h ) py = logical_h - popup_h - 4.0f;
        if ( py < 4.0f ) py = 4.0f;

        rect popup = { px, py, popup_w, popup_h };
        cp.popup_rect = popup;

        if ( cp.anim <= 0.002f )
            return;

        float prev_alpha = tr.alpha( );
        tr.set_alpha( prev_alpha * cp.anim );

        const float title_h = 30.0f;
        tr.draw_rect( popup, ctx.th.child_bg );
        tr.draw_rect( { popup.x, popup.y, popup.w, title_h }, ctx.th.child_title_bg );
        tr.draw_rect( { popup.x, popup.y + title_h, popup.w, 1.0f }, ctx.th.line );
        tr.draw_rect_outline( popup, 1.0f, ctx.th.line );

        float tyt = popup.y + ( title_h - tf.line_height( ) ) * 0.5f;
        tr.draw_text( tf, "Color", popup.x + 12.0f, tyt, ctx.th.text );

        rect preview_rect = { popup.x + 12.0f, popup.y + title_h + 10.0f, popup.w - 24.0f, 24.0f };
        color preview_c = cp.value;
        preview_c.a = 1.0f;
        tr.draw_rect( preview_rect, preview_c );
        tr.draw_rect_outline( preview_rect, 1.0f, ctx.th.line );

        const char* labels[ 4 ] = { "R", "G", "B", "A" };
        slider* sliders[ 4 ] = { &cp.r_slider, &cp.g_slider, &cp.b_slider, &cp.a_slider };
        float slider_row_h = 24.0f;
        float sliders_y = preview_rect.y + preview_rect.h + 10.0f;
        for ( int i = 0; i < 4; ++i )
        {
            float ry = sliders_y + ( float ) i * slider_row_h;
            float lx = popup.x + 14.0f;
            float ly = ry + ( slider_row_h - tf.line_height( ) ) * 0.5f;
            tr.draw_text( tf, labels[ i ], lx, ly, ctx.th.text_dim );

            rect srect = { popup.x + 30.0f, ry + 4.0f, popup.w - 80.0f, slider_row_h - 8.0f };
            slider_ctrl( ctx, srect, *sliders[ i ] );

            char vbuf[ 8 ];
            snprintf( vbuf, sizeof( vbuf ), "%d", ( int ) ( sliders[ i ]->value * 255.0f + 0.5f ) );
            float vx = popup.x + popup.w - 40.0f;
            float vy = ry + ( slider_row_h - tf.line_height( ) ) * 0.5f;
            tr.draw_text( tf, vbuf, vx, vy, ctx.th.text );
        }

        cp.value.r = cp.r_slider.value;
        cp.value.g = cp.g_slider.value;
        cp.value.b = cp.b_slider.value;
        cp.value.a = cp.a_slider.value;

        float hex_y = sliders_y + 4.0f * slider_row_h + 8.0f;
        char hex_buf[ 8 ];
        color_to_hex( cp.value, hex_buf );

        rect hex_box = { popup.x + 12.0f, hex_y, popup.w - 84.0f, 26.0f };
        tr.draw_rect( hex_box, ctx.th.bg );
        tr.draw_rect_outline( hex_box, 1.0f, ctx.th.line );
        float hbx = hex_box.x + 8.0f;
        float hby = hex_box.y + ( hex_box.h - tf.line_height( ) ) * 0.5f;
        tr.draw_text( tf, hex_buf, hbx, hby, ctx.th.text );

        rect copy_btn = { popup.x + popup.w - 64.0f, hex_y, 26.0f, 26.0f };
        rect paste_btn = { popup.x + popup.w - 34.0f, hex_y, 26.0f, 26.0f };

        float mx = ( float ) w.mouse_x( );
        float my = ( float ) w.mouse_y( );
        bool in_copy = point_in_rect( mx, my, copy_btn.x, copy_btn.y, copy_btn.w, copy_btn.h );
        bool in_paste = point_in_rect( mx, my, paste_btn.x, paste_btn.y, paste_btn.w, paste_btn.h );

        color copy_bg = in_copy ? ctx.th.item_hover_bg : ctx.th.bg;
        color paste_bg = in_paste ? ctx.th.item_hover_bg : ctx.th.bg;
        tr.draw_rect( copy_btn, copy_bg );
        tr.draw_rect( paste_btn, paste_bg );
        tr.draw_rect_outline( copy_btn, 1.0f, ctx.th.line );
        tr.draw_rect_outline( paste_btn, 1.0f, ctx.th.line );
        draw_icon_centered( ctx, ifnt, 0xF0C5, copy_btn.x + copy_btn.w * 0.5f, copy_btn.y + copy_btn.h * 0.5f, ctx.th.text_dim );
        draw_icon_centered( ctx, ifnt, 0xF0EA, paste_btn.x + paste_btn.w * 0.5f, paste_btn.y + paste_btn.h * 0.5f, ctx.th.text_dim );

        if ( w.mouse_left_clicked( ) )
        {
            if ( in_copy )
            {
                w.set_clipboard_text( hex_buf );
            }
            else if ( in_paste )
            {
                std::string s = w.get_clipboard_text( );
                color parsed;
                parsed.a = cp.value.a;
                if ( hex_to_color( s, parsed ) )
                {
                    cp.value.r = parsed.r;
                    cp.value.g = parsed.g;
                    cp.value.b = parsed.b;
                    cp.r_slider.value = parsed.r;
                    cp.g_slider.value = parsed.g;
                    cp.b_slider.value = parsed.b;
                }
            }
        }

        tr.set_alpha( prev_alpha );
    }

    void color_picker_popup_end( context& ctx, color_picker& cp )
    {
        window& w = *ctx.w;

        if ( cp.open && w.mouse_left_clicked( ) )
        {
            float mx = ( float ) w.mouse_x( );
            float my = ( float ) w.mouse_y( );
            bool in_popup = point_in_rect( mx, my, cp.popup_rect.x, cp.popup_rect.y, cp.popup_rect.w, cp.popup_rect.h );
            bool in_swatch = point_in_rect( mx, my, cp.swatch_rect.x, cp.swatch_rect.y, cp.swatch_rect.w, cp.swatch_rect.h );
            if ( !in_popup && !in_swatch )
                cp.open = false;
        }
    }
}
