
#include <new>

#include "Scripting/LuaScriptDomain.h"

#include "Memory/icMemory.h"

#include "Scripting/Debugging/LuaDebugger.h"
#include "Scripting/Game.h"

extern "C"
{
#include "Lua/lua.h"
#include "Lua/lualib.h"
#include "Lua/lauxlib.h"
#include "Lua/ldebug.h"
}

using namespace Ice::Scripting::Debugger;
using namespace Ice::Scripting::Debugger::Lua;

namespace Ice { namespace Scripting { namespace Lua
{
	void* luaUserReAlloc(void* ud, void* ptr, size_t old_size, size_t new_size)
	{
		return icMemory::getScriptPool()->realloc(ptr, new_size);
	};

	void LuaScriptDomain::DebugHook(lua_State* L, lua_Debug* ar)
	{
		void* domain;
		lua_getallocf(L, &domain);
		if (domain != 0)
		{
			IDebugger* debugger = static_cast<LuaScriptDomain*>(domain)->_debugger;
			if (debugger != 0)
			{
				void* mem = icMemory::getDefaultPool()->malloc(sizeof(LuaDebugEventArgs));
				new (mem) LuaDebugEventArgs(L, ar);
				debugger->OnDebugEvent((IDebuggerEventArgs*)mem);
				icMemory::getDefaultPool()->free(mem);
			}
		}
	}

	LuaScriptDomain::LuaScriptDomain(IDebugger* debugger)
	{
		_debugger = debugger;
		L = lua_newstate(luaUserReAlloc, this);
		luaL_openlibs(L);
		if (debugger)
		{
			lua_sethook(L, DebugHook, LUA_MASKCALL|LUA_MASKLINE|LUA_MASKRET, 0);
		}
	}

	LuaScriptDomain::~LuaScriptDomain()
	{
		if (L)
		{
			lua_close(L);
			L = 0;
		}
	}

	void LuaScriptDomain::Update(const float fDeltaTime)
	{
		static GameScript gamescript;
		gamescript.Update(fDeltaTime);
	}

	ICRESULT LuaScriptDomain::ExecuteFile(const char* szFilePath)
	{
		ICRESULT result = LoadFile(szFilePath);
		if (ICEFAIL(result))
		{
			return result;
		}

		int state = lua_pcall(L, 0, LUA_MULTRET, 0);

		return state == LUA_OK ? IC_OK : IC_FAIL_SCRIPT_RUNTIME_ERROR;
	}

	ICRESULT LuaScriptDomain::ExecuteBuffer(const char* szBuffer, size_t size)
	{
		ICRESULT result = LoadBuffer(szBuffer, size);
		if (ICEFAIL(result))
		{
			return result;
		}

		int state = lua_pcall(L, 0, LUA_MULTRET, 0);

		if (state != 0)
		{

			return IC_FAIL_SCRIPT_RUNTIME_ERROR;
		}

		return IC_OK;
	}

	ICRESULT LuaScriptDomain::LoadBuffer(const char* szBuffer, size_t size)
	{
		if (!L) return IC_FAIL_NO_SCRIPT_CONTEXT;

		int state = luaL_loadbuffer(L, szBuffer, size, szBuffer);
		if (state != 0)
		{
			return IC_FAIL_SCRIPT_LOAD_ERROR;
		}

		return IC_OK;
	}

	ICRESULT LuaScriptDomain::LoadFile(const char* szFilePath)
	{
		if (!L) return IC_FAIL_NO_SCRIPT_CONTEXT;

		int state = luaL_loadfile(L, szFilePath);
		if (state != 0)
		{
			return IC_FAIL_SCRIPT_LOAD_ERROR;
		}

		return IC_OK;
	}

	ICRESULT LuaScriptDomain::LoadFile(const class icFile* file)
	{
		if (!L) return IC_FAIL_NO_SCRIPT_CONTEXT;

		return IC_NOT_IMPL;
	}

	/*
	ICRESULT LuaScriptDomain::GetValue(const char* path, int& value)
	{
		return IC_NOT_IMPL;
	}

	ICRESULT LuaScriptDomain::SetValue(const char* path, const int value)
	{
		return IC_NOT_IMPL;
	}

	ICRESULT LuaScriptDomain::GetValue(const char* path, float& value)
	{
		return IC_NOT_IMPL;
	}

	ICRESULT LuaScriptDomain::SetValue(const char* path, const float value)
	{
		return IC_NOT_IMPL;
	}

	ICRESULT LuaScriptDomain::GetValue(const char* path, void*& value)
	{
		return IC_NOT_IMPL;
	}

	ICRESULT LuaScriptDomain::SetValue(const char* path, const void* value)
	{
		return IC_NOT_IMPL;
	}

	ICRESULT LuaScriptDomain::GetValue(const char* path, icString& value)
	{
		return IC_NOT_IMPL;
	}

	ICRESULT LuaScriptDomain::SetValue(const char* path, const icString& value)
	{
		return IC_NOT_IMPL;
	}
	*/

} } } // Ice.Scripting.Lua