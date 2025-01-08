#include "kraft_resource_manager.h"

#include <containers/kraft_array.h>
#include <core/kraft_asserts.h>
#include <core/kraft_log.h>
#include <math/kraft_math.h>
#include <renderer/kraft_renderer_types.h>

namespace kraft::renderer {

struct ResourceManagerStateT
{
    PhysicalDeviceFormatSpecs FormatSpecs;
} ResourceManagerData;

void ResourceManager::SetPhysicalDeviceFormatSpecs(const PhysicalDeviceFormatSpecs& Specs)
{
    ResourceManagerData.FormatSpecs = Specs;
}

const PhysicalDeviceFormatSpecs& ResourceManager::GetPhysicalDeviceFormatSpecs() const
{
    return ResourceManagerData.FormatSpecs;
}

}