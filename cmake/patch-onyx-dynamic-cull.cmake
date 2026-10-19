set(SOURCE "${ONYX_SOURCE_DIR}/onyx/source/resources.cpp")

if(NOT EXISTS "${SOURCE}")
  message(FATAL_ERROR "LOST-IN-MEMORY - Could not find ${SOURCE} to patch")
endif()

file(READ "${SOURCE}" CONTENTS)

set(ORIGINAL
    "    ONYX_CHECK_RESOURCE_IS_NOT_NULL(handle);
    ONYX_CHECK_RESOURCE_POOL_IS_NOT_NULL(handle);

    const ResourceType rtype = GetResourceType(handle);
    if (rtype == Resource_GlyphMesh)
        return false;

    const u32 rid = GetResourceId(handle);
    if (rtype == Resource_DynamicMesh)
        return s_ResourceData3->DynamicMeshes[rid].Flags & MeshDataFlag_BackCulled;

    const u32 pid")

set(REPLACEMENT
    "    ONYX_CHECK_RESOURCE_IS_NOT_NULL(handle);

    const ResourceType rtype = GetResourceType(handle);
    if (rtype == Resource_GlyphMesh)
        return false;

    const u32 rid = GetResourceId(handle);
    if (rtype == Resource_DynamicMesh)
        return s_ResourceData3->DynamicMeshes[rid].Flags & MeshDataFlag_BackCulled;

    ONYX_CHECK_RESOURCE_POOL_IS_NOT_NULL(handle);

    const u32 pid")

string(FIND "${CONTENTS}" "${ORIGINAL}" ORIGINAL_POSITION)

if(NOT ORIGINAL_POSITION EQUAL -1)
  string(REPLACE "${ORIGINAL}" "${REPLACEMENT}" PATCHED "${CONTENTS}")
  file(WRITE "${SOURCE}" "${PATCHED}")
  message(STATUS "LOST-IN-MEMORY - Moved the pool check after the dynamic mesh branch in onyx resources.cpp")
else()
  string(FIND "${CONTENTS}" "${REPLACEMENT}" REPLACEMENT_POSITION)
  if(NOT REPLACEMENT_POSITION EQUAL -1)
    message(STATUS "LOST-IN-MEMORY - onyx resources.cpp already patched")
  else()
    message(FATAL_ERROR "LOST-IN-MEMORY - Could not find the cull check in onyx resources.cpp")
  endif()
endif()
