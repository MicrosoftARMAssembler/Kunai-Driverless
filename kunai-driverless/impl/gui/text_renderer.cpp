#include "text_renderer.h"
#include "device.h"
#include "font.h"
#include "image.h"
#include "shaders.h"
#include <d3dcompiler.h>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>

namespace kunai
{
    static constexpr size_t k_initial_quads = 1024;

    struct projection_cb
    {
        f32 matrix[ 16 ];
    };

    static ComPtr<ID3DBlob> compile_shader( const char* source, const char* target, const char* stage_name )
    {
        UINT flags = 0;
#if defined(_DEBUG)
        flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

        ComPtr<ID3DBlob> blob;
        ComPtr<ID3DBlob> err;
        HRESULT hr = D3DCompile(
            source, std::strlen( source ),
            nullptr, nullptr, nullptr, "main", target,
            flags, 0, blob.GetAddressOf( ), err.GetAddressOf( )
        );
        if ( FAILED( hr ) )
        {
            std::string msg = std::string( stage_name ) + " compile failed";
            if ( err )
                msg += std::string( ": " ) + static_cast< const char* >( err->GetBufferPointer( ) );
            throw std::runtime_error( msg );
        }
        return blob;
    }

    text_renderer::text_renderer( device& dev, font& fnt )
        : m_device( dev ), m_font( fnt )
    {
        create_resources( );
    }

    text_renderer::~text_renderer( ) = default;

    void text_renderer::create_resources( )
    {
        ID3D11Device* d = m_device.raw_device( );

        ComPtr<ID3DBlob> vs_blob = compile_shader( shaders::quad_vs, "vs_5_0", "vertex shader" );
        ComPtr<ID3DBlob> ps_alpha_blob = compile_shader( shaders::alpha_ps, "ps_5_0", "alpha pixel shader" );
        ComPtr<ID3DBlob> ps_rgba_blob = compile_shader( shaders::rgba_ps, "ps_5_0", "rgba pixel shader" );

        d->CreateVertexShader( vs_blob->GetBufferPointer( ), vs_blob->GetBufferSize( ), nullptr, m_vs.GetAddressOf( ) );
        d->CreatePixelShader( ps_alpha_blob->GetBufferPointer( ), ps_alpha_blob->GetBufferSize( ), nullptr, m_ps_alpha.GetAddressOf( ) );
        d->CreatePixelShader( ps_rgba_blob->GetBufferPointer( ), ps_rgba_blob->GetBufferSize( ), nullptr, m_ps_rgba.GetAddressOf( ) );

        D3D11_INPUT_ELEMENT_DESC elements[ ] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,       0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0,  8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        d->CreateInputLayout(
            elements, ( UINT ) ( sizeof( elements ) / sizeof( elements[ 0 ] ) ),
            vs_blob->GetBufferPointer( ), vs_blob->GetBufferSize( ),
            m_layout.GetAddressOf( )
        );

        D3D11_BUFFER_DESC cbd = {};
        cbd.Usage = D3D11_USAGE_DYNAMIC;
        cbd.ByteWidth = sizeof( projection_cb );
        cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        d->CreateBuffer( &cbd, nullptr, m_cb.GetAddressOf( ) );

        D3D11_BLEND_DESC bd = {};
        bd.RenderTarget[ 0 ].BlendEnable = TRUE;
        bd.RenderTarget[ 0 ].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        bd.RenderTarget[ 0 ].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        bd.RenderTarget[ 0 ].BlendOp = D3D11_BLEND_OP_ADD;
        bd.RenderTarget[ 0 ].SrcBlendAlpha = D3D11_BLEND_ONE;
        bd.RenderTarget[ 0 ].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        bd.RenderTarget[ 0 ].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        bd.RenderTarget[ 0 ].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        d->CreateBlendState( &bd, m_blend.GetAddressOf( ) );

        D3D11_RASTERIZER_DESC rd = {};
        rd.FillMode = D3D11_FILL_SOLID;
        rd.CullMode = D3D11_CULL_NONE;
        rd.ScissorEnable = FALSE;
        rd.DepthClipEnable = TRUE;
        d->CreateRasterizerState( &rd, m_raster.GetAddressOf( ) );

        D3D11_SAMPLER_DESC sd = {};
        sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.MaxLOD = D3D11_FLOAT32_MAX;
        d->CreateSamplerState( &sd, m_sampler.GetAddressOf( ) );

        D3D11_DEPTH_STENCIL_DESC dsd = {};
        dsd.DepthEnable = FALSE;
        dsd.StencilEnable = FALSE;
        d->CreateDepthStencilState( &dsd, m_depth.GetAddressOf( ) );

        grow_vertex_buffer( k_initial_quads * 4 );
    }

