#ifndef __IC_LUA_SCRIPT_DOMAIN_H__
#define __IC_LUA_SCRIPT_DOMAIN_H__

#include "Scripting/IScriptDomain.h"
#include "Scripting/Debugging/IDebugger.h"

struct lua_State;
struct lua_Debug;


namespace Ice { namespace Scripting { namespace Lua
{
	class LuaScriptDomain : public IScriptDomain
	{
	public:
		LuaScriptDomain(Ice::Scripting::Debugger::IDebugger* debugger = 0);
		~LuaScriptDomain();

		void Update(const float fDeltaTime);

		virtual ICRESULT ExecuteFile(const char* szFilePath);
		virtual ICRESULT ExecuteBuffer(const char* szBuffer, size_t size);

		virtual ICRESULT LoadBuffer(const char* szBuffer, size_t size);
		virtual ICRESULT LoadFile(const char* szFilePath);
		virtual ICRESULT LoadFile(const class icFile* file);

		/*
		virtual ICRESULT GetValue(const char* path, int& value);
		virtual ICRESULT SetValue(const char* path, const int value);

		virtual ICRESULT GetValue(const char* path, float& value);
		virtual ICRESULT SetValue(const char* path, const float value);

		virtual ICRESULT GetValue(const char* path, void*& value);
		virtual ICRESULT SetValue(const char* path, const void* value);

		virtual ICRESULT GetValue(const char* path, icString& value);
		virtual ICRESULT SetValue(const char* path, const icString& value);
		*/

	//private:
		static void DebugHook(lua_State* L, lua_Debug* ar);

		lua_State* L;

		Ice::Scripting::Debugger::IDebugger* _debugger;
	};


} } } // Ice.Scripting.Lua

#endif /// __IC_LUA_SCRIPT_DOMAIN_H__
