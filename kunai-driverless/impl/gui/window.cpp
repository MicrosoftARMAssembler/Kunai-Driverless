#include "window.h"
#include <stdexcept>
#include <windowsx.h>
#include <deps/oxorany/oxorany.h>
#include <dwmapi.h>

namespace kunai
{
    static std::wstring to_wide( const std::string& s )
    {
        if ( s.empty( ) ) return {};
        int size = MultiByteToWideChar( CP_UTF8, 0, s.data( ), ( int ) s.size( ), nullptr, 0 );
        std::wstring out( size, 0 );
        MultiByteToWideChar( CP_UTF8, 0, s.data( ), ( int ) s.size( ), out.data( ), size );
        return out;
    }

    static HWND get_discord_window( ) {
        return FindWindowA( oxorany( "Chrome_WidgetWin_1" ), oxorany( "Discord Overlay" ) );
    }

    window::window( i32 width, i32 height )
        : m_width( width ), m_height( height )
    {
        typedef BOOL( WINAPI* set_dpi_ctx_t )( DPI_AWARENESS_CONTEXT );
        HMODULE user32 = GetModuleHandleW( L"user32.dll" );
        if ( user32 )
        {
            set_dpi_ctx_t set_dpi_ctx = ( set_dpi_ctx_t ) GetProcAddress( user32, "SetProcessDpiAwarenessContext" );
            if ( set_dpi_ctx )
                set_dpi_ctx( DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 );
        }

        this->m_hwnd = get_discord_window ( );
        while ( !m_hwnd ) { m_hwnd = get_discord_window( ); SleepEx( 1, false ); };

        RECT rect{};
        GetClientRect( m_hwnd, &rect );
        ClientToScreen( m_hwnd, reinterpret_cast< POINT* >( &rect.left ) );
        ClientToScreen( m_hwnd, reinterpret_cast< POINT* >( &rect.right ) );
        if ( !SetWindowPos( this->m_hwnd, nullptr, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER ) )
            return;

        GetWindowRect( GetDesktopWindow( ), &rect );

        MARGINS window_margin{ -1 };
        DwmExtendFrameIntoClientArea( this->m_hwnd, &window_margin );
        SetLayeredWindowAttributes( this->m_hwnd, 0, 255, LWA_ALPHA );

        UpdateWindow( this->m_hwnd );
        ShowWindow( this->m_hwnd, SW_SHOW );
    }

    window::~window( )
    {

    }

    bool window::poll( )
    {
        m_mouse_left_clicked = false;
        m_click_consumed = false;
        m_text_input.clear( );
        m_key_events.clear( );
        MSG msg = {};
        while ( PeekMessageW( &msg, nullptr, 0, 0, PM_REMOVE ) )
        {
            if ( msg.message == WM_QUIT )
                m_should_close = true;
            TranslateMessage( &msg );
            DispatchMessageW( &msg );
        }
        return !m_should_close;
    }

    bool window::resized( )
    {
        bool r = m_resized;
        m_resized = false;
        return r;
    }