    void text_renderer::grow_vertex_buffer( size_t vert_count )
    {
        ID3D11Device* d = m_device.raw_device( );
        size_t quad_count = ( vert_count + 3 ) / 4;
        size_t idx_count = quad_count * 6;

        D3D11_BUFFER_DESC vbd = {};
        vbd.Usage = D3D11_USAGE_DYNAMIC;
        vbd.ByteWidth = ( UINT ) ( quad_count * 4 * sizeof( vertex ) );
        vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        m_vb.Reset( );
        d->CreateBuffer( &vbd, nullptr, m_vb.GetAddressOf( ) );
        m_vb_capacity = quad_count * 4;

        std::vector<u32> indices( idx_count );
        for ( size_t q = 0; q < quad_count; ++q )
        {
            u32 base = ( u32 ) ( q * 4 );
            indices[ q * 6 + 0 ] = base + 0;
            indices[ q * 6 + 1 ] = base + 1;
            indices[ q * 6 + 2 ] = base + 2;
            indices[ q * 6 + 3 ] = base + 0;
            indices[ q * 6 + 4 ] = base + 2;
            indices[ q * 6 + 5 ] = base + 3;
        }

        D3D11_BUFFER_DESC ibd = {};
        ibd.Usage = D3D11_USAGE_IMMUTABLE;
        ibd.ByteWidth = ( UINT ) ( idx_count * sizeof( u32 ) );
        ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;

        D3D11_SUBRESOURCE_DATA isrd = {};
        isrd.pSysMem = indices.data( );

        m_ib.Reset( );
        d->CreateBuffer( &ibd, &isrd, m_ib.GetAddressOf( ) );
        m_ib_capacity = idx_count;
    }

    void text_renderer::set_ui_scale( f32 scale )
    {
        if ( scale <= 0.0f ) scale = 1.0f;
        if ( scale == m_ui_scale ) return;
        m_ui_scale = scale;
        m_last_projection_w = -1;
        m_last_projection_h = -1;
    }

    void text_renderer::update_projection( )
    {
        i32 w = m_device.width( );
        i32 h = m_device.height( );
        if ( w == m_last_projection_w && h == m_last_projection_h ) return;

        m_last_projection_w = w;
        m_last_projection_h = h;

        projection_cb data = {};
        f32 inv_scale = 1.0f / m_ui_scale;
        f32 l = 0.0f, r = ( f32 ) w * inv_scale, t = 0.0f, b = ( f32 ) h * inv_scale;

        data.matrix[  0 ] = 2.0f / ( r - l );
        data.matrix[  5 ] = 2.0f / ( t - b );
        data.matrix[ 10 ] = 1.0f;
        data.matrix[ 12 ] = ( l + r ) / ( l - r );
        data.matrix[ 13 ] = ( t + b ) / ( b - t );
        data.matrix[ 14 ] = 0.0f;
        data.matrix[ 15 ] = 1.0f;

        D3D11_MAPPED_SUBRESOURCE map = {};
        m_device.context( )->Map( m_cb.Get( ), 0, D3D11_MAP_WRITE_DISCARD, 0, &map );
        std::memcpy( map.pData, &data, sizeof( data ) );
        m_device.context( )->Unmap( m_cb.Get( ), 0 );
    }

    text_renderer::batch& text_renderer::current_batch( ID3D11ShaderResourceView* srv, shader_mode mode )
    {
        if ( !m_batches.empty( ) )
        {
            batch& last = m_batches.back( );
            if ( last.srv == srv && last.mode == mode )
                return last;
        }
        batch b = {};
        b.start = ( u32 ) m_vertices.size( );
        b.count = 0;
        b.srv = srv;
        b.mode = mode;
        m_batches.push_back( b );
        return m_batches.back( );
    }

