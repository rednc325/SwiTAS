#pragma once

#define SOL_USING_CXX_LUA 1

#include <sol.hpp>
#include <string>

#ifdef __SWITCH__
#include <switch.h>
#endif

class LuaScripting {
private:
	sol::state luaState;

public:
	LuaScripting();
};