#include "kraft_resource_manager.h"

#include <containers/kraft_array.h>

// TODO (amn): REMOVE
#include <core/kraft_base_includes.h>

#include <renderer/kraft_renderer_types.h>

namespace kraft::r {

struct ResourceManagerState
{
    PhysicalDeviceFormatSpecs FormatSpecs;
} InternalState;

void ResourceManager::SetPhysicalDeviceFormatSpecs(const PhysicalDeviceFormatSpecs& Specs)
{
    InternalState.FormatSpecs = Specs;
}

const PhysicalDeviceFormatSpecs& ResourceManager::GetPhysicalDeviceFormatSpecs() const
{
    return InternalState.FormatSpecs;
}

}