    void text_renderer::push_quad( f32 x0, f32 y0, f32 x1, f32 y1, f32 u0, f32 v0, f32 u1, f32 v1, const color& c )
    {
        f32 a = c.a * m_alpha_mul;
        m_vertices.push_back( { { x0, y0 }, { u0, v0 }, { c.r, c.g, c.b, a } } );
        m_vertices.push_back( { { x1, y0 }, { u1, v0 }, { c.r, c.g, c.b, a } } );
        m_vertices.push_back( { { x1, y1 }, { u1, v1 }, { c.r, c.g, c.b, a } } );
        m_vertices.push_back( { { x0, y1 }, { u0, v1 }, { c.r, c.g, c.b, a } } );
        m_batches.back( ).count += 4;
    }

    void text_renderer::draw_text( std::string_view text, f32 x, f32 y, const color& col )
    {
        draw_text( m_font, text, x, y, col );
    }

    void text_renderer::draw_text( font& f, std::string_view text, f32 x, f32 y, const color& col )
    {
        current_batch( f.srv( ), shader_mode::alpha );

        f32 pen_x = std::floor( x + 0.5f );
        f32 pen_y = std::floor( y + 0.5f ) + f.ascent( );

        for ( char ch : text )
        {
            if ( ch == '\n' )
            {
                pen_x = x;
                pen_y += f.line_height( );
                continue;
            }
            if ( ch == '\r' ) continue;

            const glyph* g = f.find( ( u32 ) ( unsigned char ) ch );
            if ( !g )
            {
                g = f.find( ( u32 ) '?' );
                if ( !g )
                {
                    pen_x += ( f32 ) f.pixel_size( ) * 0.5f;
                    continue;
                }
            }

            if ( g->width > 0.0f && g->height > 0.0f )
            {
                f32 x0 = pen_x + g->bearing_x;
                f32 y0 = pen_y - g->bearing_y;
                f32 x1 = x0 + g->width;
                f32 y1 = y0 + g->height;
                push_quad( x0, y0, x1, y1, g->u0, g->v0, g->u1, g->v1, col );
            }

            pen_x += g->advance;
        }
    }

    void text_renderer::draw_codepoint( font& f, u32 codepoint, f32 x, f32 y, const color& col )
    {
        const glyph* g = f.find( codepoint );
        if ( !g || g->width <= 0.0f || g->height <= 0.0f ) return;

        current_batch( f.srv( ), shader_mode::alpha );

        f32 rx = std::floor( x + 0.5f );
        f32 pen_y = std::floor( y + 0.5f ) + f.ascent( );
        f32 x0 = rx + g->bearing_x;
        f32 y0 = pen_y - g->bearing_y;
        f32 x1 = x0 + g->width;
        f32 y1 = y0 + g->height;
        push_quad( x0, y0, x1, y1, g->u0, g->v0, g->u1, g->v1, col );
    }

    void text_renderer::draw_line( vec2 a, vec2 b, const color& col, f32 thickness ) {
        // snap endpoints to pixel grid
        a.x = std::floor( a.x + 0.5f );
        a.y = std::floor( a.y + 0.5f );
        b.x = std::floor( b.x + 0.5f );
        b.y = std::floor( b.y + 0.5f );

        float dx = b.x - a.x;
        float dy = b.y - a.y;
        float len = std::sqrt( dx * dx + dy * dy );
        if ( len < 0.0001f ) return;

        // snap thickness to nearest pixel
        float t = std::floor( thickness + 0.5f );
        if ( t < 1.0f ) t = 1.0f;

        float nx = -dy / len * ( t * 0.5f );
        float ny = dx / len * ( t * 0.5f );

        // snap the extruded corners too
        auto snap = [ ]( float v ) { return std::floor( v + 0.5f ); };

        current_batch( m_font.srv( ), shader_mode::alpha );
        vec2 uv = m_font.white_uv( );

        float a_val = col.a * m_alpha_mul;
        m_vertices.push_back( { { snap( a.x + nx ), snap( a.y + ny ) }, { uv.x, uv.y }, { col.r, col.g, col.b, a_val } } );
        m_vertices.push_back( { { snap( b.x + nx ), snap( b.y + ny ) }, { uv.x, uv.y }, { col.r, col.g, col.b, a_val } } );
        m_vertices.push_back( { { snap( b.x - nx ), snap( b.y - ny ) }, { uv.x, uv.y }, { col.r, col.g, col.b, a_val } } );
        m_vertices.push_back( { { snap( a.x - nx ), snap( a.y - ny ) }, { uv.x, uv.y }, { col.r, col.g, col.b, a_val } } );
        m_batches.back( ).count += 4;
    }

