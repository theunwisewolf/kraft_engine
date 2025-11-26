#pragma once

#include "stb/stb_truetype.h"

#define KRAFT_MAX_GLYPHS_IN_ATLAS 512

namespace kraft {
namespace r {

struct FontGlyph
{
    f32 baseline_xoff_0;
    f32 baseline_yoff_0;
    f32 baseline_xoff_1;
    f32 baseline_yoff_1;
    f32 x_advance;
    f32 kern_advance;
    f32 u0;
    f32 v0;
    f32 u1;
    f32 v1;
};

struct FontAtlas
{
    // Handle to the bitmap texture
    Handle<Texture> bitmap;
    f32             width;
    f32             height;
    // The first character in the atlas
    u32 start_character;
    // Total characters in the atlas
    u32 num_chars;
    // Glyph information
    FontGlyph glyphs[KRAFT_MAX_GLYPHS_IN_ATLAS];
    // General font details
    f32 font_scale;
    f32 ascent;
    f32 descent;

    // Internal-use
    stbtt_fontinfo stb_font_info;
};

FontAtlas     LoadFontAtlas(ArenaAllocator* arena, u8* font_data, u64 font_data_size, f32 font_size);
r::Renderable RenderText(Material* material, FontAtlas* atlas, String8 text, Mat4f transform, Vec4f color);

} // namespace r
} // namespace kraft