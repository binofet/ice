#ifndef __IC_GAME_H__
#define __IC_GAME_H__

struct lua_State;

int luaopen_game(lua_State* L);
int luaopen_system(lua_State* L);
int game_deltaTime(lua_State* L);
int game_fps(lua_State* L);
int system_user(lua_State* L);

class GameScript
{
public:
	GameScript();
	~GameScript();

	void Update(const float fDeltaTime);

private:
};

#endif // __IC_GAME_H__