    void text_renderer::draw_rect( const rect& r, const color& col )
    {
        current_batch( m_font.srv( ), shader_mode::alpha );
        vec2 uv = m_font.white_uv( );
        push_quad( r.x, r.y, r.x + r.w, r.y + r.h, uv.x, uv.y, uv.x, uv.y, col );
    }

    void text_renderer::draw_rect_gradient_h( const rect& r, const color& left, const color& right )
    {
        current_batch( m_font.srv( ), shader_mode::alpha );
        vec2 uv = m_font.white_uv( );
        f32 la = left.a * m_alpha_mul;
        f32 ra = right.a * m_alpha_mul;
        m_vertices.push_back( { { r.x,         r.y         }, { uv.x, uv.y }, { left.r,  left.g,  left.b,  la } } );
        m_vertices.push_back( { { r.x + r.w,   r.y         }, { uv.x, uv.y }, { right.r, right.g, right.b, ra } } );
        m_vertices.push_back( { { r.x + r.w,   r.y + r.h   }, { uv.x, uv.y }, { right.r, right.g, right.b, ra } } );
        m_vertices.push_back( { { r.x,         r.y + r.h   }, { uv.x, uv.y }, { left.r,  left.g,  left.b,  la } } );
        m_batches.back( ).count += 4;
    }

    void text_renderer::draw_rect_gradient_v( const rect& r, const color& top, const color& bottom ) {
        current_batch( m_font.srv( ), shader_mode::alpha );
        vec2 uv = m_font.white_uv( );
        f32 ta = top.a * m_alpha_mul;
        f32 ba = bottom.a * m_alpha_mul;
        m_vertices.push_back( { { r.x,       r.y       }, { uv.x, uv.y }, { top.r,    top.g,    top.b,    ta } } );
        m_vertices.push_back( { { r.x + r.w, r.y       }, { uv.x, uv.y }, { top.r,    top.g,    top.b,    ta } } );
        m_vertices.push_back( { { r.x + r.w, r.y + r.h }, { uv.x, uv.y }, { bottom.r, bottom.g, bottom.b, ba } } );
        m_vertices.push_back( { { r.x,       r.y + r.h }, { uv.x, uv.y }, { bottom.r, bottom.g, bottom.b, ba } } );
        m_batches.back( ).count += 4;
    }

    void text_renderer::draw_text_clipped( font& f, std::string_view text, f32 x, f32 y, f32 clip_x1, const color& col, const color& fade_bg, f32 fade_width )
    {
        current_batch( f.srv( ), shader_mode::alpha );
        f32 pen_x = std::floor( x + 0.5f );
        f32 pen_y = std::floor( y + 0.5f ) + f.ascent( );

        for ( char ch : text )
        {
            if ( ch == '\r' || ch == '\n' ) continue;
            const glyph* g = f.find( ( u32 ) ( unsigned char ) ch );
            if ( !g )
            {
                g = f.find( ( u32 ) '?' );
                if ( !g ) { pen_x += ( f32 ) f.pixel_size( ) * 0.5f; continue; }
            }
            if ( pen_x > clip_x1 ) break;

            if ( g->width > 0.0f && g->height > 0.0f )
            {
                f32 x0 = pen_x + g->bearing_x;
                f32 y0 = pen_y - g->bearing_y;
                f32 x1 = x0 + g->width;
                f32 y1 = y0 + g->height;
                push_quad( x0, y0, x1, y1, g->u0, g->v0, g->u1, g->v1, col );
            }
            pen_x += g->advance;
        }

        if ( pen_x > clip_x1 - fade_width )
        {
            f32 fw = fade_width;
            f32 fx = clip_x1 - fw;
            color left_c = { fade_bg.r, fade_bg.g, fade_bg.b, 0.0f };
            color right_c = { fade_bg.r, fade_bg.g, fade_bg.b, fade_bg.a };
            draw_rect_gradient_h( { fx, y, fw, f.line_height( ) }, left_c, right_c );
        }
    }

