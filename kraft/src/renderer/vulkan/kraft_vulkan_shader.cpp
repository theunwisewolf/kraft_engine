#include "kraft_vulkan_shader.h"

#include "platform/kraft_filesystem.h"
#include "core/kraft_memory.h"

namespace kraft
{

bool VulkanCreateShaderModule(VulkanContext* context, const char* path, VkShaderModule* out)
{
    FileHandle handle;
    if (!filesystem::OpenFile(path, FILE_OPEN_MODE_READ, true, &handle))
    {
        KERROR("Failed to open shader file %s", path);
        return false;
    }

    uint64 bufferSize = filesystem::GetFileSize(&handle);
    uint8* buffer = (uint8*)Malloc(bufferSize, MEMORY_TAG_RENDERER, true);
    uint64 size = 0;
    filesystem::ReadAllBytes(&handle, &buffer, &size);
    filesystem::CloseFile(&handle);

    bool ret = VulkanCreateShaderModule(context, buffer, bufferSize, out);
    Free(buffer, bufferSize, MEMORY_TAG_RENDERER);

    return ret;
}

bool VulkanCreateShaderModule(VulkanContext* Context, const uint8* Data, uint64 Size, VkShaderModule* Out)
{
    VkShaderModuleCreateInfo ShaderModuleCreateInfo = {};
    ShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ShaderModuleCreateInfo.codeSize = Size;
    ShaderModuleCreateInfo.pCode = (uint32*)Data;

    KRAFT_VK_CHECK(vkCreateShaderModule(Context->LogicalDevice.Handle, &ShaderModuleCreateInfo, Context->AllocationCallbacks, Out));

    return true;
}

void VulkanDestroyShaderModule(VulkanContext* context, VkShaderModule* module)
{
    vkDestroyShaderModule(context->LogicalDevice.Handle, *module, context->AllocationCallbacks);
    *module = 0;
}

}