#pragma once

#include <cstdint>

namespace kunai
{
    using u8 = uint8_t;
    using u16 = uint16_t;
    using u32 = uint32_t;
    using u64 = uint64_t;
    using i8 = int8_t;
    using i16 = int16_t;
    using i32 = int32_t;
    using i64 = int64_t;
    using f32 = float;
    using f64 = double;

    struct vec2
    {
        f32 x = 0.0f;
        f32 y = 0.0f;
    };

    struct rect
    {
        f32 x = 0.0f;
        f32 y = 0.0f;
        f32 w = 0.0f;
        f32 h = 0.0f;
    };

    struct color
    {
        f32 r = 0.0f;
        f32 g = 0.0f;
        f32 b = 0.0f;
        f32 a = 1.0f;
    };

    static u32 screen_w = 0;
    static u32 screen_h = 0;

    static u32 center_w = 0;
    static u32 center_h = 0;
}
