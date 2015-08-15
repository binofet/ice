
#include "Scripting/Debugging/LuaDebugger.h"

extern "C"
{
#include "Lua/lua.h"
}

#include <vector>

using namespace std;

namespace Ice {
	namespace Scripting {
		namespace Debugger { 
			namespace Lua
{

	LuaDebugEventArgs::LuaDebugEventArgs(lua_State* luaState, lua_Debug* ar)
	{
		_luaState = luaState;
		_luaDebug = ar;
	}

	void LuaDebugger::OnDebugEvent(IDebuggerEventArgs* e)
	{
		LuaDebugEventArgs* event = static_cast<LuaDebugEventArgs*>(e);
		if (event == 0) return;

		lua_State* L = event->_luaState;
		lua_Debug* ar = event->_luaDebug;

		int ballsack = lua_getstack(L, 0, ar);
		lua_getinfo(L, ">Sntu", ar);

		int depth = 0;
		while(ballsack != 0)
		{
			ballsack = lua_getstack(L, depth++, ar);
			lua_getinfo(L, ">Sntu", ar);
		}

		if (ar->event == LUA_HOOKLINE)
		{
			BreakPoints::iterator curBp = _breakPoints->begin();
			BreakPoints::iterator bpEnd = _breakPoints->end();
			for (; curBp != bpEnd; ++curBp)
			{
				if (strcmp(ar->source, curBp->_source.ToChar()) == 0 &&
					curBp->_line == ar->currentline)
				{
					int breakpoint = 1;
				}
			}
		}
	}

	void LuaDebugger::Update(const float fDeltaTime)
	{
	}

} } } }
