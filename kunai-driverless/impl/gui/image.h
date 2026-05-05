#pragma once

#include "types.h"
#include <d3d11.h>
#include <wrl/client.h>

namespace kunai
{
    using Microsoft::WRL::ComPtr;

    class device;

    class image
    {
    public:
        image( device& dev, const u8* data, u64 size );
        ~image( );

        image( const image& ) = delete;
        image& operator=( const image& ) = delete;

        ID3D11ShaderResourceView* srv( ) const { return m_srv.Get( ); }
        i32 width( ) const { return m_width; }
        i32 height( ) const { return m_height; }
        f32 aspect( ) const { return m_height > 0 ? ( f32 ) m_width / ( f32 ) m_height : 1.0f; }

    private:
        ComPtr<ID3D11Texture2D> m_texture;
        ComPtr<ID3D11ShaderResourceView> m_srv;
        i32 m_width = 0;
        i32 m_height = 0;
    };
}
