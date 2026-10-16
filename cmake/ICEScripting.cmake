include_guard(GLOBAL)
include(FetchContent)
include("${CMAKE_CURRENT_LIST_DIR}/ICEMeta.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ICEUri.cmake")

# Lua is compiled from its official, immutable source archive. A deliberate local
# override supports offline builds; it bypasses archive verification, not version checks.
set(ICE_LUA_SOURCE_DIR "" CACHE PATH "Optional local Lua 5.5.1 source tree (no download)")
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
if(ICE_LUA_SOURCE_DIR)
    get_filename_component(lua_source "${ICE_LUA_SOURCE_DIR}" REALPATH)
    if(NOT EXISTS "${lua_source}/src/lua.h")
        message(FATAL_ERROR "ICE_LUA_SOURCE_DIR must contain the Lua 5.5.1 source tree")
    endif()
    set(FETCHCONTENT_SOURCE_DIR_ICE_LUA "${lua_source}")
    message(STATUS "ICE Lua explicit source override: ${lua_source}")
endif()
FetchContent_Declare(ice_lua
    URL https://www.lua.org/ftp/lua-5.5.1.tar.gz
    URL_HASH SHA256=1c4b4068d67061f2a2231ad2b5422e77acea1487ea9890f6320af614f4373dce)
FetchContent_MakeAvailable(ice_lua)
file(READ "${ice_lua_SOURCE_DIR}/src/lua.h" lua_header)
if(NOT lua_header MATCHES "LUA_VERSION_MAJOR_N[ \t]+5" OR
   NOT lua_header MATCHES "LUA_VERSION_MINOR_N[ \t]+5" OR
   NOT lua_header MATCHES "LUA_VERSION_RELEASE_N[ \t]+1")
    message(FATAL_ERROR "ICE scripting requires the pinned Lua 5.5.1 API")
endif()

# No interpreter/compiler executable, package loader, io, os, debug or coroutine
# libraries are linked. The wrapper explicitly opens and restricts selected libraries.
enable_language(C)
set(lua_files lapi lcode lctype ldebug ldo ldump lfunc lgc llex lmem lobject
    lopcodes lparser lstate lstring ltable ltm lundump lvm lzio lauxlib
    lbaselib lmathlib lstrlib ltablib lutf8lib)
set(lua_sources)
foreach(source IN LISTS lua_files)
    list(APPEND lua_sources "${ice_lua_SOURCE_DIR}/src/${source}.c")
endforeach()
add_library(ICELuaVendor STATIC ${lua_sources})
target_include_directories(ICELuaVendor SYSTEM PUBLIC "${ice_lua_SOURCE_DIR}/src")
target_compile_features(ICELuaVendor PRIVATE c_std_99)
set_target_properties(ICELuaVendor PROPERTIES POSITION_INDEPENDENT_CODE ON
    XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED NO)
if(UNIX AND NOT APPLE)
    target_link_libraries(ICELuaVendor PUBLIC m)
endif()
if(ICE_ENABLE_SANITIZERS)
    target_compile_options(ICELuaVendor PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(ICELuaVendor INTERFACE -fsanitize=address,undefined)
endif()

get_filename_component(ICE_SCRIPT_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
add_library(ICEScriptLua STATIC "${ICE_SCRIPT_ROOT}/engine/src/script/lua_program.cpp")
add_library(ice::script_lua ALIAS ICEScriptLua)
target_link_libraries(ICEScriptLua PUBLIC ice::meta ice::uri PRIVATE ICELuaVendor)
target_include_directories(ICEScriptLua PUBLIC "${ICE_SCRIPT_ROOT}/engine/include")
target_compile_features(ICEScriptLua PUBLIC cxx_std_${ICE_CXX_STANDARD})
set_target_properties(ICEScriptLua PROPERTIES POSITION_INDEPENDENT_CODE ON
    XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED NO)
ice_strict_target(ICEScriptLua)