    LRESULT CALLBACK window::wnd_proc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam )
    {
        if ( msg == WM_NCCREATE )
        {
            CREATESTRUCTW* cs = reinterpret_cast< CREATESTRUCTW* >( lparam );
            SetWindowLongPtrW( hwnd, GWLP_USERDATA, reinterpret_cast< LONG_PTR >( cs->lpCreateParams ) );
        }

        window* self = reinterpret_cast< window* >( GetWindowLongPtrW( hwnd, GWLP_USERDATA ) );

        if ( self )
        {
            switch ( msg )
            {
            case WM_SIZE:
                self->m_width = LOWORD( lparam );
                self->m_height = HIWORD( lparam );
                self->m_resized = true;
                return 0;
            case WM_CLOSE:
                PostQuitMessage( 0 );
                return 0;
            case WM_DESTROY:
                PostQuitMessage( 0 );
                return 0;
            case WM_KEYDOWN:
            {
                key_event ke;
                ke.vk = ( u32 ) wparam;
                ke.ctrl = ( GetKeyState( VK_CONTROL ) & 0x8000 ) != 0;
                ke.shift = ( GetKeyState( VK_SHIFT ) & 0x8000 ) != 0;
                ke.alt = ( GetKeyState( VK_MENU ) & 0x8000 ) != 0;
                self->m_key_events.push_back( ke );
                if ( wparam == VK_ESCAPE )
                {
                    PostQuitMessage( 0 );
                    return 0;
                }
                break;
            }
            case WM_MOUSEMOVE:
                self->m_mouse_x = GET_X_LPARAM( lparam );
                self->m_mouse_y = GET_Y_LPARAM( lparam );
                return 0;
            case WM_LBUTTONDOWN:
                self->m_mouse_left_pressed = true;
                self->m_mouse_left_clicked = true;
                return 0;
            case WM_LBUTTONUP:
                self->m_mouse_left_pressed = false;
                return 0;
            case WM_CHAR:
                if ( wparam < 128 )
                    self->m_text_input += ( char ) wparam;
                return 0;
            case WM_NCHITTEST:
            {
                const rect& dr = self->m_drag_region;
                if ( dr.w > 0.0f && dr.h > 0.0f )
                {
                    POINT p = { GET_X_LPARAM( lparam ), GET_Y_LPARAM( lparam ) };
                    ScreenToClient( hwnd, &p );
                    if ( ( f32 ) p.x >= dr.x && ( f32 ) p.x < dr.x + dr.w &&
                         ( f32 ) p.y >= dr.y && ( f32 ) p.y < dr.y + dr.h )
                    {
                        for ( const rect& ex : self->m_drag_exclusions )
                        {
                            if ( ( f32 ) p.x >= ex.x && ( f32 ) p.x < ex.x + ex.w &&
                                 ( f32 ) p.y >= ex.y && ( f32 ) p.y < ex.y + ex.h )
                                return HTCLIENT;
                        }
                        return HTCAPTION;
                    }
                }
                return HTCLIENT;
            }
            case WM_ERASEBKGND:
                return 1;
            }
        }

        return DefWindowProcW( hwnd, msg, wparam, lparam );
    }

    std::string window::get_clipboard_text( ) const
    {
        if ( !OpenClipboard( m_hwnd ) ) return {};
        HANDLE h = GetClipboardData( CF_UNICODETEXT );
        if ( !h ) { CloseClipboard( ); return {}; }
        wchar_t* w = ( wchar_t* ) GlobalLock( h );
        if ( !w ) { CloseClipboard( ); return {}; }
        int len = WideCharToMultiByte( CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr );
        std::string out;
        if ( len > 1 )
        {
            out.resize( ( size_t ) ( len - 1 ) );
            WideCharToMultiByte( CP_UTF8, 0, w, -1, out.data( ), len, nullptr, nullptr );
        }
        GlobalUnlock( h );
        CloseClipboard( );
        return out;
    }

    bool window::set_clipboard_text( std::string_view text ) const
    {
        if ( !OpenClipboard( m_hwnd ) ) return false;
        EmptyClipboard( );
        int wlen = MultiByteToWideChar( CP_UTF8, 0, text.data( ), ( int ) text.size( ), nullptr, 0 );
        HGLOBAL h = GlobalAlloc( GMEM_MOVEABLE, ( ( size_t ) wlen + 1 ) * sizeof( wchar_t ) );
        if ( !h ) { CloseClipboard( ); return false; }
        wchar_t* w = ( wchar_t* ) GlobalLock( h );
        if ( wlen > 0 ) MultiByteToWideChar( CP_UTF8, 0, text.data( ), ( int ) text.size( ), w, wlen );
        w[ wlen ] = 0;
        GlobalUnlock( h );
        SetClipboardData( CF_UNICODETEXT, h );
        CloseClipboard( );
        return true;
    }
}
