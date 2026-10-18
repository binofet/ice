include_guard(GLOBAL)
get_filename_component(ICE_PROFILE_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
option(ICE_ENABLE_PROFILING "Compile optional manual-lifetime localhost Tracy instrumentation" OFF)
set(ICE_TRACY_SOURCE_DIR "" CACHE PATH "Pristine local Tracy v0.14.1 checkout; never fetched automatically")
set(ICE_TRACY_COMMIT "30997d5ca6bb632cc10807a1da8a6d3de0aeeb3c")
# Capture configure-time source identity without a runtime lookup or mutable metadata registry.
# Reconfigure after source edits; qualified captures require a clean, sealed source pair.
function(ice_profiling_source_identity target prefix source_root)
    if(NOT ICE_ENABLE_PROFILING)
        return()
    endif()
    find_package(Git REQUIRED)
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${source_root}" rev-parse HEAD
        RESULT_VARIABLE result OUTPUT_VARIABLE revision OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${source_root}" status --porcelain=v1 --untracked-files=all
        RESULT_VARIABLE status_result OUTPUT_VARIABLE status OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT result EQUAL 0 OR NOT status_result EQUAL 0 OR NOT revision MATCHES "^[0-9a-f]+$")
        message(FATAL_ERROR "Profiling requires an inspectable source revision for ${target}")
    endif()
    set(source_state "clean")
    if(NOT status STREQUAL "")
        set(source_state "dirty")
    endif()
    target_compile_definitions(${target} PRIVATE
        ${prefix}_REVISION="${revision}" ${prefix}_SOURCE_STATE="${source_state}"
        ${prefix}_CXX_STANDARD="${ICE_CXX_STANDARD}" ${prefix}_CONFIGURATION="$<CONFIG>")
endfunction()
if(NOT ICE_ENABLE_PROFILING)
    add_library(ICEProfiling INTERFACE)
    target_include_directories(ICEProfiling INTERFACE "${ICE_PROFILE_ROOT}/engine/include")
    target_compile_features(ICEProfiling INTERFACE cxx_std_${ICE_CXX_STANDARD})
    add_library(ice::profiling ALIAS ICEProfiling)
    return()
endif()
if(NOT ICE_TRACY_SOURCE_DIR OR NOT EXISTS "${ICE_TRACY_SOURCE_DIR}/public/TracyClient.cpp")
    message(FATAL_ERROR "Profiling requires an explicit pristine Tracy ${ICE_TRACY_COMMIT} checkout; ICE never downloads it")
endif()
find_package(Git REQUIRED)
execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${ICE_TRACY_SOURCE_DIR}" rev-parse HEAD
    RESULT_VARIABLE tracy_git_result OUTPUT_VARIABLE tracy_head OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${ICE_TRACY_SOURCE_DIR}" status --porcelain=v1 --untracked-files=all
    RESULT_VARIABLE tracy_status_result OUTPUT_VARIABLE tracy_status OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT tracy_git_result EQUAL 0 OR NOT tracy_status_result EQUAL 0 OR
   NOT tracy_head STREQUAL ICE_TRACY_COMMIT OR NOT tracy_status STREQUAL "")
    message(FATAL_ERROR "ICE requires pristine Tracy commit ${ICE_TRACY_COMMIT}; unverified local overrides are rejected")
endif()
if(TARGET TracyClient OR TARGET Tracy::TracyClient)
    message(FATAL_ERROR "ICE profiling must own its single verified Tracy client/define set")
endif()
foreach(flag ENABLE STATIC ON_DEMAND MANUAL_LIFETIME ONLY_LOCALHOST NO_BROADCAST NO_CALLSTACK
        NO_CODE_TRANSFER NO_SYSTEM_TRACING NO_CONTEXT_SWITCH NO_SAMPLING NO_VSYNC_CAPTURE
        NO_FRAME_IMAGE NO_CRASH_HANDLER)
    set(TRACY_${flag} ON CACHE BOOL "ICE first CPU profiling profile" FORCE)
endforeach()
foreach(flag NO_EXIT NO_VERIFY FIBERS LTO Fortran LIBUNWIND_BACKTRACE DEBUGINFOD
        OPENGL_AUTO_CALIBRATION TIMER_FALLBACK DISALLOW_HW_TIMER VERBOSE)
    set(TRACY_${flag} OFF CACHE BOOL "ICE first CPU profiling profile" FORCE)
endforeach()
set(TRACY_CALLSTACK "" CACHE STRING "No per-zone callstack override" FORCE)
set(TRACY_PLATFORM_HEADER "" CACHE STRING "No external platform-hook override" FORCE)
function(ice_add_cpu_tracy)
    # The pinned upstream otherwise auto-detects and links ROCm profiling if installed.
    set(CMAKE_DISABLE_FIND_PACKAGE_rocprofiler-sdk ON)
    add_subdirectory("${ICE_TRACY_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/ice-tracy-client" EXCLUDE_FROM_ALL)
endfunction()
ice_add_cpu_tracy()
# Pinned Tracy derives its internal delayed-init switch from MANUAL_LIFETIME.
add_library(ICEProfiling STATIC
    "${ICE_PROFILE_ROOT}/engine/src/profiling/profile.cpp"
    "${ICE_PROFILE_ROOT}/engine/src/profiling/tracy_backend.cpp")
target_include_directories(ICEProfiling PUBLIC "${ICE_PROFILE_ROOT}/engine/include")
target_compile_definitions(ICEProfiling PUBLIC ICE_PROFILING_ENABLED=1)
target_compile_features(ICEProfiling PUBLIC cxx_std_${ICE_CXX_STANDARD})
target_link_libraries(ICEProfiling PUBLIC Tracy::TracyClient)
ice_profiling_source_identity(ICEProfiling ICE_PROFILE "${ICE_PROFILE_ROOT}")
set_target_properties(ICEProfiling PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(ice::profiling ALIAS ICEProfiling)
ice_strict_target(ICEProfiling)
