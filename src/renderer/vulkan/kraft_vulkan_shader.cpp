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

    uint64 bufferSize = filesystem::GetSize(&handle);
    uint8* buffer = (uint8*)Malloc(bufferSize, MEMORY_TAG_RENDERER, true);
    uint64 size = 0;
    filesystem::ReadAllBytes(&handle, &buffer, &size);
    filesystem::CloseFile(&handle);

    VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfo.codeSize = bufferSize;
    shaderModuleCreateInfo.pCode = (uint32*)buffer;

    KRAFT_VK_CHECK(vkCreateShaderModule(context->LogicalDevice.Handle, &shaderModuleCreateInfo, context->AllocationCallbacks, out));
    Free(buffer);
}

}