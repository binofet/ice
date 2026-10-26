include_guard(GLOBAL)
if(NOT ICE_BUILD_VULKAN OR NOT WIN32 OR NOT ICE_BUILD_DESKTOP)
    message(FATAL_ERROR "Vulkan smoke requires ICE_BUILD_VULKAN and Win32 desktop")
endif()
find_package(Vulkan REQUIRED COMPONENTS glslc)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
get_filename_component(ICE_VK_SMOKE_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(packet "${CMAKE_CURRENT_BINARY_DIR}/ice-vulkan-mesh.shader")
add_custom_command(OUTPUT "${packet}"
    COMMAND Vulkan::glslc --target-env=vulkan1.1 "${ICE_VK_SMOKE_ROOT}/tests/data/vulkan/mesh.vert" -o "${CMAKE_CURRENT_BINARY_DIR}/ice-vulkan-mesh.vert.spv"
    COMMAND Vulkan::glslc --target-env=vulkan1.1 "${ICE_VK_SMOKE_ROOT}/tests/data/vulkan/mesh.frag" -o "${CMAKE_CURRENT_BINARY_DIR}/ice-vulkan-mesh.frag.spv"
    COMMAND "${Python3_EXECUTABLE}" "${ICE_VK_SMOKE_ROOT}/tools/pack_vulkan_shader.py"
        "${CMAKE_CURRENT_BINARY_DIR}/ice-vulkan-mesh.vert.spv" "${CMAKE_CURRENT_BINARY_DIR}/ice-vulkan-mesh.frag.spv" "${packet}"
    DEPENDS "${ICE_VK_SMOKE_ROOT}/tests/data/vulkan/mesh.vert" "${ICE_VK_SMOKE_ROOT}/tests/data/vulkan/mesh.frag" "${ICE_VK_SMOKE_ROOT}/tools/pack_vulkan_shader.py"
    VERBATIM)
add_executable(ice_vulkan_smoke "${ICE_VK_SMOKE_ROOT}/tests/vulkan_smoke.cpp" "${packet}")
target_link_libraries(ice_vulkan_smoke PRIVATE ice::render ice::window)
ice_strict_target(ice_vulkan_smoke)
# Deliberately no add_test: creating a real native window/GPU is an explicit operator action.
