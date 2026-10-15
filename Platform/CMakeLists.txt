add_library(ICEPlatformRuntime STATIC ../Engine/src/Platform/icHost.cpp)
target_include_directories(ICEPlatformRuntime PUBLIC ../Engine/include)
target_compile_features(ICEPlatformRuntime PUBLIC cxx_std_11)
set_target_properties(ICEPlatformRuntime PROPERTIES POSITION_INDEPENDENT_CODE ON)

find_package(Threads REQUIRED)
target_link_libraries(ICEPlatformRuntime PUBLIC Threads::Threads)
