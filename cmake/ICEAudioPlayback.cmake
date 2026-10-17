include_guard(GLOBAL)
include(FetchContent)
include("${CMAKE_CURRENT_LIST_DIR}/ICEAudioPCM.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ICEMath.cmake")
get_filename_component(ICE_PLAYBACK_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

if(ANDROID)
    # NDK26 supplies these headers without the C++20 implementations used by this
    # upstream release. Diagnose the library boundary before downloading/building.
    include(CheckCXXSourceCompiles)
    check_cxx_source_compiles("#include <algorithm>
        #include <array>
        #include <compare>
        #include <format>
        int main() {
            std::array<int,1> a{1};
            auto order = std::lexicographical_compare_three_way(a.begin(),a.end(),a.begin(),a.end());
            return order == 0 && std::format(\"{}\",1) == \"1\" ? 0 : 1;
        }" ICE_OPENAL_COMPLETE_CPP20_LIBRARY)
    if(NOT ICE_OPENAL_COMPLETE_CPP20_LIBRARY)
        message(FATAL_ERROR "OpenAL Soft1.25.2 requires implemented C++20 std::format and lexicographical_compare_three_way. NDK26d is unsupported; use a separate current NDK for playback (qualified with r30/API29).")
    endif()
endif()

# Official source release; development static linkage does not waive LGPL distribution terms.
set(ICE_OPENAL_SOURCE_DIR "" CACHE PATH "Explicit verified OpenAL Soft1.25.2 source override")
if(ICE_OPENAL_SOURCE_DIR)
    get_filename_component(ice_openal_override "${ICE_OPENAL_SOURCE_DIR}" REALPATH)
    set(FETCHCONTENT_SOURCE_DIR_ICE_OPENAL "${ice_openal_override}")
    message(STATUS "ICE playback explicit OpenAL source override: ${ice_openal_override}")
endif()
FetchContent_Declare(ice_openal
    URL https://openal-soft.org/openal-releases/openal-soft-1.25.2.tar.bz2
    URL_HASH SHA256=1dbaac44e7579d5bc8847ca8db4b2e8b9fd3961041f35ee20def4958301e1089
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
set(LIBTYPE STATIC CACHE STRING "OpenAL development library type" FORCE)
foreach(flag ALSOFT_UTILS ALSOFT_EXAMPLES ALSOFT_TESTS ALSOFT_INSTALL ALSOFT_UPDATE_BUILD_VERSION
             ALSOFT_BACKEND_OBOE ALSOFT_BACKEND_WAVE ALSOFT_EAX)
    set(${flag} OFF CACHE BOOL "" FORCE)
endforeach()
set(ALSOFT_EMBED_HRTF_DATA ON CACHE BOOL "Retain the pinned upstream HRTF dataset" FORCE)
if(ANDROID)
    set(ALSOFT_REQUIRE_OPENSL ON CACHE BOOL "Require actual Android output backend" FORCE)
elseif(APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "watchOS")
    set(ALSOFT_REQUIRE_COREAUDIO ON CACHE BOOL "Require actual Apple output backend" FORCE)
elseif(WIN32)
    set(ALSOFT_REQUIRE_WASAPI ON CACHE BOOL "Require actual Windows output backend" FORCE)
endif()
enable_language(C)
FetchContent_MakeAvailable(ice_openal)
file(READ "${ice_openal_SOURCE_DIR}/CMakeLists.txt" ice_openal_cmake)
if(NOT ice_openal_cmake MATCHES "VERSION[ \t\r\n]+1[.]25[.]2[ \t\r\n]")
    message(FATAL_ERROR "ICE playback requires the pinned OpenAL Soft1.25.2 source")
endif()
set_target_properties(OpenAL PROPERTIES POSITION_INDEPENDENT_CODE ON)
# Xcode17's SDK marks AURenderCallback nonblocking. The pinned upstream lambdas do
# not declare that effect; keep its diagnostic without asserting unchecked DSP
# guarantees or weakening first-party errors. Only this upstream source is affected.
if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 17)
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("-Wno-error=function-effects" ICE_OPENAL_APPLE_EFFECT_DIAGNOSTIC)
    if(ICE_OPENAL_APPLE_EFFECT_DIAGNOSTIC)
        set_property(SOURCE "${ice_openal_SOURCE_DIR}/alc/backends/coreaudio.cpp"
            DIRECTORY "${ice_openal_SOURCE_DIR}" APPEND PROPERTY COMPILE_OPTIONS
            "-Wno-error=function-effects")
    endif()
endif()
add_library(ICEAudioPlayback STATIC
    "${ICE_PLAYBACK_ROOT}/engine/src/audio/playback.cpp")
add_library(ice::audio_playback ALIAS ICEAudioPlayback)
target_include_directories(ICEAudioPlayback PUBLIC "${ICE_PLAYBACK_ROOT}/engine/include")
target_include_directories(ICEAudioPlayback SYSTEM PRIVATE "${ice_openal_SOURCE_DIR}/include")
target_link_libraries(ICEAudioPlayback PUBLIC ice::audio_pcm ice::math PRIVATE OpenAL)
target_compile_features(ICEAudioPlayback PUBLIC cxx_std_${ICE_CXX_STANDARD})
target_compile_definitions(ICEAudioPlayback PRIVATE AL_LIBTYPE_STATIC)
set_target_properties(ICEAudioPlayback PROPERTIES POSITION_INDEPENDENT_CODE ON)
ice_strict_target(ICEAudioPlayback)
