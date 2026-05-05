#pragma once

#include "types.h"
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

namespace kunai
{
    using Microsoft::WRL::ComPtr;

    class window;

    class device
    {
    public:
        explicit device( window& win );
        ~device( );

        device( const device& ) = delete;
        device& operator=( const device& ) = delete;

        void begin_frame( const color& clear );
        void end_frame( );
        void resize( i32 width, i32 height );

        ID3D11Device* raw_device( ) const { return m_device.Get( ); }
        ID3D11DeviceContext* context( ) const { return m_context.Get( ); }
        IDXGISwapChain* swap_chain( ) const { return m_swap_chain.Get( ); }
        ID3D11RenderTargetView* back_buffer_rtv( ) const { return m_rtv.Get( ); }
        i32 width( ) const { return m_width; }
        i32 height( ) const { return m_height; }

    private:
        void create_back_buffer( );
        void destroy_back_buffer( );

        window& m_window;
        ComPtr<ID3D11Device> m_device;
        ComPtr<ID3D11DeviceContext> m_context;
        ComPtr<IDXGISwapChain> m_swap_chain;
        ComPtr<ID3D11RenderTargetView> m_rtv;
        i32 m_width = 0;
        i32 m_height = 0;
    };
}
