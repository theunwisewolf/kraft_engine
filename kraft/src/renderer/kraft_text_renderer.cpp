#define STB_RECT_PACK_IMPLEMENTATION
#include "stb/stb_rect_pack.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb/stb_truetype.h"

namespace kraft {
namespace r {

FontAtlas LoadFontAtlas(ArenaAllocator* arena, u8* font_data, u64 font_data_size, f32 font_size)
{
    TempArena      scratch = ScratchBegin(&arena, 1);
    stbtt_fontinfo font;
    stbtt_InitFont(&font, font_data, stbtt_GetFontOffsetForIndex(font_data, 0));

    const u32 start_character = ' ';
    const u32 num_chars = '~' - start_character;

    FontAtlas atlas = FontAtlas{
        .width = 2048,
        .height = 2048,
        .start_character = start_character,
        .num_chars = num_chars,
    };

    KASSERT(num_chars < KRAFT_MAX_GLYPHS_IN_ATLAS);

    stbtt_packedchar   packed_characters[num_chars];
    stbtt_aligned_quad aligned_quads[num_chars];
    u8*                packed_bitmap = ArenaPushArray(scratch.arena, u8, atlas.width * atlas.height);
    stbtt_pack_context pack_context;
    stbtt_pack_range   range = {
          .font_size = font_size,
          .first_unicode_codepoint_in_range = start_character,
          .array_of_unicode_codepoints = nullptr,
          .num_chars = num_chars,
          .chardata_for_range = packed_characters,
    };
    stbtt_PackBegin(&pack_context, packed_bitmap, atlas.width, atlas.height, 0, 1, nullptr);
    stbtt_PackSetOversampling(&pack_context, 2, 2);
    stbtt_PackFontRanges(&pack_context, font.data, 0, &range, 1);
    stbtt_PackEnd(&pack_context);

    f32 _x = 0.f;
    f32 _y = 0.f;
    for (u32 i = 0; i < num_chars; i++)
    {
        stbtt_GetPackedQuad(packed_characters, atlas.width, atlas.height, i, &_x, &_y, &aligned_quads[i], 0);
        atlas.glyphs[i].baseline_xoff_0 = packed_characters[i].xoff;
        atlas.glyphs[i].baseline_yoff_0 = packed_characters[i].yoff;
        atlas.glyphs[i].baseline_xoff_1 = packed_characters[i].xoff2;
        atlas.glyphs[i].baseline_yoff_1 = packed_characters[i].yoff2;
        atlas.glyphs[i].x_advance = packed_characters[i].xadvance;
        atlas.glyphs[i].u0 = aligned_quads[i].s0;
        atlas.glyphs[i].v0 = aligned_quads[i].t0;
        atlas.glyphs[i].u1 = aligned_quads[i].s1;
        atlas.glyphs[i].v1 = aligned_quads[i].t1;
    }

    KASSERT(packed_bitmap);

    int ascent;
    int descent;
    stbtt_GetFontVMetrics(&font, &ascent, &descent, 0);

    atlas.font_scale = stbtt_ScaleForPixelHeight(&font, font_size);
    atlas.ascent = (f32)ascent;
    atlas.descent = (f32)descent;
    atlas.bitmap = TextureSystem::AcquireTextureWithData(String8Raw("bitmap"), packed_bitmap, atlas.width, atlas.height, 1);

    ScratchEnd(scratch);
    return atlas;
}

r::Renderable RenderText(Material* material, FontAtlas* atlas, String8 text, Mat4f transform, Vec4f color)
{
    TempArena    scratch = ScratchBegin(0, 0);
    r::Vertex2D* vertices = ArenaPushArray(scratch.arena, r::Vertex2D, 4 * text.count);
    u32*         indices = ArenaPushArray(scratch.arena, u32, 6 * text.count);

    f32 cursor_x = 0.0f;
    f32 cursor_y = 0.0f;
    f32 space_advance = atlas->glyphs[' ' - atlas->start_character].x_advance;
    for (u32 i = 0; i < text.count; i++)
    {
        char character = text.ptr[i];
        if (character == '\n')
        {
            cursor_y -= atlas->ascent * atlas->font_scale;
            cursor_x = 0.f;
            continue;
        }

        if (character == '\t')
        {
            cursor_x += 4.0f * space_advance;
            continue;
        }

        u32       char_index = character - atlas->start_character;
        FontGlyph glyph = atlas->glyphs[char_index];

        f32 kern_advance = 0.f;
        if ((i + 1) < text.count)
        {
            kern_advance = atlas->font_scale * stbtt_GetCodepointKernAdvance(&atlas->stb_font_info, text.ptr[i], text.ptr[i + 1]);
        }

        // top-right
        vertices[4 * i + 0] = r::Vertex2D{
            .Position =
                Vec3f{
                    cursor_x + glyph.baseline_xoff_1,
                    cursor_y - glyph.baseline_yoff_0,
                    0.f,
                },
            .UV = { glyph.u1, glyph.v0 },
            .Color = color,
        };

        // bottom-right
        vertices[4 * i + 1] = r::Vertex2D{
            .Position =
                Vec3f{
                    cursor_x + glyph.baseline_xoff_1,
                    cursor_y - glyph.baseline_yoff_1,
                    0.f,
                },
            .UV = { glyph.u1, glyph.v1 },
            .Color = color,
        };

        // bottom-left
        vertices[4 * i + 2] = r::Vertex2D{
            .Position =
                Vec3f{
                    cursor_x + glyph.baseline_xoff_0,
                    cursor_y - glyph.baseline_yoff_1,
                    0.f,
                },
            .UV = { glyph.u0, glyph.v1 },
            .Color = color,
        };

        // top-left
        vertices[4 * i + 3] = r::Vertex2D{
            .Position =
                Vec3f{
                    cursor_x + glyph.baseline_xoff_0,
                    cursor_y - glyph.baseline_yoff_0,
                    0.f,
                },
            .UV = { glyph.u0, glyph.v0 },
            .Color = color,
        };

        indices[6 * i + 0] = 4 * i + 0;
        indices[6 * i + 1] = 4 * i + 1;
        indices[6 * i + 2] = 4 * i + 2;
        indices[6 * i + 3] = 4 * i + 2;
        indices[6 * i + 4] = 4 * i + 3;
        indices[6 * i + 5] = 4 * i + 0;

        cursor_x += kern_advance + glyph.x_advance;
    }

    // This uploads the vertices and index to the GPU, so it is safe to free them
    Geometry* geometry = GeometrySystem::AcquireGeometryWithData(GeometryData{
        .VertexCount = 4 * (u32)text.count,
        .IndexCount = 6 * (u32)text.count,
        .VertexSize = sizeof(r::Vertex2D),
        .IndexSize = sizeof(u32),
        .Vertices = vertices,
        .Indices = indices,
    });

    KASSERT(geometry);

    MaterialSystem::SetProperty(material, String8Raw("DiffuseColor"), color);

    ScratchEnd(scratch);

    return {
        .ModelMatrix = transform,
        .MaterialInstance = material,
        .GeometryId = geometry->InternalID,
        .EntityId = geometry->InternalID,
    };
}

} // namespace r
} // namespace kraft