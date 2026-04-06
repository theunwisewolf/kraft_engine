#include "kraft_includes.h"

// Just for the lolz
// Not usable

namespace kraft {
namespace r {

struct SlugCurve {
    // Quadratic bezier: p1 -> p2 (control) -> p3
    f32 p1x, p1y;
    f32 p2x, p2y;
    f32 p3x, p3y;
};

static bool IsUtf8ContinuationByte(u8 byte) {
    return (byte & 0xC0u) == 0x80u;
}

static u32 DecodeUtf8Codepoint(const u8* text, u64 remaining, u32* out_advance) {
    if (!text || remaining == 0) {
        if (out_advance) {
            *out_advance = 0;
        }
        return 0;
    }

    u8 b0 = text[0];
    if ((b0 & 0x80u) == 0) {
        if (out_advance) {
            *out_advance = 1;
        }
        return (u32)b0;
    }

    u32 codepoint = 0xFFFDu;
    u32 advance = 1;

    if ((b0 & 0xE0u) == 0xC0u) {
        if (remaining >= 2 && IsUtf8ContinuationByte(text[1])) {
            u32 decoded = ((u32)(b0 & 0x1Fu) << 6) | (u32)(text[1] & 0x3Fu);
            if (decoded >= 0x80u) {
                codepoint = decoded;
                advance = 2;
            }
        }
    } else if ((b0 & 0xF0u) == 0xE0u) {
        if (remaining >= 3 && IsUtf8ContinuationByte(text[1]) && IsUtf8ContinuationByte(text[2])) {
            u32 decoded = ((u32)(b0 & 0x0Fu) << 12) | ((u32)(text[1] & 0x3Fu) << 6) | (u32)(text[2] & 0x3Fu);
            if (decoded >= 0x800u && !(decoded >= 0xD800u && decoded <= 0xDFFFu)) {
                codepoint = decoded;
                advance = 3;
            }
        }
    } else if ((b0 & 0xF8u) == 0xF0u) {
        if (remaining >= 4 && IsUtf8ContinuationByte(text[1]) && IsUtf8ContinuationByte(text[2]) && IsUtf8ContinuationByte(text[3])) {
            u32 decoded = ((u32)(b0 & 0x07u) << 18) | ((u32)(text[1] & 0x3Fu) << 12) | ((u32)(text[2] & 0x3Fu) << 6) | (u32)(text[3] & 0x3Fu);
            if (decoded >= 0x10000u && decoded <= 0x10FFFFu) {
                codepoint = decoded;
                advance = 4;
            }
        }
    }

    if (out_advance) {
        *out_advance = advance;
    }

    return codepoint;
}

// Compute the y-range of a quadratic bezier curve (conservative, using control point bbox)
static void CurveYRange(const SlugCurve& c, f32* out_min, f32* out_max) {
    f32 lo = c.p1y < c.p2y ? c.p1y : c.p2y;
    lo = lo < c.p3y ? lo : c.p3y;
    f32 hi = c.p1y > c.p2y ? c.p1y : c.p2y;
    hi = hi > c.p3y ? hi : c.p3y;
    *out_min = lo;
    *out_max = hi;
}

static void CurveXRange(const SlugCurve& c, f32* out_min, f32* out_max) {
    f32 lo = c.p1x < c.p2x ? c.p1x : c.p2x;
    lo = lo < c.p3x ? lo : c.p3x;
    f32 hi = c.p1x > c.p2x ? c.p1x : c.p2x;
    hi = hi > c.p3x ? hi : c.p3x;
    *out_min = lo;
    *out_max = hi;
}

// Get units_per_em from the font's head table
static f32 GetUnitsPerEm(const stbtt_fontinfo* info) {
    // head table offset +18 = unitsPerEm (uint16)
    u8* data = info->data;
    i32 head = info->head;
    i32 units_per_em = (data[head + 18] << 8) | data[head + 19];

    return (f32)units_per_em;
}

struct ResolvedSlugGlyph {
    SlugGlyph* glyph = nullptr;
    const stbtt_fontinfo* source_info = nullptr;
    f32 source_em_scale = 0.0f;
};

static i32 FindGlyphIndex(const SlugFont* font, u32 codepoint, const u8* source_font_data) {
    for (u32 i = 0; i < font->glyph_count; ++i) {
        if (font->codepoints[i] == codepoint && font->source_font_data[i] == source_font_data) {
            return (i32)i;
        }
    }

    return -1;
}

static SlugGlyph* GetOrCreateGlyphFromSource(SlugFont* font, u32 codepoint, const stbtt_fontinfo* source_info, f32 source_em_scale) {
    const u8* source_font_data = source_info ? source_info->data : nullptr;
    i32 glyph_index = FindGlyphIndex(font, codepoint, source_font_data);
    if (glyph_index >= 0) {
        return &font->glyphs[glyph_index];
    }

    if (font->glyph_count >= KRAFT_SLUG_MAX_GLYPHS) {
        KWARN("Slug glyph cache exhausted while loading U+%04X", codepoint);
        return nullptr;
    }

    u32 slot = font->glyph_count++;
    font->num_chars = font->glyph_count;
    font->codepoints[slot] = codepoint;
    font->source_font_data[slot] = source_font_data;

    SlugGlyph* glyph = &font->glyphs[slot];
    MemZero(glyph, sizeof(*glyph));

    int stb_glyph_index = stbtt_FindGlyphIndex(source_info, (int)codepoint);
    int advance_width = 0;
    int lsb = 0;
    stbtt_GetGlyphHMetrics(source_info, stb_glyph_index, &advance_width, &lsb);
    glyph->x_advance = (f32)advance_width * source_em_scale;

    // Glyph 0 is usually ".notdef". Treat it as unsupported for non-zero codepoints so
    // unsupported emoji or missing Unicode codepoints don't render garbage geometry.
    if (stb_glyph_index == 0 && codepoint != 0) {
        glyph->x_advance = 0.0f;
        return glyph;
    }

    int ix0 = 0;
    int iy0 = 0;
    int ix1 = 0;
    int iy1 = 0;
    if (!stbtt_GetGlyphBox(source_info, stb_glyph_index, &ix0, &iy0, &ix1, &iy1)) {
        return glyph;
    }

    stbtt_vertex* vertices = nullptr;
    int num_verts = stbtt_GetGlyphShape(source_info, stb_glyph_index, &vertices);
    if (num_verts <= 0 || !vertices) {
        if (vertices) {
            stbtt_FreeShape(source_info, vertices);
        }
        return glyph;
    }

    TempArena scratch = ScratchBegin(0, 0);

    glyph->em_x0 = (f32)ix0 * source_em_scale;
    glyph->em_y0 = (f32)iy0 * source_em_scale;
    glyph->em_x1 = (f32)ix1 * source_em_scale;
    glyph->em_y1 = (f32)iy1 * source_em_scale;

    u32 max_curve_count = (u32)num_verts * 2u;
    if (max_curve_count == 0) {
        max_curve_count = 1;
    }

    SlugCurve* glyph_curves = ArenaPushArrayNoZero(scratch.arena, SlugCurve, max_curve_count);

    u32 curve_count = 0;
    f32 cur_x = 0.0f;
    f32 cur_y = 0.0f;

    for (int vi = 0; vi < num_verts; vi++) {
        stbtt_vertex* v = &vertices[vi];
        f32 vx = (f32)v->x * source_em_scale;
        f32 vy = (f32)v->y * source_em_scale;

        switch (v->type) {
        case STBTT_vmove:
            cur_x = vx;
            cur_y = vy;
            break;

        case STBTT_vline: {
            KASSERT(curve_count < max_curve_count);
            SlugCurve* c = &glyph_curves[curve_count++];
            c->p1x = cur_x;
            c->p1y = cur_y;
            c->p2x = (cur_x + vx) * 0.5f;
            c->p2y = (cur_y + vy) * 0.5f;
            c->p3x = vx;
            c->p3y = vy;
            cur_x = vx;
            cur_y = vy;
        } break;

        case STBTT_vcurve: {
            KASSERT(curve_count < max_curve_count);
            SlugCurve* c = &glyph_curves[curve_count++];
            c->p1x = cur_x;
            c->p1y = cur_y;
            c->p2x = (f32)v->cx * source_em_scale;
            c->p2y = (f32)v->cy * source_em_scale;
            c->p3x = vx;
            c->p3y = vy;
            cur_x = vx;
            cur_y = vy;
        } break;

        case STBTT_vcubic: {
            f32 cx1 = (f32)v->cx * source_em_scale;
            f32 cy1 = (f32)v->cy * source_em_scale;
            f32 cx2 = (f32)v->cx1 * source_em_scale;
            f32 cy2 = (f32)v->cy1 * source_em_scale;

            f32 mid_cx = (cx1 + cx2) * 0.5f;
            f32 mid_cy = (cy1 + cy2) * 0.5f;
            f32 mid_x = (cur_x + 3.0f * cx1 + 3.0f * cx2 + vx) * 0.125f;
            f32 mid_y = (cur_y + 3.0f * cy1 + 3.0f * cy2 + vy) * 0.125f;

            f32 q1_cx = (cur_x + cx1) * 0.5f;
            f32 q1_cy = (cur_y + cy1) * 0.5f;
            KASSERT(curve_count + 1 < max_curve_count);
            SlugCurve* c1 = &glyph_curves[curve_count++];
            c1->p1x = cur_x;
            c1->p1y = cur_y;
            c1->p2x = q1_cx + (mid_cx - q1_cx) * 0.5f;
            c1->p2y = q1_cy + (mid_cy - q1_cy) * 0.5f;
            c1->p3x = mid_x;
            c1->p3y = mid_y;

            f32 q2_cx = (cx2 + vx) * 0.5f;
            f32 q2_cy = (cy2 + vy) * 0.5f;
            SlugCurve* c2 = &glyph_curves[curve_count++];
            c2->p1x = mid_x;
            c2->p1y = mid_y;
            c2->p2x = q2_cx + (mid_cx - q2_cx) * 0.5f;
            c2->p2y = q2_cy + (mid_cy - q2_cy) * 0.5f;
            c2->p3x = vx;
            c2->p3y = vy;

            cur_x = vx;
            cur_y = vy;
        } break;
        }
    }

    stbtt_FreeShape(source_info, vertices);

    if (curve_count == 0) {
        ScratchEnd(scratch);
        return glyph;
    }

    glyph->is_supported = true;

    f32 bbox_w = glyph->em_x1 - glyph->em_x0;
    f32 bbox_h = glyph->em_y1 - glyph->em_y0;

    u32 h_bands = (bbox_h > 0.0f) ? KRAFT_SLUG_DEFAULT_BANDS : 0;
    u32 v_bands = (bbox_w > 0.0f) ? KRAFT_SLUG_DEFAULT_BANDS : 0;

    if (curve_count <= 4) {
        h_bands = h_bands > 0 ? 2 : 0;
        v_bands = v_bands > 0 ? 2 : 0;
    } else if (curve_count <= 8) {
        h_bands = h_bands > 0 ? 4 : 0;
        v_bands = v_bands > 0 ? 4 : 0;
    }

    if (h_bands == 0 || v_bands == 0) {
        ScratchEnd(scratch);
        return glyph;
    }

    u32 total_bands = h_bands + v_bands;
    u32* band_curve_indices = ArenaPushArrayNoZero(scratch.arena, u32, total_bands * curve_count);
    u32* band_curve_counts = ArenaPushArray(scratch.arena, u32, total_bands);

    f32 h_band_size = bbox_h / (f32)h_bands;
    for (u32 bi = 0; bi < h_bands; bi++) {
        f32 band_y0 = glyph->em_y0 + bi * h_band_size;
        f32 band_y1 = band_y0 + h_band_size;
        u32 count = 0;
        for (u32 ci = 0; ci < curve_count; ci++) {
            f32 cy_min = 0.0f;
            f32 cy_max = 0.0f;
            CurveYRange(glyph_curves[ci], &cy_min, &cy_max);
            if (cy_max >= band_y0 && cy_min <= band_y1) {
                band_curve_indices[bi * curve_count + count] = ci;
                count++;
            }
        }
        band_curve_counts[bi] = count;
    }

    f32 v_band_size = bbox_w / (f32)v_bands;
    for (u32 bi = 0; bi < v_bands; bi++) {
        f32 band_x0 = glyph->em_x0 + bi * v_band_size;
        f32 band_x1 = band_x0 + v_band_size;
        u32 count = 0;
        for (u32 ci = 0; ci < curve_count; ci++) {
            f32 cx_min = 0.0f;
            f32 cx_max = 0.0f;
            CurveXRange(glyph_curves[ci], &cx_min, &cx_max);
            if (cx_max >= band_x0 && cx_min <= band_x1) {
                band_curve_indices[(h_bands + bi) * curve_count + count] = ci;
                count++;
            }
        }
        band_curve_counts[h_bands + bi] = count;
    }

    u32 required_curve_vec4s = curve_count * 2u;
    u32 required_band_uints = total_bands * 2u;
    for (u32 bi = 0; bi < total_bands; ++bi) {
        required_band_uints += band_curve_counts[bi];
    }

    if (font->total_curve_vec4s + required_curve_vec4s > font->curve_capacity_vec4s) {
        KWARN("Slug curve buffer exhausted while loading U+%04X", codepoint);
        ScratchEnd(scratch);
        return glyph;
    }

    if (font->total_band_uints + required_band_uints > font->band_capacity_uints) {
        KWARN("Slug band buffer exhausted while loading U+%04X", codepoint);
        ScratchEnd(scratch);
        return glyph;
    }

    glyph->curve_data_offset = font->total_curve_vec4s;
    glyph->curve_count = curve_count;
    glyph->band_data_offset = font->total_band_uints;
    glyph->h_band_count = h_bands;
    glyph->v_band_count = v_bands;

    for (u32 i = 0; i < curve_count; i++) {
        u32 base = (font->total_curve_vec4s + i * 2u) * 4u;
        font->curve_data[base + 0] = glyph_curves[i].p1x;
        font->curve_data[base + 1] = glyph_curves[i].p1y;
        font->curve_data[base + 2] = glyph_curves[i].p2x;
        font->curve_data[base + 3] = glyph_curves[i].p2y;
        font->curve_data[base + 4] = glyph_curves[i].p3x;
        font->curve_data[base + 5] = glyph_curves[i].p3y;
        font->curve_data[base + 6] = 0.0f;
        font->curve_data[base + 7] = 0.0f;
    }
    font->total_curve_vec4s += required_curve_vec4s;

    u32 header_start = font->total_band_uints;
    u32 list_offset = header_start + total_bands * 2u;
    font->total_band_uints += required_band_uints;

    for (u32 bi = 0; bi < total_bands; bi++) {
        u32 count = band_curve_counts[bi];
        font->band_data[header_start + bi * 2u + 0u] = count;
        font->band_data[header_start + bi * 2u + 1u] = list_offset;

        for (u32 k = 0; k < count; k++) {
            font->band_data[list_offset++] = band_curve_indices[bi * curve_count + k];
        }
    }

    ScratchEnd(scratch);
    return glyph;
}

static SlugGlyph* GetOrCreateGlyph(SlugFont* font, u32 codepoint) {
    return GetOrCreateGlyphFromSource(font, codepoint, &font->stb_font_info, font->em_scale);
}

static ResolvedSlugGlyph ResolveGlyph(SlugFont* font, SlugFont* fallback_font, u32 codepoint) {
    ResolvedSlugGlyph resolved = {
        .glyph = GetOrCreateGlyph(font, codepoint),
        .source_info = &font->stb_font_info,
        .source_em_scale = font->em_scale,
    };

    if (resolved.glyph && resolved.glyph->is_supported) {
        return resolved;
    }

    if (fallback_font) {
        SlugGlyph* fallback_glyph = GetOrCreateGlyphFromSource(font, codepoint, &fallback_font->stb_font_info, fallback_font->em_scale);
        if (fallback_glyph && fallback_glyph->is_supported) {
            resolved.glyph = fallback_glyph;
            resolved.source_info = &fallback_font->stb_font_info;
            resolved.source_em_scale = fallback_font->em_scale;
        }
    }

    return resolved;
}

SlugFont LoadSlugFont(ArenaAllocator* arena, u8* font_data, u64 font_data_size) {
    (void)arena;
    (void)font_data_size;

    SlugFont font = {};
    stbtt_InitFont(&font.stb_font_info, font_data, stbtt_GetFontOffsetForIndex(font_data, 0));

    f32 units_per_em = GetUnitsPerEm(&font.stb_font_info);
    font.em_scale = 1.0f / units_per_em;

    int ascent, descent, line_gap;
    stbtt_GetFontVMetrics(&font.stb_font_info, &ascent, &descent, &line_gap);
    font.ascent = (f32)ascent * font.em_scale;
    font.descent = (f32)descent * font.em_scale;

    font.start_character = 0;
    font.num_chars = 0;
    font.glyph_count = 0;
    font.curve_capacity_vec4s = KRAFT_SLUG_MAX_TOTAL_CURVE_VEC4S;
    font.band_capacity_uints = KRAFT_SLUG_MAX_TOTAL_BAND_UINTS;

    u64 curve_data_size = (u64)font.curve_capacity_vec4s * 4ull * sizeof(f32);
    font.curve_ssbo = ResourceManager->CreateBuffer({
        .DebugName = "SlugCurveData",
        .Size = curve_data_size,
        .UsageFlags = BufferUsageFlags::BUFFER_USAGE_FLAGS_STORAGE_BUFFER,
        .MemoryPropertyFlags = MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_HOST_VISIBLE | MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_HOST_COHERENT,
        .MapMemory = true,
    });
    KASSERT(font.curve_ssbo != Handle<Buffer>::Invalid());
    font.curve_data = (f32*)ResourceManager->GetBufferData(font.curve_ssbo);
    MemZero(font.curve_data, curve_data_size);

    u64 band_data_size = (u64)font.band_capacity_uints * sizeof(u32);
    font.band_ssbo = ResourceManager->CreateBuffer({
        .DebugName = "SlugBandData",
        .Size = band_data_size,
        .UsageFlags = BufferUsageFlags::BUFFER_USAGE_FLAGS_STORAGE_BUFFER,
        .MemoryPropertyFlags = MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_HOST_VISIBLE | MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_HOST_COHERENT,
        .MapMemory = true,
    });
    KASSERT(font.band_ssbo != Handle<Buffer>::Invalid());
    font.band_data = (u32*)ResourceManager->GetBufferData(font.band_ssbo);
    MemZero(font.band_data, band_data_size);

    BindGroupEntry slug_entries[] = {
        {
            .buffer = font.curve_ssbo,
            .binding = 0,
            .type = ResourceType::StorageBuffer,
        },
        {
            .buffer = font.band_ssbo,
            .binding = 1,
            .type = ResourceType::StorageBuffer,
        },
    };

    font.bind_group = ResourceManager->CreateBindGroup({
        .debug_name = "SlugFontBindGroup",
        .entries = slug_entries,
        .entry_count = KRAFT_C_ARRAY_SIZE(slug_entries),
        .set_index = 3,
        .stage_flags = SHADER_STAGE_FLAGS_FRAGMENT,
    });

    for (u32 codepoint = ' '; codepoint <= '~'; ++codepoint) {
        GetOrCreateGlyph(&font, codepoint);
    }

    return font;
}

r::Renderable SlugRenderText(Material* material, SlugFont* font, String8 text, mat4 transform, vec4 color, f32 font_size_px, SlugFont* fallback_font) {
    KRAFT_TIMING_STATS_BEGIN(slug_render_text_timer);
    RendererSlugTextStats stats = {};
    stats.call_count = 1;
    stats.text_byte_count = text.count;

    TempArena scratch = ScratchBegin(0, 0);

    f32 scale = font_size_px;
    f32 dilation = 1.0f / font_size_px;

    SlugVertex* vertices = ArenaPushArray(scratch.arena, SlugVertex, 4 * text.count);
    u32* indices = ArenaPushArray(scratch.arena, u32, 6 * text.count);

    f32 cursor_x = 0.0f;
    f32 cursor_y = 0.0f;

    f32 space_advance = 0.25f * scale;
    ResolvedSlugGlyph space_glyph = ResolveGlyph(font, fallback_font, ' ');
    if (space_glyph.glyph && space_glyph.glyph->is_supported) {
        space_advance = space_glyph.glyph->x_advance * scale;
    }

    u32 quad_count = 0;
    KRAFT_TIMING_STATS_BEGIN(slug_build_geometry_timer);

    for (u64 byte_index = 0; byte_index < text.count;) {
        u32 advance = 0;
        u32 codepoint = DecodeUtf8Codepoint(text.ptr + byte_index, text.count - byte_index, &advance);
        if (advance == 0) {
            break;
        }
        byte_index += advance;

        if (codepoint == '\n') {
            cursor_y -= font->ascent * scale;
            cursor_x = 0.0f;
            continue;
        }

        if (codepoint == '\t') {
            cursor_x += 4.0f * space_advance;
            continue;
        }

        ResolvedSlugGlyph resolved_glyph = ResolveGlyph(font, fallback_font, codepoint);
        SlugGlyph* glyph = resolved_glyph.glyph;
        if (!glyph) {
            continue;
        }

        f32 kern_advance = 0.0f;
        if (byte_index < text.count) {
            u32 next_advance = 0;
            u32 next_codepoint = DecodeUtf8Codepoint(text.ptr + byte_index, text.count - byte_index, &next_advance);
            if (next_advance > 0) {
                ResolvedSlugGlyph next_glyph = ResolveGlyph(font, fallback_font, next_codepoint);
                if (resolved_glyph.source_info && resolved_glyph.source_info == next_glyph.source_info) {
                    kern_advance = resolved_glyph.source_em_scale * scale * stbtt_GetCodepointKernAdvance(resolved_glyph.source_info, (int)codepoint, (int)next_codepoint);
                }
            }
        }

        if (!glyph->is_supported || glyph->curve_count == 0) {
            cursor_x += kern_advance + glyph->x_advance * scale;
            continue;
        }

        f32 x0 = cursor_x + (glyph->em_x0 - dilation) * scale;
        f32 y0 = cursor_y + (glyph->em_y0 - dilation) * scale;
        f32 x1 = cursor_x + (glyph->em_x1 + dilation) * scale;
        f32 y1 = cursor_y + (glyph->em_y1 + dilation) * scale;

        f32 em_x0 = glyph->em_x0 - dilation;
        f32 em_y0 = glyph->em_y0 - dilation;
        f32 em_x1 = glyph->em_x1 + dilation;
        f32 em_y1 = glyph->em_y1 + dilation;

        f32 bbox_w = glyph->em_x1 - glyph->em_x0;
        f32 bbox_h = glyph->em_y1 - glyph->em_y0;
        Vec2f band_scale = {
            (bbox_w > 0.0f) ? (f32)glyph->v_band_count / bbox_w : 0.0f,
            (bbox_h > 0.0f) ? (f32)glyph->h_band_count / bbox_h : 0.0f,
        };
        Vec2f band_offset = {
            -glyph->em_x0 * band_scale.x,
            -glyph->em_y0 * band_scale.y,
        };

        u32 vi = quad_count * 4;
        for (u32 k = 0; k < 4; k++) {
            vertices[vi + k].color = color;
            vertices[vi + k].band_scale = band_scale;
            vertices[vi + k].band_offset = band_offset;
            vertices[vi + k].band_data_offset = glyph->band_data_offset;
            vertices[vi + k].h_band_count = glyph->h_band_count;
            vertices[vi + k].v_band_count = glyph->v_band_count;
            vertices[vi + k].curve_data_offset = glyph->curve_data_offset;
        }

        vertices[vi + 0].position = {x1, y1};
        vertices[vi + 0].em_coord = {em_x1, em_y1};

        vertices[vi + 1].position = {x1, y0};
        vertices[vi + 1].em_coord = {em_x1, em_y0};

        vertices[vi + 2].position = {x0, y0};
        vertices[vi + 2].em_coord = {em_x0, em_y0};

        vertices[vi + 3].position = {x0, y1};
        vertices[vi + 3].em_coord = {em_x0, em_y1};

        u32 ii = quad_count * 6;
        indices[ii + 0] = vi + 0;
        indices[ii + 1] = vi + 1;
        indices[ii + 2] = vi + 2;
        indices[ii + 3] = vi + 2;
        indices[ii + 4] = vi + 3;
        indices[ii + 5] = vi + 0;

        quad_count++;
        cursor_x += kern_advance + glyph->x_advance * scale;
    }
    KRAFT_TIMING_STATS_ACCUM(slug_build_geometry_timer, stats.build_geometry_ms);

    if (quad_count == 0) {
        ScratchEnd(scratch);
        KRAFT_TIMING_STATS_SET(slug_render_text_timer, stats.total_ms);
        g_Renderer->RecordSlugTextStats(stats);
        return {};
    }

    u32 vertex_count = quad_count * 4;
    u32 index_count = quad_count * 6;
    stats.quad_count = quad_count;
    stats.vertex_bytes_generated = (u64)vertex_count * sizeof(SlugVertex);
    stats.index_bytes_generated = (u64)index_count * sizeof(u32);

    KRAFT_TIMING_STATS_BEGIN(slug_alloc_timer);
    DynamicGeometryAllocation alloc = g_Renderer->AllocateDynamicGeometry(vertex_count, sizeof(SlugVertex), index_count);
    KRAFT_TIMING_STATS_ACCUM(slug_alloc_timer, stats.allocate_dynamic_geometry_ms);

    KRAFT_TIMING_STATS_BEGIN(slug_vertex_memcpy_timer);
    MemCpy(alloc.vertex_ptr, vertices, vertex_count * sizeof(SlugVertex));
    KRAFT_TIMING_STATS_ACCUM(slug_vertex_memcpy_timer, stats.vertex_memcpy_ms);

    KRAFT_TIMING_STATS_BEGIN(slug_index_memcpy_timer);
    MemCpy(alloc.index_ptr, indices, index_count * sizeof(u32));
    KRAFT_TIMING_STATS_ACCUM(slug_index_memcpy_timer, stats.index_memcpy_ms);

    KRAFT_TIMING_STATS_BEGIN(slug_material_update_timer);
    // MaterialSystem::SetProperty(material, String8Raw("DiffuseColor"), color);
    KRAFT_TIMING_STATS_ACCUM(slug_material_update_timer, stats.material_update_ms);

    ScratchEnd(scratch);

    Renderable result = {
        .ModelMatrix = transform,
        .MaterialInstance = material,
        .DrawData = alloc.AsDrawData(),
        .CustomBindGroup = font->bind_group,
        .VertexBuffer = alloc.vertex_buffer,
        .IndexBuffer = alloc.index_buffer,
    };

    KRAFT_TIMING_STATS_SET(slug_render_text_timer, stats.total_ms);
    g_Renderer->RecordSlugTextStats(stats);
    return result;
}

} // namespace r
} // namespace kraft
