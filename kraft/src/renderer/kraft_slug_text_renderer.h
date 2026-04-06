#pragma once

#include "stb/stb_truetype.h"

#define KRAFT_SLUG_MAX_GLYPHS 4096
#define KRAFT_SLUG_DEFAULT_BANDS 8
#define KRAFT_SLUG_MAX_TOTAL_CURVES 131072
#define KRAFT_SLUG_MAX_TOTAL_CURVE_VEC4S (KRAFT_SLUG_MAX_TOTAL_CURVES * 2)
#define KRAFT_SLUG_MAX_TOTAL_BAND_UINTS 524288

namespace kraft {
namespace r {

struct SlugGlyph {
    // Bounding box in em-space (font units / units_per_em)
    f32 em_x0, em_y0, em_x1, em_y1;
    // Horizontal advance in em-space
    f32 x_advance;
    // Offset into curve SSBO (in vec4 units, each curve = 2 vec4s)
    u32 curve_data_offset;
    // Total number of quadratic bezier curves for this glyph
    u32 curve_count;
    // Offset into band SSBO (in uint units)
    u32 band_data_offset;
    // Number of horizontal and vertical bands
    u32 h_band_count;
    u32 v_band_count;
    bool is_supported;
};

struct SlugFont {
    stbtt_fontinfo stb_font_info;
    f32 em_scale; // 1.0 / units_per_em
    f32 ascent; // in em-space
    f32 descent; // in em-space (negative)
    u32 start_character;
    u32 num_chars;
    u32 glyph_count;
    u32 codepoints[KRAFT_SLUG_MAX_GLYPHS];
    const u8* source_font_data[KRAFT_SLUG_MAX_GLYPHS];
    SlugGlyph glyphs[KRAFT_SLUG_MAX_GLYPHS];
    u32 curve_capacity_vec4s;
    u32 total_curve_vec4s;
    u32 band_capacity_uints;
    u32 total_band_uints;
    f32* curve_data;
    u32* band_data;

    Handle<Buffer> curve_ssbo;
    Handle<Buffer> band_ssbo;
    Handle<BindGroup> bind_group;
};

// Vertex layout for slug text rendering (matches GLSL struct in slug_font.kfx)
// 64 bytes per vertex
struct SlugVertex {
    Vec2f position; // object-space quad corner
    Vec2f em_coord; // em-space coordinate at this corner
    Vec4f color; // vertex color (rgba)
    // Per-glyph data (same for all 4 verts of a quad)
    Vec2f band_scale; // maps em-coord to band index
    Vec2f band_offset; // offset for band index calculation
    u32 band_data_offset; // offset into band SSBO
    u32 h_band_count; // number of horizontal bands
    u32 v_band_count; // number of vertical bands
    u32 curve_data_offset; // offset into curve SSBO (in vec4 units)
};

SlugFont LoadSlugFont(ArenaAllocator* arena, u8* font_data, u64 font_data_size);

// Build a Renderable for one string of slug (vector) text.
// Vertex/index data is bump-allocated from the per-frame dynamic geometry buffer —
// no Geometry lifetime to manage. Call every frame; the allocation is reset automatically.
r::Renderable SlugRenderText(Material* material, SlugFont* font, String8 text, Mat4f transform, Vec4f color, f32 font_size_px, SlugFont* fallback_font = nullptr);

} // namespace r
} // namespace kraft