    f32 text_renderer::advance_to_index( font& f, std::string_view text, i32 index ) const
    {
        f32 x = 0.0f;
        if ( index < 0 ) index = 0;
        if ( index > ( i32 ) text.size( ) ) index = ( i32 ) text.size( );
        for ( i32 i = 0; i < index; ++i )
        {
            const glyph* g = f.find( ( u32 ) ( unsigned char ) text[ i ] );
            if ( g ) x += g->advance;
        }
        return x;
    }

    i32 text_renderer::index_from_x( font& f, std::string_view text, f32 x_rel ) const
    {
        f32 x = 0.0f;
        for ( i32 i = 0; i < ( i32 ) text.size( ); ++i )
        {
            const glyph* g = f.find( ( u32 ) ( unsigned char ) text[ i ] );
            if ( !g ) continue;
            f32 half = g->advance * 0.5f;
            if ( x_rel < x + half ) return i;
            x += g->advance;
        }
        return ( i32 ) text.size( );
    }

    void text_renderer::draw_rect_outline( const rect& r, f32 thickness, const color& col )
    {
        draw_rect( { r.x, r.y, r.w, thickness }, col );
        draw_rect( { r.x, r.y + r.h - thickness, r.w, thickness }, col );
        draw_rect( { r.x, r.y + thickness, thickness, r.h - thickness * 2.0f }, col );
        draw_rect( { r.x + r.w - thickness, r.y + thickness, thickness, r.h - thickness * 2.0f }, col );
    }

    void text_renderer::draw_image( const image& img, const rect& dst, const color& tint )
    {
        current_batch( img.srv( ), shader_mode::rgba );
        push_quad( dst.x, dst.y, dst.x + dst.w, dst.y + dst.h, 0.0f, 0.0f, 1.0f, 1.0f, tint );
    }

    vec2 text_renderer::measure_text( std::string_view text ) const
    {
        return measure_text( m_font, text );
    }

    vec2 text_renderer::measure_text( const font& f, std::string_view text ) const
    {
        f32 max_w = 0.0f;
        f32 w = 0.0f;
        f32 h = text.empty( ) ? 0.0f : f.line_height( );

        for ( char ch : text )
        {
            if ( ch == '\n' )
            {
                if ( w > max_w ) max_w = w;
                w = 0.0f;
                h += f.line_height( );
                continue;
            }
            if ( ch == '\r' ) continue;
            const glyph* g = f.find( ( u32 ) ( unsigned char ) ch );
            if ( g ) w += g->advance;
        }
        if ( w > max_w ) max_w = w;
        return { max_w, h };
    }

    void text_renderer::draw_circle_filled( vec2 center, f32 radius, const color& col, int segments ) {
        if ( segments < 3 ) segments = 3;
        current_batch( m_font.srv( ), shader_mode::alpha );
        vec2 uv = m_font.white_uv( );
        f32 a_val = col.a * m_alpha_mul;

        // fan triangulation — each triangle is (center, p[i], p[i+1])
        // pad to quads: emit as degenerate quads where v0==v3 (center repeated)
        for ( int i = 0; i < segments; ++i ) {
            float a0 = ( float )i / ( float )segments * 6.28318530f;
            float a1 = ( float )( i + 1 ) / ( float )segments * 6.28318530f;

            float x0 = center.x + std::cos( a0 ) * radius;
            float y0 = center.y + std::sin( a0 ) * radius;
            float x1 = center.x + std::cos( a1 ) * radius;
            float y1 = center.y + std::sin( a1 ) * radius;

            // emit as quad: center, p0, p1, center (degenerate — two tris share center)
            m_vertices.push_back( { { center.x, center.y }, { uv.x, uv.y }, { col.r, col.g, col.b, a_val } } );
            m_vertices.push_back( { { x0,       y0       }, { uv.x, uv.y }, { col.r, col.g, col.b, a_val } } );
            m_vertices.push_back( { { x1,       y1       }, { uv.x, uv.y }, { col.r, col.g, col.b, a_val } } );
            m_vertices.push_back( { { center.x, center.y }, { uv.x, uv.y }, { col.r, col.g, col.b, a_val } } );
            m_batches.back( ).count += 4;
        }
    }

