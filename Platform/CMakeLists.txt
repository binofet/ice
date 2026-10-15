include(${CMAKE_CURRENT_LIST_DIR}/../cmake/ICEStandards.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/../cmake/ICEMath.cmake)
add_library(ICEPlatformRuntime STATIC ../engine/src/platform/host.cpp ../engine/src/input/companion.cpp ../engine/src/net/companion_link.cpp ../engine/src/input/companion_room.cpp ../engine/src/util/join_qr.cpp)
target_include_directories(ICEPlatformRuntime PUBLIC ../engine/include)
target_link_libraries(ICEPlatformRuntime PUBLIC ice::math)
target_compile_features(ICEPlatformRuntime PUBLIC cxx_std_${ICE_CXX_STANDARD})
set_target_properties(ICEPlatformRuntime PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED NO)

ice_strict_target(ICEPlatformRuntime)

find_package(Threads REQUIRED)
target_link_libraries(ICEPlatformRuntime PUBLIC Threads::Threads)

if(WIN32)
    target_link_libraries(ICEPlatformRuntime PUBLIC ws2_32 iphlpapi)
endif()

if(NOT ANDROID AND NOT CMAKE_SYSTEM_NAME MATCHES "^(iOS|watchOS)$")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../Projects/MobileProbe/web/controller.html" ICE_WEB_HTML)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../Projects/MobileProbe/web/controller.html")
    configure_file(../engine/src/net/companion_page.h.in icCompanionPage.h @ONLY)
    add_library(ICECompanionWeb STATIC ../engine/src/net/companion_web.cpp)
    target_include_directories(ICECompanionWeb SYSTEM PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
    target_link_libraries(ICECompanionWeb PUBLIC ICEPlatformRuntime)
    ice_strict_target(ICECompanionWeb)
endif()
