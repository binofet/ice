include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/ICEStandards.cmake")
get_filename_component(ICE_PLATFORM_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
# NDKr26 target compilation uses ICEStandards' c++2b spelling. Nested CMake probes
# reconstruct compiler defaults and lose that override; pthread availability is a C
# facility, so probe it in the supported20 mode, then restore the actual target level.
set(ICE_THREAD_PROBE_STANDARD "${CMAKE_CXX_STANDARD}")
if(ANDROID AND ICE_CXX_STANDARD STREQUAL "23" AND ICE_ANDROID_CPP23_DRAFT_SPELLING
        AND NOT ICE_ANDROID_CPP23_FINAL_SPELLING)
    set(CMAKE_CXX_STANDARD 20)
endif()
find_package(Threads REQUIRED)
set(CMAKE_CXX_STANDARD "${ICE_THREAD_PROBE_STANDARD}")
unset(ICE_THREAD_PROBE_STANDARD)
add_library(ICEPlatformRuntime STATIC
    "${ICE_PLATFORM_ROOT}/engine/src/platform/host.cpp"
    "${ICE_PLATFORM_ROOT}/engine/src/platform/platform_events.cpp")
add_library(ice::platform ALIAS ICEPlatformRuntime)
target_include_directories(ICEPlatformRuntime PUBLIC "${ICE_PLATFORM_ROOT}/engine/include")
target_link_libraries(ICEPlatformRuntime PUBLIC Threads::Threads)
target_compile_features(ICEPlatformRuntime PUBLIC cxx_std_${ICE_CXX_STANDARD})
set_target_properties(ICEPlatformRuntime PROPERTIES POSITION_INDEPENDENT_CODE ON XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED NO)
ice_strict_target(ICEPlatformRuntime)

option(ICE_BUILD_COMPANION "Build optional gesture/replay, local pairing and browser-controller modules" ON)
if(ICE_BUILD_COMPANION)
    add_library(ICECompanionRuntime STATIC
        "${ICE_PLATFORM_ROOT}/engine/src/input/companion.cpp"
        "${ICE_PLATFORM_ROOT}/engine/src/input/companion_room.cpp"
        "${ICE_PLATFORM_ROOT}/engine/src/net/companion_link.cpp"
        "${ICE_PLATFORM_ROOT}/engine/src/util/join_qr.cpp")
    add_library(ice::companion ALIAS ICECompanionRuntime)
    target_link_libraries(ICECompanionRuntime PUBLIC ice::platform)
    set_target_properties(ICECompanionRuntime PROPERTIES POSITION_INDEPENDENT_CODE ON XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED NO)
    if(WIN32)
        target_link_libraries(ICECompanionRuntime PUBLIC ws2_32 iphlpapi)
    endif()
    ice_strict_target(ICECompanionRuntime)
    if(NOT ANDROID AND NOT CMAKE_SYSTEM_NAME MATCHES "^(iOS|watchOS|tvOS|visionOS)$")
        set(controller "${ICE_PLATFORM_ROOT}/examples/mobile/web/controller.html")
        file(READ "${controller}" ICE_WEB_HTML)
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${controller}")
        configure_file("${ICE_PLATFORM_ROOT}/engine/src/net/companion_page.h.in" "${CMAKE_CURRENT_BINARY_DIR}/companion_page.h" @ONLY)
        add_library(ICECompanionWeb STATIC "${ICE_PLATFORM_ROOT}/engine/src/net/companion_web.cpp")
        add_library(ice::companion_web ALIAS ICECompanionWeb)
        target_include_directories(ICECompanionWeb SYSTEM PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
        target_link_libraries(ICECompanionWeb PUBLIC ice::companion)
        ice_strict_target(ICECompanionWeb)
    endif()
endif()