    void text_renderer::draw_circle( vec2 center, f32 radius, const color& col, int segments, f32 thickness ) {
        if ( segments < 3 ) segments = 3;
        for ( int i = 0; i < segments; ++i ) {
            float a0 = ( float )i / ( float )segments * 6.28318530f;
            float a1 = ( float )( i + 1 ) / ( float )segments * 6.28318530f;
            vec2 p0 = { center.x + std::cos( a0 ) * radius, center.y + std::sin( a0 ) * radius };
            vec2 p1 = { center.x + std::cos( a1 ) * radius, center.y + std::sin( a1 ) * radius };
            draw_line( p0, p1, col, thickness );
        }
    }

    void text_renderer::flush( )
    {
        if ( m_vertices.empty( ) || m_batches.empty( ) )
        {
            m_vertices.clear( );
            m_batches.clear( );
            return;
        }

        if ( m_vertices.size( ) > m_vb_capacity )
        {
            size_t new_verts = m_vertices.size( ) * 2;
            grow_vertex_buffer( new_verts );
        }

        update_projection( );

        ID3D11DeviceContext* ctx = m_device.context( );

        D3D11_MAPPED_SUBRESOURCE map = {};
        ctx->Map( m_vb.Get( ), 0, D3D11_MAP_WRITE_DISCARD, 0, &map );
        std::memcpy( map.pData, m_vertices.data( ), m_vertices.size( ) * sizeof( vertex ) );
        ctx->Unmap( m_vb.Get( ), 0 );

        UINT stride = sizeof( vertex );
        UINT offset = 0;

        ctx->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
        ctx->IASetInputLayout( m_layout.Get( ) );
        ctx->IASetVertexBuffers( 0, 1, m_vb.GetAddressOf( ), &stride, &offset );
        ctx->IASetIndexBuffer( m_ib.Get( ), DXGI_FORMAT_R32_UINT, 0 );
        ctx->VSSetShader( m_vs.Get( ), nullptr, 0 );
        ctx->VSSetConstantBuffers( 0, 1, m_cb.GetAddressOf( ) );
        ctx->PSSetSamplers( 0, 1, m_sampler.GetAddressOf( ) );

        f32 blend_factor[ 4 ] = { 0.0f, 0.0f, 0.0f, 0.0f };
        ctx->OMSetBlendState( m_blend.Get( ), blend_factor, 0xffffffff );
        ctx->OMSetDepthStencilState( m_depth.Get( ), 0 );
        ctx->RSSetState( m_raster.Get( ) );

        shader_mode bound_mode = shader_mode::alpha;
        ID3D11ShaderResourceView* bound_srv = nullptr;
        ctx->PSSetShader( m_ps_alpha.Get( ), nullptr, 0 );

        for ( const batch& b : m_batches )
        {
            if ( b.count == 0 ) continue;

            if ( b.mode != bound_mode )
            {
                ctx->PSSetShader( b.mode == shader_mode::rgba ? m_ps_rgba.Get( ) : m_ps_alpha.Get( ), nullptr, 0 );
                bound_mode = b.mode;
            }
            if ( b.srv != bound_srv )
            {
                ID3D11ShaderResourceView* srv = b.srv;
                ctx->PSSetShaderResources( 0, 1, &srv );
                bound_srv = b.srv;
            }

            u32 start_quad = b.start / 4;
            u32 quad_count = b.count / 4;
            ctx->DrawIndexed( quad_count * 6, start_quad * 6, 0 );
        }

        m_vertices.clear( );
        m_batches.clear( );
    }
}
