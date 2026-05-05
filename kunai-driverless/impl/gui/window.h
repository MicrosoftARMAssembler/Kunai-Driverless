#pragma once

#include "types.h"
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>
#include <Windows.h>

namespace kunai
{
    struct key_event
    {
        u32 vk = 0;
        bool ctrl = false;
        bool shift = false;
        bool alt = false;
    };

    class window
    {
    public:
        window( i32 width, i32 height );
        ~window( );

        window( const window& ) = delete;
        window& operator=( const window& ) = delete;

        bool poll( );
        bool resized( );

        void set_drag_region( const rect& r ) { m_drag_region = r; }
        void set_drag_exclusions( std::initializer_list< rect > exclusions ) { m_drag_exclusions.assign( exclusions.begin( ), exclusions.end( ) ); }
        void set_drag_exclusions( const std::vector< rect >& exclusions ) { m_drag_exclusions = exclusions; }
        void clear_drag_exclusions( ) { m_drag_exclusions.clear( ); }

        HWND handle( ) const { return m_hwnd; }
        i32 width( ) const { return m_width; }
        i32 height( ) const { return m_height; }

        i32 mouse_x( ) const { return ( i32 ) ( ( f32 ) m_mouse_x / m_ui_scale ); }
        i32 mouse_y( ) const { return ( i32 ) ( ( f32 ) m_mouse_y / m_ui_scale ); }
        i32 raw_mouse_x( ) const { return m_mouse_x; }
        i32 raw_mouse_y( ) const { return m_mouse_y; }

        void set_ui_scale( f32 scale ) { m_ui_scale = ( scale > 0.0f ) ? scale : 1.0f; }
        f32 ui_scale( ) const { return m_ui_scale; }
        bool mouse_left_pressed( ) const { return m_mouse_left_pressed; }
        bool mouse_left_clicked( ) const { return m_mouse_left_clicked && !m_click_consumed; }
        bool mouse_left_clicked_raw( ) const { return m_mouse_left_clicked; }
        void consume_click( ) { m_click_consumed = true; }
        void restore_click( ) { m_click_consumed = false; }
        std::string_view text_input( ) const { return m_text_input; }
        const std::vector< key_event >& key_events( ) const { return m_key_events; }

        std::string get_clipboard_text( ) const;
        bool set_clipboard_text( std::string_view text ) const;

    private:
        static LRESULT CALLBACK wnd_proc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam );

        HWND m_hwnd = nullptr;
        HINSTANCE m_hinstance = nullptr;
        std::wstring m_class_name;
        std::string m_text_input;
        std::vector< key_event > m_key_events;
        std::vector< rect > m_drag_exclusions;
        rect m_drag_region = {};
        i32 m_width = 0;
        i32 m_height = 0;
        i32 m_mouse_x = 0;
        i32 m_mouse_y = 0;
        f32 m_ui_scale = 1.0f;
        bool m_should_close = false;
        bool m_resized = false;
        bool m_mouse_left_pressed = false;
        bool m_mouse_left_clicked = false;
        bool m_click_consumed = false;
    };
}
