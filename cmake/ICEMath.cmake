include_guard(GLOBAL)

# Shared by desktop targets, native mobile applications, and headless tools.
get_filename_component(ICE_MATH_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
add_library(ICEMath INTERFACE)
add_library(ice::math ALIAS ICEMath)
target_include_directories(ICEMath INTERFACE "${ICE_MATH_ROOT}/engine/include")
target_include_directories(ICEMath SYSTEM INTERFACE "${ICE_MATH_ROOT}/engine/3rdparty/glm")
target_compile_definitions(ICEMath INTERFACE GLM_FORCE_CTOR_INIT)
target_compile_features(ICEMath INTERFACE cxx_std_20)
