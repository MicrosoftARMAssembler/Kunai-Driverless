#include "font.h"
#include "device.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdexcept>
#include <vector>

namespace kunai
{
    static constexpr i32 k_atlas_width = 1024;
    static constexpr i32 k_atlas_padding = 2;
    static constexpr i32 k_white_block = 4;

    font::font( device& dev, const u8* ttf_data, u64 ttf_size, i32 pixel_size,
                std::initializer_list< codepoint_range > ranges )
        : m_pixel_size( pixel_size )
    {
        FT_Library ft = nullptr;
        if ( FT_Init_FreeType( &ft ) )
            throw std::runtime_error( "freetype init failed" );

        FT_Face face = nullptr;
        if ( FT_New_Memory_Face( ft, ttf_data, ( FT_Long ) ttf_size, 0, &face ) )
        {
            FT_Done_FreeType( ft );
            throw std::runtime_error( "failed to load embedded font" );
        }

        FT_Set_Pixel_Sizes( face, 0, pixel_size );

        m_ascent = ( f32 ) ( face->size->metrics.ascender >> 6 );
        m_descent = ( f32 ) ( face->size->metrics.descender >> 6 );
        m_line_height = ( f32 ) ( face->size->metrics.height >> 6 );

        std::vector<u8> atlas( k_atlas_width * 1024, 0 );
        auto ensure_rows = [ & ] ( i32 rows_needed ) {
            if ( rows_needed * k_atlas_width > ( i32 ) atlas.size( ) )
                atlas.resize( ( rows_needed + 128 ) * k_atlas_width, 0 );
            };

        for ( i32 y = 0; y < k_white_block; ++y )
            for ( i32 x = 0; x < k_white_block; ++x )
                atlas[ y * k_atlas_width + x ] = 255;

        i32 pen_x = k_white_block + k_atlas_padding;
        i32 pen_y = 0;
        i32 row_height = k_white_block;
        i32 max_y = k_white_block;

        for ( const codepoint_range& range : ranges )
        {
            for ( u32 cp = range.first; cp <= range.last; ++cp )
            {
                if ( FT_Load_Char( face, cp, FT_LOAD_RENDER ) )
                    continue;

                FT_Bitmap& bmp = face->glyph->bitmap;
                i32 w = ( i32 ) bmp.width;
                i32 h = ( i32 ) bmp.rows;

                if ( pen_x + w + k_atlas_padding >= k_atlas_width )
                {
                    pen_x = k_atlas_padding;
                    pen_y += row_height + k_atlas_padding;
                    row_height = 0;
                }

                ensure_rows( pen_y + h + k_atlas_padding );

                for ( i32 y = 0; y < h; ++y )
                    for ( i32 x = 0; x < w; ++x )
                        atlas[ ( pen_y + y ) * k_atlas_width + ( pen_x + x ) ] = bmp.buffer[ y * bmp.pitch + x ];

                glyph g;
                g.u0 = ( f32 ) pen_x / ( f32 ) k_atlas_width;
                g.v0 = ( f32 ) pen_y;
                g.u1 = ( f32 ) ( pen_x + w ) / ( f32 ) k_atlas_width;
                g.v1 = ( f32 ) ( pen_y + h );
                g.width = ( f32 ) w;
                g.height = ( f32 ) h;
                g.bearing_x = ( f32 ) face->glyph->bitmap_left;
                g.bearing_y = ( f32 ) face->glyph->bitmap_top;
                g.advance = ( f32 ) ( face->glyph->advance.x >> 6 );

                m_glyphs[ cp ] = g;

                pen_x += w + k_atlas_padding;
                if ( h > row_height ) row_height = h;
                if ( pen_y + h > max_y ) max_y = pen_y + h;
            }
        }

        FT_Done_Face( face );
        FT_Done_FreeType( ft );

        i32 atlas_height = max_y + k_atlas_padding;
        if ( atlas_height < 8 ) atlas_height = 8;

        f32 inv_h = 1.0f / ( f32 ) atlas_height;
        for ( auto& kv : m_glyphs )
        {
            kv.second.v0 *= inv_h;
            kv.second.v1 *= inv_h;
        }

        m_white_uv.x = 1.0f / ( f32 ) k_atlas_width;
        m_white_uv.y = 1.0f / ( f32 ) atlas_height;

        D3D11_TEXTURE2D_DESC td = {};
        td.Width = k_atlas_width;
        td.Height = atlas_height;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_IMMUTABLE;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA srd = {};
        srd.pSysMem = atlas.data( );
        srd.SysMemPitch = k_atlas_width;

        HRESULT hr = dev.raw_device( )->CreateTexture2D( &td, &srd, m_texture.GetAddressOf( ) );
        if ( FAILED( hr ) )
            throw std::runtime_error( "font atlas texture create failed" );

        hr = dev.raw_device( )->CreateShaderResourceView( m_texture.Get( ), nullptr, m_srv.GetAddressOf( ) );
        if ( FAILED( hr ) )
            throw std::runtime_error( "font atlas srv create failed" );
    }

    font::~font( ) = default;

    const glyph* font::find( u32 codepoint ) const
    {
        auto it = m_glyphs.find( codepoint );
        if ( it == m_glyphs.end( ) ) return nullptr;
        return &it->second;
    }
}
