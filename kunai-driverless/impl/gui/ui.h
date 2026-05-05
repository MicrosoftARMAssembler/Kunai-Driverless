#pragma once

#include "types.h"
#include <string>
#include <string_view>
#include <vector>

namespace kunai
{
    class window;
    class device;
    class font;
    class text_renderer;
    class image;

    namespace ui
    {
        struct theme
        {
            color bg = {};
            color child_bg = {};
            color child_title_bg = {};
            color line = {};
            color text_dim = {};
            color text = {};
            color accent = {};
            color accent_dim = {};
            color placeholder = {};
            color highlight_bg = {};
            color highlight_border = {};
            color panel_bg = {};
            color item_hover_bg = {};
            color selection = {};
            color track = {};
        };

        struct context
        {
            window* w = nullptr;
            device* d = nullptr;
            text_renderer* tr = nullptr;
            font* text_font = nullptr;
            font* icon_font = nullptr;
            float dt = 0.0f;
            float ui_scale = 1.0f;
            theme th;
        };

        void draw_icon_centered( context& ctx, font& f, u32 cp, float cx, float cy, const color& c );

        rect split_h_left( const rect& r, float ratio, float gap );
        rect split_h_right( const rect& r, float ratio, float gap );
        rect split_v_top( const rect& r, float ratio, float gap );
        rect split_v_bottom( const rect& r, float ratio, float gap );

        rect child( context& ctx, const rect& area, const char* title, u32 icon );

        rect row( context& ctx, rect& cursor, const char* caption, float control_w, float row_h = 36.0f, float control_h = 28.0f );

        struct checkbox
        {
            bool value = false;
            float anim = 0.0f;
        };

        bool checkbox_ctrl( context& ctx, const rect& r, checkbox& state );

        struct slider
        {
            float value = 0.0f;
            float min_val = 0.0f;
            float max_val = 1.0f;
            bool dragging = false;
            float hover_anim = 0.0f;
        };

        bool slider_ctrl( context& ctx, const rect& r, slider& state );

        struct text_edit
        {
            std::string text;
            int cursor = 0;
            int anchor = 0;
            bool focused = false;
            bool dragging = false;
            float cursor_blink = 0.0f;
            size_t max_length = 256;

            bool has_selection( ) const { return cursor != anchor; }
            int sel_start( ) const { return cursor < anchor ? cursor : anchor; }
            int sel_end( ) const { return cursor > anchor ? cursor : anchor; }
            void delete_selection( );
            void insert_text( std::string_view s );
            void select_all( );
        };

        bool search_box( context& ctx, const rect& r, text_edit& state, const char* placeholder );

        struct combobox
        {
            const char* const* options = nullptr;
            int count = 0;
            int selected = 0;
            bool open = false;
            float anim = 0.0f;
            float width_anim = 0.0f;
            int hover_item = -1;
        };

        bool combobox_widget( context& ctx, const rect& r, combobox& state );

        struct config_selector
        {
            std::vector< std::string > configs;
            int selected = 0;
            bool open = false;
            float anim = 0.0f;
            float width_anim = 0.0f;
            int hover_item = -1;
            int hover_action = -1;
        };

        enum class config_action
        {
            none,
            select,
            save,
            new_config
        };

        struct config_result
        {
            config_action action = config_action::none;
            int index = -1;
            float rendered_width = 0.0f;
        };

        config_result config_selector_widget( context& ctx, float x, float y, float h, config_selector& state );

        struct tab_item
        {
            const char* label;
            u32 icon;
        };

        struct tabs
        {
            const tab_item* items = nullptr;
            int count = 0;
            int selected = 0;
            float highlight_y = 0.0f;
        };

        int tabs_widget( context& ctx, float x, float y, float w, float item_h, tabs& state );

        bool icon_button( context& ctx, const rect& r, u32 icon, const color& bg, const color& icon_col );

        struct content_anim
        {
            int current = -1;
            float progress = 1.0f;

            void update( int new_tab, float dt );
            float alpha( ) const { return progress; }
            float offset_y( ) const { return ( 1.0f - progress ) * 18.0f; }
        };

        struct expander
        {
            bool open = false;
            float anim = 0.0f;
            float hover_anim = 0.0f;
            rect knob_rect = {};
            rect popup_rect = {};
        };

        rect row_with_expander(
            context& ctx,
            rect& cursor,
            const char* caption,
            float control_w,
            expander& exp,
            float row_h = 36.0f,
            float control_h = 28.0f );

        rect expander_popup_begin(
            context& ctx,
            const rect& anchor_rect,
            const char* title,
            u32 icon,
            expander& exp,
            float w = 300.0f,
            float h = 200.0f );

        void expander_popup_end( context& ctx, expander& exp );

        struct color_picker
        {
            color value = { 0.24f, 0.48f, 0.98f, 1.0f };
            slider r_slider;
            slider g_slider;
            slider b_slider;
            slider a_slider;
            bool initialized = false;
            bool open = false;
            float anim = 0.0f;
            float hover_anim = 0.0f;
            rect swatch_rect = {};
            rect popup_rect = {};
        };

        bool color_picker_widget( context& ctx, const rect& r, color_picker& state );

        rect row_with_color_picker( context& ctx, rect& cursor, const char* caption, float control_w, color_picker& cp, float row_h = 36.0f, float control_h = 28.0f );

        void color_picker_popup_begin( context& ctx, color_picker& cp );
        void color_picker_popup_end( context& ctx, color_picker& cp );
    }
}
