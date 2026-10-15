#include "Scripting/Game.h"
#include "Scripting/LuaScriptDomain.h"

extern "C"
{
#include "Lua/lua.h"
#include "Lua/lauxlib.h"
#include "Lua/lstring.h"
};

using namespace Ice::Scripting;
using namespace Ice::Scripting::Lua;

extern IScriptDomain* gScript;

	float _fps;
	float _deltaTime;

static const luaL_Reg CoreLibs[] = {
	{"game",     luaopen_game},
	{"system",   luaopen_system},
	{NULL, NULL}
};

static const luaL_Reg GameLibs[] = {
	{"fps",        game_fps},
	{"deltaTime",  game_deltaTime},
	{NULL, NULL}
};

static const luaL_Reg SysLibs[] = {
	{"user",        system_user},
	{NULL, NULL}
};

int game_fps(lua_State* L)
{
	int num_args = lua_gettop(L);
	if (num_args > 0)
	{
		lua_pushstring(L, "too many arguments for game.fps()");
		lua_error(L);
	}

	lua_pushnumber(L, (lua_Number)_fps);
	return 1;
}

int game_deltaTime(lua_State* L)
{
	int num_args = lua_gettop(L);
	if (num_args > 0)
	{
		lua_pushstring(L, "too many arguments for game.deltaTime()");
		lua_error(L);
	}

	lua_pushnumber(L, (lua_Number)_deltaTime);
	return 1;
}

int system_user(lua_State* L)
{
	int num_args = lua_gettop(L);
	if (num_args > 0)
	{
		lua_pushstring(L, "too many arguments for game.user()");
		lua_error(L);
	}

	char userName[100];
#ifdef WIN32
	DWORD nUserName = sizeof(userName);
	GetUserName(userName, &nUserName);
#elif defined(__APPLE__)
#endif
	lua_pushstring(L, userName);
	return 1;
}

int luaopen_game(lua_State* L)
{
	luaL_newlib(L, GameLibs);
	return 1;
}

int luaopen_system(lua_State* L)
{
	luaL_newlib(L, SysLibs);
	return 1;
}

GameScript::GameScript()
{
	lua_State* L = ((LuaScriptDomain*)gScript)->L;
	for (const luaL_Reg* lib = CoreLibs; lib->func; ++lib)
	{
		luaL_requiref(L, lib->name, lib->func, 1);
		lua_pop(L, 1);
	}
}

GameScript::~GameScript()
{
}

void GameScript::Update(const float fDeltaTime)
{
	_deltaTime = fDeltaTime;
	_fps = 60.0f;
}
