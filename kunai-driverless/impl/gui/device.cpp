#include "device.h"
#include "window.h"
#include <stdexcept>

namespace kunai
{
    device::device( window& win )
        : m_window( win )
    {
        m_width = win.width( );
        m_height = win.height( );

        DXGI_SWAP_CHAIN_DESC scd = {};
        scd.BufferCount = 2;
        scd.BufferDesc.Width = m_width;
        scd.BufferDesc.Height = m_height;
        scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        scd.BufferDesc.RefreshRate.Numerator = 60;
        scd.BufferDesc.RefreshRate.Denominator = 1;
        scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        scd.OutputWindow = win.handle( );
        scd.SampleDesc.Count = 1;
        scd.Windowed = TRUE;

        UINT flags = 0;
#if defined(_DEBUG)
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        D3D_FEATURE_LEVEL feature_levels[ ] = {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
        };
        D3D_FEATURE_LEVEL got_level = {};

        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
            flags, feature_levels, ( UINT ) ( sizeof( feature_levels ) / sizeof( feature_levels [ 0 ] ) ),
            D3D11_SDK_VERSION, &scd,
            m_swap_chain.GetAddressOf( ),
            m_device.GetAddressOf( ),
            &got_level,
            m_context.GetAddressOf( )
        );

        if ( FAILED( hr ) )
        {
            hr = D3D11CreateDeviceAndSwapChain(
                nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
                flags, feature_levels, ( UINT ) ( sizeof( feature_levels ) / sizeof( feature_levels [ 0 ] ) ),
                D3D11_SDK_VERSION, &scd,
                m_swap_chain.GetAddressOf( ),
                m_device.GetAddressOf( ),
                &got_level,
                m_context.GetAddressOf( )
            );
        }

        if ( FAILED( hr ) )
            throw std::runtime_error( "d3d11 device create failed" );

        create_back_buffer( );
    }

    device::~device( ) = default;

    void device::create_back_buffer( )
    {
        ComPtr<ID3D11Texture2D> back;
        m_swap_chain->GetBuffer( 0, IID_PPV_ARGS( back.GetAddressOf( ) ) );
        m_device->CreateRenderTargetView( back.Get( ), nullptr, m_rtv.GetAddressOf( ) );
    }

    void device::destroy_back_buffer( )
    {
        m_rtv.Reset( );
    }

    void device::resize( i32 width, i32 height )
    {
        if ( width <= 0 || height <= 0 ) return;
        if ( width == m_width && height == m_height ) return;

        m_width = width;
        m_height = height;

        m_context->OMSetRenderTargets( 0, nullptr, nullptr );
        destroy_back_buffer( );
        m_swap_chain->ResizeBuffers( 0, width, height, DXGI_FORMAT_UNKNOWN, 0 );
        create_back_buffer( );
    }

    void device::begin_frame( const color& clr )
    {
        if ( m_window.resized( ) )
            resize( m_window.width( ), m_window.height( ) );

        f32 c [ 4 ] = { clr.r, clr.g, clr.b, clr.a };
        m_context->OMSetRenderTargets( 1, m_rtv.GetAddressOf( ), nullptr );
        m_context->ClearRenderTargetView( m_rtv.Get( ), c );

        D3D11_VIEWPORT vp = {};
        vp.Width = ( f32 ) m_width;
        vp.Height = ( f32 ) m_height;
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        m_context->RSSetViewports( 1, &vp );
    }

    void device::end_frame( )
    {
        m_swap_chain->Present( 1, 0 );
    }
}
