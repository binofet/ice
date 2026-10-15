add_library(ICEPlatformRuntime STATIC ../Engine/src/Platform/icHost.cpp ../Engine/src/Input/icCompanion.cpp ../Engine/src/Network/icCompanionLink.cpp)
target_include_directories(ICEPlatformRuntime PUBLIC ../Engine/include)
target_compile_features(ICEPlatformRuntime PUBLIC cxx_std_11)
set_target_properties(ICEPlatformRuntime PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED NO)

find_package(Threads REQUIRED)
target_link_libraries(ICEPlatformRuntime PUBLIC Threads::Threads)

if(WIN32)
    target_link_libraries(ICEPlatformRuntime PUBLIC ws2_32 iphlpapi)
endif()

if(NOT ANDROID AND NOT CMAKE_SYSTEM_NAME MATCHES "^(iOS|watchOS)$")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../Projects/MobileProbe/web/controller.html" ICE_WEB_HTML)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../Projects/MobileProbe/web/controller.html")
    configure_file(../Engine/src/Network/icCompanionPage.h.in icCompanionPage.h @ONLY)
    add_library(ICECompanionWeb STATIC ../Engine/src/Network/icCompanionWeb.cpp)
    target_include_directories(ICECompanionWeb PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
    target_link_libraries(ICECompanionWeb PUBLIC ICEPlatformRuntime)
endif()
