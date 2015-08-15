#ifndef __IC_LUADEBUGGER_H__
#define __IC_LUADEBUGGER_H__

#include <vector>

#include "Memory/icString.h"

#include "Scripting/Debugging/IDebugger.h"
#include "Scripting/Debugging/IDebuggerEventArgs.h"

//#include "ObjectModel\icProperty.h"

struct lua_State;
struct lua_Debug;

	struct BreakPoint
	{
		icString _source;
		int _line;
	};

	typedef std::vector<BreakPoint> BreakPoints;

namespace Ice {
	namespace Scripting {
		namespace Debugger { 
			namespace Lua
{

	class LuaDebugEventArgs : public IDebuggerEventArgs
	{
	public:
		LuaDebugEventArgs(lua_State* luaState, lua_Debug* ar);
		virtual ~LuaDebugEventArgs () {};

		lua_State* _luaState;
		lua_Debug* _luaDebug;
	};

	class LuaDebugger : public IDebugger
	{
	public:
		LuaDebugger()
		{
			_breakPoints = new BreakPoints();
			BreakPoint bp;
			bp._source = "@Resources/scripts/test.lua";
			bp._line = 5;
			_breakPoints->push_back(bp);
		}

		virtual ~LuaDebugger() { delete _breakPoints; };

		virtual void OnDebugEvent(IDebuggerEventArgs* e);

		virtual void Update(const float fDeltaTime);

		//PROPERTY(protected, getBalls);

	private:
		//FIELD(private,"_balls", float);

		BreakPoints* _breakPoints;
	};

} } } } // Ice.Scripting.Debugger.Lua

#endif // __IC_LUADEBUGGER_H__
