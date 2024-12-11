#include "kraft_shader_system.h"

#include <containers/kraft_buffer.h>
#include <containers/kraft_hashmap.h>
#include <core/kraft_asserts.h>
#include <core/kraft_log.h>
#include <renderer/kraft_renderer_frontend.h>
#include <renderer/shaderfx/kraft_shaderfx.h>
#include <renderer/shaderfx/kraft_shaderfx_types.h>
#include <resources/kraft_resource_types.h>

namespace kraft {

using namespace renderer;

static void   ReleaseShaderInternal(uint32 Index);
static uint32 AddUniform(
    Shader*                  Shader,
    String                   Name,
    uint32                   Location,
    uint32                   Offset,
    uint32                   Size,
    ResourceType::Enum       Type,
    ShaderUniformScope::Enum Scope
);

struct ShaderReference
{
    uint32 RefCount;
    bool   AutoRelease;
    Shader Shader;
};

struct ShaderSystemState
{
    uint32                  MaxShaderCount;
    ShaderReference*        Shaders;
    HashMap<String, uint32> IndexMapping;
    uint32                  CurrentShaderID;
    Shader*                 CurrentShader;
};

static ShaderSystemState* State = nullptr;

void ShaderSystem::Init(uint32 MaxShaderCount)
{
    uint32 SizeRequirement = sizeof(ShaderSystemState) + sizeof(ShaderReference) * MaxShaderCount;
    char*  RawMemory = (char*)kraft::Malloc(SizeRequirement, MEMORY_TAG_SHADER_SYSTEM, true);

    State = (ShaderSystemState*)RawMemory;
    State->Shaders = (ShaderReference*)(RawMemory + sizeof(ShaderSystemState));
    State->MaxShaderCount = MaxShaderCount;
    State->IndexMapping = HashMap<String, uint32>();
    State->CurrentShaderID = KRAFT_INVALID_ID;
    State->CurrentShader = nullptr;

    // Load the default shader
    // KASSERT(AcquireShader("res/shaders/basic.kfx.bkfx", Handle<RenderPass>::Invalid()));

    KINFO("[ShaderSystem::Init]: Initialized shader system");
}

void ShaderSystem::Shutdown()
{
    // Free the default shader
    ReleaseShaderInternal(0);

    uint32 SizeRequirement = sizeof(ShaderSystemState) + sizeof(ShaderReference) * State->MaxShaderCount;
    kraft::Free(State, SizeRequirement, MEMORY_TAG_SHADER_SYSTEM);

    KINFO("[ShaderSystem::Shutdown]: Shutting down shader system");
}

Shader* ShaderSystem::AcquireShader(const String& ShaderPath, Handle<RenderPass> RenderPassHandle, bool AutoRelease)
{
    auto It = State->IndexMapping.find(ShaderPath);
    if (It != State->IndexMapping.iend())
    {
        uint32 Index = It->second;
        if (Index > 0)
        {
            State->Shaders[Index].RefCount++;
        }

        KDEBUG("[ShaderSystem::AcquireShader]: Shader %s already acquired; Reusing", *ShaderPath);
        return &State->Shaders[Index].Shader;
    }

    int FreeIndex = -1;
    for (uint32 i = 0; i < State->MaxShaderCount; i++)
    {
        if (State->Shaders[i].RefCount == 0)
        {
            FreeIndex = i;
            break;
        }
    }

    if (FreeIndex == -1)
    {
        KWARN("[ShaderSystem::AcquireShader]: Out of memory");
        return nullptr;
    }

    ShaderReference* Reference = &State->Shaders[FreeIndex];
    if (!kraft::renderer::LoadShaderFX(ShaderPath, &Reference->Shader.ShaderEffect))
    {
        KWARN("[ShaderSystem::AcquireShader]: Failed to load %s", *ShaderPath);
        return nullptr;
    }

    State->IndexMapping[ShaderPath] = FreeIndex;
    Reference->Shader.ID = FreeIndex; // Cache the index
    Reference->Shader.Path = ShaderPath;
    Reference->RefCount = 1;
    Reference->AutoRelease = AutoRelease;

    // TODO (amn): What to do with other passes?
    const uint32 PassIndex = 0;
    uint32       GlobalResourceBindingsCount = 2; // TODO (amn): Don't hardcode
    Reference->Shader.UniformCacheMapping = HashMap<String, uint32>();
    Reference->Shader.UniformCache = Array<ShaderUniform>();

    const ShaderEffect&         Effect = Reference->Shader.ShaderEffect;
    const RenderPassDefinition& Pass = Effect.RenderPasses[PassIndex];
    Reference->Shader.UniformCache.Reserve(
        GlobalResourceBindingsCount + Pass.Resources->ResourceBindings.Length + Pass.ConstantBuffers->Fields.Length
    );
    Renderer->CreateRenderPipeline(&Reference->Shader, PassIndex, RenderPassHandle);

    // Cache the uniforms
    // The instance uniforms begin after the global data
    AddUniform(&Reference->Shader, "Projection", 0, 0, sizeof(Mat4f), ResourceType::UniformBuffer, ShaderUniformScope::Global);
    AddUniform(&Reference->Shader, "View", 1, sizeof(Mat4f), sizeof(Mat4f), ResourceType::UniformBuffer, ShaderUniformScope::Global);

    uint32 Offset = 0;
    uint32 TextureCount = 0;
    for (int i = 0; i < Pass.Resources->ResourceBindings.Length; i++)
    {
        auto& ResourceBinding = Pass.Resources->ResourceBindings[i];
        AddUniform(
            &Reference->Shader,
            ResourceBinding.Name,
            ResourceBinding.Binding,
            ResourceBinding.Type == ResourceType::UniformBuffer ? Offset : TextureCount++,
            ResourceBinding.Size,
            ResourceBinding.Type,
            ShaderUniformScope::Instance
        );

        Offset += ResourceBinding.Size;
    }

    Reference->Shader.InstanceUniformsCount = (uint32)Reference->Shader.UniformCache.Size();
    Reference->Shader.TextureCount = TextureCount;

    // Cache constant buffers as local uniforms
    Offset = 0;
    for (uint64 i = 0; i < Pass.ConstantBuffers->Fields.Length; i++)
    {
        auto& ConstantBuffer = Pass.ConstantBuffers->Fields[i];

        // Push constants must be aligned to 4 bytes
        uint32 AlignedSize = (uint32)math::AlignUp(ShaderDataType::SizeOf(ConstantBuffer.Type), 4);
        AddUniform(
            &Reference->Shader, ConstantBuffer.Name, 0, Offset, AlignedSize, ResourceType::ConstantBuffer, ShaderUniformScope::Local
        );

        Offset += AlignedSize;
    }

    return &State->Shaders[FreeIndex].Shader;
}

static uint32 AddUniform(
    Shader*                  Shader,
    String                   Name,
    uint32                   Location,
    uint32                   Offset,
    uint32                   Size,
    ResourceType::Enum       Type,
    ShaderUniformScope::Enum Scope
)
{
    ShaderUniform Uniform = {};
    Uniform.Location = Location;
    Uniform.Offset = Offset;
    Uniform.Stride = Size;
    Uniform.Scope = Scope;
    Uniform.Type = Type;

    uint32 Index = (uint32)Shader->UniformCache.Size();
    Shader->UniformCacheMapping[Name] = Index;
    Shader->UniformCache.Push(Uniform);

    return Index;
}

static void ReleaseShaderInternal(uint32 Index)
{
    ShaderReference* Reference = &State->Shaders[Index];
    if (Reference->RefCount > 0)
    {
        Reference->RefCount--;
    }

    if (Reference->AutoRelease && Reference->RefCount == 0)
    {
        renderer::Renderer->DestroyRenderPipeline(&Reference->Shader);

        Reference->AutoRelease = false;
        Reference->Shader = {};
    }
}

void ShaderSystem::ReleaseShader(Shader* Shader)
{
    auto It = State->IndexMapping.find(Shader->Path);
    if (It == State->IndexMapping.iend())
    {
        KWARN("[ShaderSystem::ReleaseShader]: Shader not found %s", *Shader->Path);
        return;
    }

    uint32 Index = It->second;
    if (Index == 0)
    {
        KWARN("[ShaderSystem::ReleaseShader]: Default shader cannot be released");
        return;
    }

    ReleaseShaderInternal(Index);
}

Shader* ShaderSystem::GetDefaultShader()
{
    return &State->Shaders[0].Shader;
}

void ShaderSystem::Bind(Shader* Shader)
{
    if (State->CurrentShaderID != Shader->ID)
    {
        renderer::Renderer->UseShader(Shader);

        State->CurrentShaderID = Shader->ID;
        State->CurrentShader = Shader;
    }
}

void ShaderSystem::BindByID(uint32 ShaderID)
{
    KASSERT(ShaderID < State->MaxShaderCount);
    ShaderReference* Reference = &State->Shaders[ShaderID];

    ShaderSystem::Bind(&Reference->Shader);
}

void ShaderSystem::SetMaterialInstance(Material* Instance)
{
    KASSERT(Instance);
    KASSERT(State->CurrentShader);

    State->CurrentShader->ActiveMaterial = Instance;
}

void ShaderSystem::Unbind()
{
    if (State->CurrentShader)
    {
        State->CurrentShader->ActiveMaterial = nullptr;
        State->CurrentShaderID = KRAFT_INVALID_ID;
        State->CurrentShader = nullptr;
    }
}

bool ShaderSystem::GetUniform(const Shader* Shader, const String& Name, ShaderUniform& Uniform)
{
    auto It = Shader->UniformCacheMapping.find(Name);
    if (It == Shader->UniformCacheMapping.iend())
        return false;

    return GetUniformByIndex(Shader, It->second.get(), Uniform);
}

bool ShaderSystem::GetUniformByIndex(const Shader* Shader, uint32 Index, ShaderUniform& Uniform)
{
    KASSERT(Index < Shader->UniformCache.Length);
    if (Index >= Shader->UniformCache.Length)
        return false;

    Uniform = Shader->UniformCache[Index];
    return true;
}

template<>
bool ShaderSystem::SetUniform(const String& Name, BufferView Value, bool Invalidate)
{
    return ShaderSystem::SetUniform(Name, (void*)Value.Memory, Invalidate);
}

template<>
bool ShaderSystem::SetUniformByIndex(uint32 Index, BufferView Value, bool Invalidate)
{
    return ShaderSystem::SetUniformByIndex(Index, (void*)Value.Memory, Invalidate);
}

bool ShaderSystem::SetUniform(const String& Name, void* Value, bool Invalidate)
{
    if (State->CurrentShaderID == KRAFT_INVALID_ID)
        return false;

    auto It = State->CurrentShader->UniformCacheMapping.find(Name);
    if (It == State->CurrentShader->UniformCacheMapping.iend())
        return false;

    return SetUniformByIndex(It->second, Value, Invalidate);
}

bool ShaderSystem::SetUniformByIndex(uint32 Index, void* Value, bool Invalidate)
{
    KASSERT(Index < State->CurrentShader->UniformCache.Length);

    ShaderUniform Uniform = State->CurrentShader->UniformCache[Index];
    Renderer->SetUniform(State->CurrentShader, Uniform, Value, Invalidate);

    return true;
}

void ShaderSystem::ApplyGlobalProperties(const Mat4f& Projection, const Mat4f& View)
{
    KASSERT(State->CurrentShader);

    SetUniformByIndex(0, Projection);
    SetUniformByIndex(1, View);
    Renderer->ApplyGlobalShaderProperties(State->CurrentShader);
}

void ShaderSystem::ApplyInstanceProperties()
{
    KASSERT(State->CurrentShader);
    Renderer->ApplyInstanceShaderProperties(State->CurrentShader);
}

}