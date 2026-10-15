include(../cmake/ICEData.cmake)
include(../cmake/ICEMath.cmake)

# CPU ingestion/animation/camera do not create a device or require native presentation.
add_library(ICEAssetRuntime STATIC
    src/animation/scene_animation.cpp
    src/assets/scene_asset.cpp
    src/assets/gltf_readers.cpp
    src/render/scene_camera.cpp)
add_library(ice::assets ALIAS ICEAssetRuntime)
target_link_libraries(ICEAssetRuntime PUBLIC ice::math ice::uri)
target_include_directories(ICEAssetRuntime PUBLIC include)
target_include_directories(ICEAssetRuntime SYSTEM PRIVATE 3rdparty)
ice_strict_target(ICEAssetRuntime)

add_library(ICEMetaRuntime STATIC src/meta/schema.cpp)
add_library(ice::meta ALIAS ICEMetaRuntime)
target_link_libraries(ICEMetaRuntime PUBLIC ice::expected)
target_include_directories(ICEMetaRuntime PUBLIC include)
ice_strict_target(ICEMetaRuntime)

# Consumers may select individual modules. The aggregate includes enabled native backends,
# without owning an application, window or device.
add_library(ICEEngine INTERFACE)
add_library(ice::engine ALIAS ICEEngine)
target_link_libraries(ICEEngine INTERFACE ice::platform ice::assets ice::meta ice::data)
if(ICE_BUILD_DESKTOP)
    include(../cmake/ICEDesktop.cmake)
    target_link_libraries(ICEEngine INTERFACE ice::window)
    target_compile_definitions(ICEEngine INTERFACE ICE_HAS_DESKTOP=1)
endif()
if(ICE_BUILD_METAL)
    include(../cmake/ICEMetal.cmake)
    target_link_libraries(ICEEngine INTERFACE ice::metal)
    target_compile_definitions(ICEEngine INTERFACE ICE_HAS_METAL=1)
endif()
if(ICE_BUILD_OPENGL)
    include(../cmake/ICEOpenGL.cmake)
    target_link_libraries(ICEEngine INTERFACE ice::opengl)
    target_compile_definitions(ICEEngine INTERFACE ICE_HAS_OPENGL=1)
endif()
if(ICE_BUILD_METAL OR ICE_BUILD_OPENGL)
    include(../cmake/ICERender.cmake)
    target_link_libraries(ICEEngine INTERFACE ice::render)
endif()
