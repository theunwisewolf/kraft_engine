#pragma once

#include "core/kraft_core.h"
#include "renderer/kraft_renderer_types.h"
#include "renderer/vulkan/kraft_vulkan_types.h"
#include "resources/kraft_resource_types.h"

namespace kraft {

struct EngineConfig;

namespace renderer {

struct ShaderEffect;

namespace VulkanRendererBackend {

bool Init(EngineConfig* config);
bool Shutdown();
bool PrepareFrame();
bool BeginFrame();
bool EndFrame();
void OnResize(int width, int height);

// SceneView (Temp stuff: Need to figure this out properly)
void            BeginSceneView();
void            OnSceneViewViewportResize(uint32 Width, uint32 Height);
void            EndSceneView();
bool            SetSceneViewViewportSize(uint32 Width, uint32 Height);
Handle<Texture> GetSceneViewTexture();

void CreateRenderPipeline(Shader* Shader, int PassIndex);
void DestroyRenderPipeline(Shader* Shader);
void CreateMaterial(Material* Material);
void DestroyMaterial(Material* Material);

void UseShader(const Shader* Shader);
void SetUniform(Shader* Instance, const ShaderUniform& Uniform, void* Value, bool Invalidate);
void ApplyGlobalShaderProperties(Shader* Material);
void ApplyInstanceShaderProperties(Shader* Material);

// Geometry
void DrawGeometryData(uint32 GeometryID);
bool CreateGeometry(
    Geometry*    Geometry,
    uint32       VertexCount,
    const void*  Vertices,
    uint32       VertexSize,
    uint32       IndexCount,
    const void*  Indices,
    const uint32 IndexSize
);
void DestroyGeometry(Geometry* Geometry);

VulkanContext* Context();
VkDevice       Device();
#ifdef KRAFT_RENDERER_DEBUG
VkBool32 DebugUtilsMessenger(
    VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT             messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void*                                       pUserData
);

void SetObjectName(uint64 Object, VkObjectType ObjectType, const char* Name);
#endif
};

}

}