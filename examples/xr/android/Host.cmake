include_guard(GLOBAL)
get_filename_component(ICE_XR_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
if(NOT ANDROID OR NOT TARGET ice::render OR NOT TARGET ice::audio)
    message(FATAL_ERROR "The optional XR Android adapter needs GLES rendering and Android audio")
endif()
set(ICE_OPENXR_AAR_DIR "" CACHE PATH "Extracted verified Khronos OpenXR Android loader 1.1.63")
if(NOT EXISTS "${ICE_OPENXR_AAR_DIR}/jni/${ANDROID_ABI}/libopenxr_loader.so")
    message(FATAL_ERROR "Provide the SHA256-verified Khronos loader using the lab/Quest build script")
endif()
add_library(ICEAndroidOpenXRLoader SHARED IMPORTED)
set_target_properties(ICEAndroidOpenXRLoader PROPERTIES
    IMPORTED_LOCATION "${ICE_OPENXR_AAR_DIR}/jni/${ANDROID_ABI}/libopenxr_loader.so"
    INTERFACE_INCLUDE_DIRECTORIES "${ICE_OPENXR_AAR_DIR}/prefab/modules/headers/include")
add_library(ICEAndroidXRHost STATIC "${ICE_XR_ROOT}/android/host.cpp"
    "${ICE_XR_ROOT}/view_camera.cpp"
    "${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c")
add_library(ice::xr_android_host ALIAS ICEAndroidXRHost)
target_include_directories(ICEAndroidXRHost PUBLIC "${ICE_XR_ROOT}/android")
target_include_directories(ICEAndroidXRHost SYSTEM PUBLIC "${ANDROID_NDK}/sources/android/native_app_glue")
target_link_libraries(ICEAndroidXRHost PUBLIC ice::platform ice::render ice::audio ice::math
    PRIVATE ICEAndroidOpenXRLoader android log EGL GLESv2)
target_compile_definitions(ICEAndroidXRHost PRIVATE XR_USE_PLATFORM_ANDROID XR_USE_GRAPHICS_API_OPENGL_ES)
target_link_options(ICEAndroidXRHost INTERFACE -uANativeActivity_onCreate)
set_target_properties(ICEAndroidXRHost PROPERTIES POSITION_INDEPENDENT_CODE ON)
ice_strict_target(ICEAndroidXRHost)
