#pragma once

#include "types.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <initializer_list>
#include <unordered_map>

namespace kunai
{
    using Microsoft::WRL::ComPtr;

    class device;

    struct glyph
    {
        f32 u0 = 0.0f;
        f32 v0 = 0.0f;
        f32 u1 = 0.0f;
        f32 v1 = 0.0f;
        f32 width = 0.0f;
        f32 height = 0.0f;
        f32 bearing_x = 0.0f;
        f32 bearing_y = 0.0f;
        f32 advance = 0.0f;
    };

    struct codepoint_range
    {
        u32 first;
        u32 last;
    };

    class font
    {
    public:
        font( device& dev, const u8* ttf_data, u64 ttf_size, i32 pixel_size,
              std::initializer_list< codepoint_range > ranges = { { 32, 126 } } );
        ~font( );

        font( const font& ) = delete;
        font& operator=( const font& ) = delete;

        const glyph* find( u32 codepoint ) const;
        f32 line_height( ) const { return m_line_height; }
        f32 ascent( ) const { return m_ascent; }
        f32 descent( ) const { return m_descent; }
        i32 pixel_size( ) const { return m_pixel_size; }
        ID3D11ShaderResourceView* srv( ) const { return m_srv.Get( ); }
        vec2 white_uv( ) const { return m_white_uv; }

    private:
        std::unordered_map<u32, glyph> m_glyphs;
        ComPtr<ID3D11Texture2D> m_texture;
        ComPtr<ID3D11ShaderResourceView> m_srv;
        vec2 m_white_uv;
        f32 m_line_height = 0.0f;
        f32 m_ascent = 0.0f;
        f32 m_descent = 0.0f;
        i32 m_pixel_size = 0;
    };
}
