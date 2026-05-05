#pragma once

namespace kunai::shaders
{
    inline constexpr const char* quad_vs = R"(
cbuffer cb_projection : register(b0)
{
    float4x4 projection;
};

struct vs_input
{
    float2 position : POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

struct vs_output
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

vs_output main(vs_input input)
{
    vs_output output;
    output.position = mul(projection, float4(input.position, 0.0, 1.0));
    output.uv = input.uv;
    output.color = input.color;
    return output;
}
)";

    inline constexpr const char* alpha_ps = R"(
Texture2D atlas_texture : register(t0);
SamplerState atlas_sampler : register(s0);

struct ps_input
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

float4 main(ps_input input) : SV_TARGET
{
    float coverage = atlas_texture.Sample(atlas_sampler, input.uv).r;
    return float4(input.color.rgb, input.color.a * coverage);
}
)";

    inline constexpr const char* rgba_ps = R"(
Texture2D rgba_texture : register(t0);
SamplerState rgba_sampler : register(s0);

struct ps_input
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

float4 main(ps_input input) : SV_TARGET
{
    return rgba_texture.Sample(rgba_sampler, input.uv) * input.color;
}
)";
}
