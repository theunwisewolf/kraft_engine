#include "kraft_material_system.h"

#include <core/kraft_base_includes.h>
#include <containers/kraft_containers_includes.h>
#include <platform/kraft_platform_includes.h>
#include <renderer/kraft_renderer_includes.h>
#include <systems/kraft_systems_includes.h>

#include <kraft_types.h>

namespace kraft {

static MaterialSystemState* material_system_state = 0;

static void ReleaseMaterialInternal(u32 Index);
// static void CreateDefaultMaterialsInternal();
static bool LoadMaterialFromFileInternal(ArenaAllocator* arena, String8 file_path, MaterialDataIntermediateFormat* data);

void MaterialSystem::Init(const RendererOptions& options) {
    ArenaAllocator* arena = CreateArena({.ChunkSize = KRAFT_SIZE_MB(64), .Alignment = 64, .Tag = MEMORY_TAG_MATERIAL_SYSTEM});

    material_system_state = ArenaPush(arena, MaterialSystemState);
    material_system_state->arena = arena;
    material_system_state->material_buffer_size = options.MaterialBufferSize;
    material_system_state->max_materials_count = options.MaxMaterials;
    material_system_state->materials_buffer = ArenaPushArray(arena, u8, material_system_state->material_buffer_size * material_system_state->max_materials_count);
    material_system_state->material_references = ArenaPushArray(arena, MaterialReference, material_system_state->max_materials_count);
}

void MaterialSystem::Shutdown() {
    DestroyArena(material_system_state->arena);
}

Material* MaterialSystem::GetDefaultMaterial() {
    if (!material_system_state) {
        KFATAL("MaterialSystem not yet initialized!");
        return nullptr;
    }

    return &material_system_state->material_references[0].material;
}

Material* MaterialSystem::CreateMaterialFromFile(String8 raw_file_path) {
    TempArena scratch = ScratchBegin(0, 0);
    String8 file_path = raw_file_path;
    if (!StringEndsWith(raw_file_path, String8Raw(".kmt"))) {
        file_path = StringCat(scratch.arena, raw_file_path, String8Raw(".kmt"));
    }

    MaterialDataIntermediateFormat data;
    data.filepath = ArenaPushString8Copy(material_system_state->arena, file_path);
    if (!LoadMaterialFromFileInternal(material_system_state->arena, file_path, &data)) {
        KINFO("[CreateMaterialFromFile]: Failed to parse material %S", raw_file_path);
        return nullptr;
    }

    ScratchEnd(scratch);
    return CreateMaterialWithData(data);
}

Material* MaterialSystem::CreateMaterialWithData(const MaterialDataIntermediateFormat& data) {
    u32 free_index = u32(-1);
    for (u32 i = 0; i < material_system_state->max_materials_count; ++i) {
        if (material_system_state->material_references[i].ref_count == 0) {
            free_index = i;
            break;
        }
    }

    if (free_index == u32(-1)) {
        KERROR("[CreateMaterialWithData]: Max number of materials reached!");
        return nullptr;
    }

    // material_system_state->IndexMapping[data.Name] = free_index;
    MaterialReference* ref = &material_system_state->material_references[free_index];
    ref->ref_count = 1;

    // Load the shader
    Shader* shader = ShaderSystem::AcquireShader(data.shader_asset);
    if (!shader) {
        KERROR("[CreateMaterialWithData]: Failed to load shader %S reference by the material %S", data.shader_asset, data.name);
        return nullptr;
    }

    Material* instance = &ref->material;
    instance->ID = free_index;
    instance->Name = data.name;
    instance->AssetPath = data.filepath;
    instance->Shader = shader;

    // Offset into the material buffer
    u8* material_buf = material_system_state->materials_buffer + free_index * material_system_state->material_buffer_size;
    MemZero(material_buf, material_system_state->material_buffer_size);

    // Copy over the material properties from the material data
    for (auto it = shader->UniformCacheMapping.begin(); it != shader->UniformCacheMapping.end(); it++) {
        ShaderUniform* uniform = &shader->UniformCache[it->second];
        if (uniform->Scope != r::ShaderUniformScope::Instance) {
            continue;
        }

        u64 uniform_key = it->first;

        // Look to see if this property is present in the material
        auto material_it = data.properties.find(uniform_key);
        if (material_it == data.properties.end()) {
            KWARN("[CreateMaterialWithData]: Material '%S' does not contain shader uniform %lld", data.name, uniform_key);
            continue;
        }

        const MaterialProperty* property = &material_it->second;
        if (property->Size != uniform->Stride) {
            KERROR("[CreateMaterialWithData]: Material %S data type mismatch for uniform %lld. Expected size: %d, got %d.", data.name, uniform_key, uniform->Stride, property->Size);
            return nullptr;
        }

        KDEBUG("Setting uniform '%lld' to '%d' (Stride = %d, Offset = %d)", uniform_key, material_it->second.u32Value, uniform->Stride, uniform->Offset);

        if (uniform->DataType.UnderlyingType == r::ShaderDataType::TextureID) {
            u32 tex_index = (u32)material_it->second.TextureValue.GetIndex();
            u32 packed = ((u32)material_it->second.SamplerIndex << 28) | (tex_index & 0x0FFFFFFFu);
            MemCpy(material_buf + uniform->Offset, &packed, uniform->Stride);
        } else {
            MemCpy(material_buf + uniform->Offset, material_it->second.Memory, uniform->Stride);
        }
    }

    // Warn about .kmt properties that don't match any shader uniform
    for (auto it = data.properties.begin(); it != data.properties.end(); it++) {
        u64 property_key = it->first;
        auto uniform_it = shader->UniformCacheMapping.find(property_key);
        if (uniform_it == shader->UniformCacheMapping.end()) {
            KWARN("[CreateMaterialWithData]: Material '%S' contains property with hash %lld that doesn't match any shader uniform (typo in .kmt?)", data.name, property_key);
        }
    }

    // Validate that material data fits within MaterialBufferSize
    {
        u32 max_offset = 0;
        for (auto it = shader->UniformCacheMapping.begin(); it != shader->UniformCacheMapping.end(); it++) {
            ShaderUniform* uniform = &shader->UniformCache[it->second];
            if (uniform->Scope != r::ShaderUniformScope::Instance)
                continue;

            u32 end = uniform->Offset + uniform->Stride;
            if (end > max_offset)
                max_offset = end;
        }

        if (max_offset > material_system_state->material_buffer_size) {
            KERROR(
                "[CreateMaterialWithData]: Material '%S' requires %d bytes but MaterialBufferSize is %d. Material data will overflow into adjacent materials!",
                data.name,
                max_offset,
                material_system_state->material_buffer_size
            );
        }
    }

    instance->Properties = data.properties;
    return instance;
}

void MaterialSystem::DestroyMaterial(String8 name) {
    Material* instance = 0;
    for (i32 i = 0; i < material_system_state->max_materials_count; i++) {
        if (StringEqual(material_system_state->material_references[i].material.Name, name)) {
            instance = &material_system_state->material_references[i].material;
            break;
        }
    }

    if (!instance) {
        KERROR("[MaterialSystem::DestroyMaterial]: Unknown material %S", name);
        return;
    }

    KDEBUG("[MaterialSystem::DestroyMaterial]: Releasing material %S", name);
    if (instance->ID == 0) {
        KWARN("[MaterialSystem::DestroyMaterial]: Default material cannot be released!");
        return;
    }

    ReleaseMaterialInternal(instance->ID);
}

void MaterialSystem::DestroyMaterial(Material* instance) {
    KASSERT(instance->ID < material_system_state->max_materials_count);
    KDEBUG("[MaterialSystem::DestroyMaterial]: Destroying material %S", instance->Name);

    MaterialReference* Reference = &material_system_state->material_references[instance->ID];
    if (!Reference) {
        KERROR("Invalid material %S", instance->Name);
    }

    ShaderSystem::ReleaseShader(instance->Shader);
    MemZero(instance, sizeof(Material));
}

bool MaterialSystem::SetTexture(Material* instance, String8 key, String8 texture_path) {
    r::Handle<Texture> texture = TextureSystem::AcquireTexture(texture_path);
    if (texture.IsInvalid()) {
        return false;
    }

    return SetTexture(instance, key, texture);
}

bool MaterialSystem::SetTexture(Material* instance, String8 key, r::Handle<Texture> texture) {
    u64 key_hash = FNV1AHashBytes(key);
    auto it = instance->Shader->UniformCacheMapping.find(key_hash);
    if (it == instance->Shader->UniformCacheMapping.end()) {
        KERROR("[MaterialSystem::SetTexture]: Unknown key %S", key);
        return false;
    }

    u8* material_buf = material_system_state->materials_buffer + instance->ID * material_system_state->material_buffer_size;
    Shader* shader = instance->Shader;
    ShaderUniform uniform;
    if (!ShaderSystem::GetUniform(shader, key, &uniform)) {
        KERROR("GetUniform() failed for '%S'", key);
        return false;
    }

    u32 Index = texture.GetIndex();
    MemCpy(material_buf + uniform.Offset, &Index, uniform.Stride);
    instance->Properties[key_hash].TextureValue = texture;

    return true;
}

u8* MaterialSystem::GetMaterialsBuffer() {
    return material_system_state->materials_buffer;
}

//
// Internal private methods
//

static void ReleaseMaterialInternal(u32 index) {
    MaterialReference ref = material_system_state->material_references[index];
    if (ref.ref_count == 0) {
        KWARN("[MaterialSystem::ReleaseMaterial]: Material %S already released!", ref.material.Name);
        return;
    }

    ref.ref_count--;
    if (ref.ref_count == 0) {
        MaterialSystem::DestroyMaterial(&ref.material);
    }
}

// static void CreateDefaultMaterialsInternal()
// {
//     MaterialDataIntermediateFormat data;
//     data.Name = "Materials.Default";
//     data.file_path = "Material.Default.kmt";
//     data.ShaderAsset = ShaderSystem::GetDefaultShader()->Path;
//     data.Properties["DiffuseColor"] = MaterialProperty(0, Vec4fOne);
//     data.Properties["DiffuseTexture"] = MaterialProperty(1, TextureSystem::GetDefaultDiffuseTexture());

//     KASSERT(MaterialSystem::CreateMaterialWithData(data, Handle<RenderPass>::Invalid()));
// }

static bool LoadMaterialFromFileInternal(ArenaAllocator* arena, String8 file_path, MaterialDataIntermediateFormat* data) {
    TempArena scratch = ScratchBegin(&arena, 1);
    buffer file_buf = fs::ReadAllBytes(scratch.arena, file_path);

    if (!file_buf.count) {
        KERROR("material file %S is empty", file_path);
        ScratchEnd(scratch);
        return false;
    }

    Lexer lexer;
    lexer.Create(file_buf);

    while (lexer.BytesLeft()) {
        LexerToken token;
        if (LexerError err = lexer.NextToken(&token)) {
            if (err == LEXER_ERROR_UNEXPECTED_EOF) {
                break;
            }

            KERROR("Encountered an error while trying to get the next token from the material file! ErrorCode = %d", err);

            ScratchEnd(scratch);
            return false;
        }

        if (token.type == TokenType::TOKEN_TYPE_IDENTIFIER) {
            if (token.MatchesKeyword(String8Raw("Material"))) {
                if (!lexer.ExpectToken(&token, TokenType::TOKEN_TYPE_IDENTIFIER)) {
                    KERROR("Error on line %d\nExpected TOKEN_TYPE_IDENTIFIER", lexer.line + 1);
                    ScratchEnd(scratch);
                    return false;
                }

                data->name = ArenaPushString8Copy(arena, token.text);
                if (!lexer.ExpectToken(&token, TokenType::TOKEN_TYPE_OPEN_BRACE)) {
                    KERROR("Error on line %d\nExpected TOKEN_TYPE_OPEN_BRACE", lexer.line + 1);
                    ScratchEnd(scratch);
                    return false;
                }

                // Parse material block
                while (!lexer.EqualsToken(&token, TokenType::TOKEN_TYPE_CLOSE_BRACE)) {
                    if (token.MatchesKeyword(String8Raw("Shader"))) {
                        if (!lexer.ExpectToken(&token, TokenType::TOKEN_TYPE_STRING)) {
                            KERROR("Error on line %d\nExpected TOKEN_TYPE_STRING", lexer.line + 1);
                            ScratchEnd(scratch);
                            return false;
                        }

                        data->shader_asset = ArenaPushString8Copy(arena, token.text);
                    } else {
                        String8 key = token.text;
                        u64 key_hash = FNV1AHashBytes(key);
                        if (!lexer.ExpectToken(&token, TokenType::TOKEN_TYPE_IDENTIFIER)) {
                            KERROR("Error on line %d\nExpected TOKEN_TYPE_IDENTIFIER", lexer.line + 1);
                            ScratchEnd(scratch);
                            return false;
                        }

                        bool is_vector = false;
                        int expected_component_count = 0;
                        if (token.MatchesKeyword(String8Raw("vec4"))) {
                            is_vector = true;
                            expected_component_count = 4;
                        } else if (token.MatchesKeyword(String8Raw("vec3"))) {
                            is_vector = true;
                            expected_component_count = 3;
                        } else if (token.MatchesKeyword(String8Raw("vec2"))) {
                            is_vector = true;
                            expected_component_count = 2;
                        }

                        if (is_vector) {
                            if (!lexer.ExpectToken(&token, TokenType::TOKEN_TYPE_OPEN_PARENTHESIS)) {
                                KERROR("Error on line %d\nExpected TOKEN_TYPE_OPEN_PARENTHESIS", lexer.line + 1);
                                ScratchEnd(scratch);
                                return false;
                            }

                            f32 components[4];
                            int component_count = 0;
                            while (!lexer.EqualsToken(&token, TokenType::TOKEN_TYPE_CLOSE_PARENTHESIS)) {
                                if (token.type == TokenType::TOKEN_TYPE_COMMA) {
                                    continue;
                                } else if (token.type == TokenType::TOKEN_TYPE_NUMBER) {
                                    components[component_count++] = (f32)token.float_value;
                                } else {
                                    KERROR("Error on line %d\nExpected TOKEN_TYPE_COMMA or TOKEN_TYPE_NUMBER", lexer.line + 1);
                                    ScratchEnd(scratch);
                                    return false;
                                }
                            }

                            if (expected_component_count != component_count) {
                                KERROR(
                                    "Failed to parse %S. Expected %d components for field '%S' but got only %d. Make sure your vec%d() has %d components.",
                                    file_path,
                                    expected_component_count,
                                    key,
                                    component_count,
                                    component_count,
                                    expected_component_count
                                );

                                ScratchEnd(scratch);
                                return false;
                            }

                            if (component_count == 4)
                                data->properties[key_hash] = vec4(components[0], components[1], components[2], components[3]);
                            if (component_count == 3)
                                data->properties[key_hash] = vec3(components[0], components[1], components[2]);
                            if (component_count == 2)
                                data->properties[key_hash] = vec2(components[0], components[1]);

                            data->properties[key_hash].Size = sizeof(f32) * component_count;
                        } else if (token.MatchesKeyword(String8Raw("f32"))) {
                            if (!lexer.ExpectToken(&token, TokenType::TOKEN_TYPE_OPEN_PARENTHESIS)) {
                                KERROR("Error on line %d\nExpected TOKEN_TYPE_OPEN_PARENTHESIS", lexer.line + 1);
                                ScratchEnd(scratch);
                                return false;
                            }

                            if (!lexer.ExpectToken(&token, TokenType::TOKEN_TYPE_NUMBER)) {
                                KERROR("Error on line %d\nExpected TOKEN_TYPE_NUMBER", lexer.line + 1);
                                ScratchEnd(scratch);
                                return false;
                            }

                            data->properties[key_hash] = f32(token.float_value);
                            data->properties[key_hash].Size = sizeof(f32);

                            if (!lexer.ExpectToken(&token, TokenType::TOKEN_TYPE_CLOSE_PARENTHESIS)) {
                                KERROR("Error on line %d\nExpected TOKEN_TYPE_CLOSE_PARENTHESIS", lexer.line + 1);
                                ScratchEnd(scratch);
                                return false;
                            }
                        } else if (token.MatchesKeyword(String8Raw("f64"))) {
                            if (!lexer.ExpectToken(&token, TokenType::TOKEN_TYPE_OPEN_PARENTHESIS)) {
                                KERROR("Error on line %d\nExpected TOKEN_TYPE_OPEN_PARENTHESIS", lexer.line + 1);
                                ScratchEnd(scratch);
                                return false;
                            }

                            if (!lexer.ExpectToken(&token, TokenType::TOKEN_TYPE_NUMBER)) {
                                KERROR("Error on line %d\nExpected TOKEN_TYPE_NUMBER", lexer.line + 1);
                                ScratchEnd(scratch);
                                return false;
                            }

                            data->properties[key_hash] = f64(token.float_value);
                            data->properties[key_hash].Size = sizeof(f64);

                            if (!lexer.ExpectToken(&token, TokenType::TOKEN_TYPE_CLOSE_PARENTHESIS)) {
                                KERROR("Error on line %d\nExpected TOKEN_TYPE_CLOSE_PARENTHESIS", lexer.line + 1);
                                ScratchEnd(scratch);
                                return false;
                            }
                        } else if (token.MatchesKeyword(String8Raw("tex"))) {
                            if (!lexer.ExpectToken(&token, TokenType::TOKEN_TYPE_OPEN_PARENTHESIS)) {
                                KERROR("Error on line %d\nExpected TOKEN_TYPE_OPEN_PARENTHESIS", lexer.line + 1);
                                ScratchEnd(scratch);
                                return false;
                            }

                            // The first parameter is always the texture path
                            if (!lexer.ExpectToken(&token, TokenType::TOKEN_TYPE_STRING)) {
                                KERROR("Error on line %d\nExpected TOKEN_TYPE_STRING (texture path)", lexer.line + 1);
                                ScratchEnd(scratch);
                                return false;
                            }

                            r::TextureSamplerDescription sampler_description = {};

                            // Following the texture path, we might have comma-separated sampler properties
                            // e.g. tex("path.png", filter=nearest, wrap=clamp, anisotropy=false)
                            while (lexer.Peek().type == TokenType::TOKEN_TYPE_COMMA) {
                                LexerToken skip;
                                lexer.NextToken(&skip); // Skip the comma

                                LexerToken prop_key;
                                if (!lexer.ExpectToken(&prop_key, TokenType::TOKEN_TYPE_IDENTIFIER)) {
                                    KERROR("Error on line %d\nExpected TOKEN_TYPE_IDENTIFIER (sampler property key)", lexer.line + 1);
                                    ScratchEnd(scratch);
                                    return false;
                                }

                                LexerToken eq;
                                if (!lexer.ExpectToken(&eq, TokenType::TOKEN_TYPE_EQUALS)) {
                                    KERROR("Error on line %d\nExpected TOKEN_TYPE_EQUALS after sampler key '%S'", lexer.line + 1, prop_key.text);
                                    ScratchEnd(scratch);
                                    return false;
                                }

                                LexerToken value;
                                if (!lexer.ExpectToken(&value, TokenType::TOKEN_TYPE_IDENTIFIER)) {
                                    KERROR("Error on line %d\nExpected TOKEN_TYPE_IDENTIFIER (sampler property value) for key '%S'", lexer.line + 1, prop_key.text);
                                    ScratchEnd(scratch);
                                    return false;
                                }

                                if (prop_key.MatchesKeyword(S("filter"))) {
                                    if (value.MatchesKeyword(S("nearest")) || value.MatchesKeyword(S("point"))) {
                                        sampler_description.MinFilter = r::TextureFilter::Nearest;
                                        sampler_description.MagFilter = r::TextureFilter::Nearest;
                                    } else if (value.MatchesKeyword(S("linear"))) {
                                        sampler_description.MinFilter = r::TextureFilter::Linear;
                                        sampler_description.MagFilter = r::TextureFilter::Linear;
                                    } else {
                                        KERROR("Invalid filter value '%S'. Expected: nearest, point, linear", value.text);
                                    }
                                } else if (prop_key.MatchesKeyword(S("wrap"))) {
                                    r::TextureWrapMode::Enum mode;
                                    if (value.MatchesKeyword(S("repeat"))) {
                                        mode = r::TextureWrapMode::Repeat;
                                    } else if (value.MatchesKeyword(S("mirror"))) {
                                        mode = r::TextureWrapMode::MirroredRepeat;
                                    } else if (value.MatchesKeyword(S("clamp"))) {
                                        mode = r::TextureWrapMode::ClampToEdge;
                                    } else if (value.MatchesKeyword(S("border"))) {
                                        mode = r::TextureWrapMode::ClampToBorder;
                                    } else {
                                        KERROR("Invalid wrap value '%S'. Expected: repeat, mirror, clamp, border", value.text);
                                        continue;
                                    }

                                    sampler_description.WrapModeU = mode;
                                    sampler_description.WrapModeV = mode;
                                    sampler_description.WrapModeW = mode;
                                } else if (prop_key.MatchesKeyword(S("anisotropy"))) {
                                    if (value.MatchesKeyword(S("true"))) {
                                        sampler_description.AnisotropyEnabled = true;
                                    } else if (value.MatchesKeyword(S("false"))) {
                                        sampler_description.AnisotropyEnabled = false;
                                    } else {
                                        KERROR("Invalid anisotropy value '%S'. Expected: true, false", value.text);
                                    }
                                } else if (prop_key.MatchesKeyword(S("mipmap"))) {
                                    if (value.MatchesKeyword(S("nearest"))) {
                                        sampler_description.MipMapMode = r::TextureMipMapMode::Nearest;
                                    } else if (value.MatchesKeyword(S("linear"))) {
                                        sampler_description.MipMapMode = r::TextureMipMapMode::Linear;
                                    } else {
                                        KERROR("Invalid mipmap value '%S'. Expected: nearest, linear", value.text);
                                    }
                                } else {
                                    KERROR("Unknown sampler property '%S'. Expected: filter, wrap, anisotropy, mipmap", prop_key.text);
                                }
                            }

                            // Create or reuse a sampler with the parsed description
                            r::Handle<TextureSampler> sampler_handle = r::ResourceManager->CreateTextureSampler(sampler_description);

                            // Tokens aren't null terminated, so create a null-terminated string
                            String8 texture_path = StringCopy(scratch.arena, token.text);
                            r::Handle<Texture> handle = TextureSystem::AcquireTexture(texture_path);
                            if (handle.IsInvalid()) {
                                KERROR("[MaterialSystem::CreateMaterial]: Failed to load texture %S for material %S. Using default texture.", texture_path, file_path);
                                handle = TextureSystem::GetDefaultDiffuseTexture();
                            }

                            data->properties[key_hash] = handle;
                            data->properties[key_hash].Size = r::ShaderDataType::SizeOf(r::ShaderDataType{.UnderlyingType = r::ShaderDataType::TextureID});
                            data->properties[key_hash].SamplerIndex = (u8)sampler_handle.GetIndex();

                            if (!lexer.ExpectToken(&token, TokenType::TOKEN_TYPE_CLOSE_PARENTHESIS)) {
                                KERROR("Error on line %d\nExpected TOKEN_TYPE_CLOSE_PARENTHESIS", lexer.line + 1);
                                ScratchEnd(scratch);
                                return false;
                            }
                        } else {
                            KERROR("Material has an invalid field %S", token.text);
                            ScratchEnd(scratch);
                            return false;
                        }
                    }
                }
            }
        } else if (token.type == TokenType::TOKEN_TYPE_END_OF_STREAM) {
            break;
        }
    }

    ScratchEnd(scratch);

    return true;
}

#define MATERIAL_SYSTEM_SET_PROPERTY(T)                                                                                                                                                                \
    template <> bool MaterialSystem::SetProperty(Material* instance, String8 key, T value) {                                                                                                           \
        u64 key_hash = FNV1AHashBytes(key);                                                                                                                                                            \
        auto it = instance->Shader->UniformCacheMapping.find(key_hash);                                                                                                                                \
        if (it == instance->Shader->UniformCacheMapping.end()) {                                                                                                                                       \
            KERROR("[SetProperty]: Unknown property '%S' on material '%S'", key, instance->Name);                                                                                                      \
            return false;                                                                                                                                                                              \
        }                                                                                                                                                                                              \
                                                                                                                                                                                                       \
        u8* material_buffer = material_system_state->materials_buffer + instance->ID * material_system_state->material_buffer_size;                                                                    \
        Shader* shader = instance->Shader;                                                                                                                                                             \
        ShaderUniform uniform;                                                                                                                                                                         \
        if (!ShaderSystem::GetUniform(shader, key, &uniform)) {                                                                                                                                        \
            KERROR("GetUniform() failed for '%S'", key);                                                                                                                                               \
            return false;                                                                                                                                                                              \
        }                                                                                                                                                                                              \
                                                                                                                                                                                                       \
        if (sizeof(value) != uniform.Stride) {                                                                                                                                                         \
            KERROR("You are trying to set a value of size '%d' when the uniform's stride is set to '%d'", sizeof(value), uniform.Stride);                                                              \
            return false;                                                                                                                                                                              \
        }                                                                                                                                                                                              \
                                                                                                                                                                                                       \
        MemCpy(material_buffer + uniform.Offset, &value, uniform.Stride);                                                                                                                              \
        return true;                                                                                                                                                                                   \
    }

MATERIAL_SYSTEM_SET_PROPERTY(mat4);
MATERIAL_SYSTEM_SET_PROPERTY(vec4);
MATERIAL_SYSTEM_SET_PROPERTY(vec3);
MATERIAL_SYSTEM_SET_PROPERTY(vec2);
MATERIAL_SYSTEM_SET_PROPERTY(f32);
MATERIAL_SYSTEM_SET_PROPERTY(f64);
MATERIAL_SYSTEM_SET_PROPERTY(u8);
MATERIAL_SYSTEM_SET_PROPERTY(u16);
MATERIAL_SYSTEM_SET_PROPERTY(u32);
MATERIAL_SYSTEM_SET_PROPERTY(u64);

} // namespace kraft