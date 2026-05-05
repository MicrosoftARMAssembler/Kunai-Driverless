#include "image.h"
#include "device.h"

#include <wincodec.h>
#include <wrl/client.h>
#include <stdexcept>
#include <vector>

#pragma comment( lib, "windowscodecs.lib" )

namespace kunai
{
    static IWICImagingFactory* wic_factory( )
    {
        static ComPtr<IWICImagingFactory> factory;
        if ( !factory )
        {
            CoInitializeEx( nullptr, COINIT_MULTITHREADED );
            HRESULT hr = CoCreateInstance(
                CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS( factory.GetAddressOf( ) )
            );
            if ( FAILED( hr ) )
                throw std::runtime_error( "wic factory create failed" );
        }
        return factory.Get( );
    }

    image::image( device& dev, const u8* data, u64 size )
    {
        IWICImagingFactory* factory = wic_factory( );

        ComPtr<IWICStream> stream;
        factory->CreateStream( stream.GetAddressOf( ) );
        if ( FAILED( stream->InitializeFromMemory( const_cast< u8* >( data ), ( DWORD ) size ) ) )
            throw std::runtime_error( "wic stream init failed" );

        ComPtr<IWICBitmapDecoder> decoder;
        if ( FAILED( factory->CreateDecoderFromStream( stream.Get( ), nullptr, WICDecodeMetadataCacheOnLoad, decoder.GetAddressOf( ) ) ) )
            throw std::runtime_error( "wic decoder create failed" );

        ComPtr<IWICBitmapFrameDecode> frame;
        if ( FAILED( decoder->GetFrame( 0, frame.GetAddressOf( ) ) ) )
            throw std::runtime_error( "wic frame decode failed" );

        ComPtr<IWICFormatConverter> converter;
        factory->CreateFormatConverter( converter.GetAddressOf( ) );
        if ( FAILED( converter->Initialize(
            frame.Get( ), GUID_WICPixelFormat32bppRGBA,
            WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom ) ) )
            throw std::runtime_error( "wic convert failed" );

        UINT w = 0, h = 0;
        converter->GetSize( &w, &h );
        m_width = ( i32 ) w;
        m_height = ( i32 ) h;

        UINT stride = w * 4;
        UINT buffer_size = stride * h;
        std::vector<u8> pixels( buffer_size );
        if ( FAILED( converter->CopyPixels( nullptr, stride, buffer_size, pixels.data( ) ) ) )
            throw std::runtime_error( "wic copy pixels failed" );

        D3D11_TEXTURE2D_DESC td = {};
        td.Width = w;
        td.Height = h;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_IMMUTABLE;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA srd = {};
        srd.pSysMem = pixels.data( );
        srd.SysMemPitch = stride;

        if ( FAILED( dev.raw_device( )->CreateTexture2D( &td, &srd, m_texture.GetAddressOf( ) ) ) )
            throw std::runtime_error( "image texture create failed" );

        if ( FAILED( dev.raw_device( )->CreateShaderResourceView( m_texture.Get( ), nullptr, m_srv.GetAddressOf( ) ) ) )
            throw std::runtime_error( "image srv create failed" );
    }

    image::~image( ) = default;
}
