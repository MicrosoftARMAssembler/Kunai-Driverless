#pragma once

#include "types.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <string_view>
#include <vector>

namespace kunai
{
    using Microsoft::WRL::ComPtr;

    class device;
    class font;
    class image;

    class text_renderer
    {
    public:
        text_renderer( device& dev, font& fnt );
        ~text_renderer( );

        text_renderer( const text_renderer& ) = delete;
        text_renderer& operator=( const text_renderer& ) = delete;

        void draw_text( std::string_view text, f32 x, f32 y, const color& col );
        void draw_text( font& f, std::string_view text, f32 x, f32 y, const color& col );
        void draw_text_clipped( font& f, std::string_view text, f32 x, f32 y, f32 clip_x1, const color& col, const color& fade_bg, f32 fade_width = 16.0f );
        void draw_codepoint( font& f, u32 codepoint, f32 x, f32 y, const color& col );
        void draw_rect( const rect& r, const color& col );
        void draw_rect_gradient_h( const rect& r, const color& left, const color& right );
        void draw_rect_gradient_v( const rect& r, const color& top, const color& bottom );
        void draw_rect_outline( const rect& r, f32 thickness, const color& col );
        void draw_line( vec2 a, vec2 b, const color& col, f32 thickness );
        void draw_image( const image& img, const rect& dst, const color& tint = { 1.0f, 1.0f, 1.0f, 1.0f } );
        void flush( );
        void draw_circle_filled( vec2 center, f32 radius, const color& col, int segments );
        void draw_circle( vec2 center, f32 radius, const color& col, int segments, f32 thickness );

        f32 advance_to_index( font& f, std::string_view text, i32 index ) const;
        i32 index_from_x( font& f, std::string_view text, f32 x_rel ) const;

        void set_alpha( f32 a ) { m_alpha_mul = a; }
        f32 alpha( ) const { return m_alpha_mul; }

        void set_ui_scale( f32 scale );
        f32 ui_scale( ) const { return m_ui_scale; }

        vec2 measure_text( std::string_view text ) const;
        vec2 measure_text( const font& f, std::string_view text ) const;

    private:
        enum class shader_mode
        {
            alpha,
            rgba,
        };

        struct vertex
        {
            f32 pos[ 2 ];
            f32 uv[ 2 ];
            f32 col[ 4 ];
        };

        struct batch
        {
            u32 start = 0;
            u32 count = 0;
            ID3D11ShaderResourceView* srv = nullptr;
            shader_mode mode = shader_mode::alpha;
        };

        void create_resources( );
        void grow_vertex_buffer( size_t vert_count );
        void update_projection( );
        void push_quad( f32 x0, f32 y0, f32 x1, f32 y1, f32 u0, f32 v0, f32 u1, f32 v1, const color& c );
        batch& current_batch( ID3D11ShaderResourceView* srv, shader_mode mode );

        device& m_device;
        font& m_font;

        std::vector<vertex> m_vertices;
        std::vector<batch> m_batches;

        ComPtr<ID3D11Buffer> m_vb;
        ComPtr<ID3D11Buffer> m_ib;
        ComPtr<ID3D11Buffer> m_cb;
        ComPtr<ID3D11VertexShader> m_vs;
        ComPtr<ID3D11PixelShader> m_ps_alpha;
        ComPtr<ID3D11PixelShader> m_ps_rgba;
        ComPtr<ID3D11InputLayout> m_layout;
        ComPtr<ID3D11BlendState> m_blend;
        ComPtr<ID3D11RasterizerState> m_raster;
        ComPtr<ID3D11SamplerState> m_sampler;
        ComPtr<ID3D11DepthStencilState> m_depth;

        size_t m_vb_capacity = 0;
        size_t m_ib_capacity = 0;

        i32 m_last_projection_w = -1;
        i32 m_last_projection_h = -1;

        f32 m_alpha_mul = 1.0f;
        f32 m_ui_scale = 1.0f;
    };
}
