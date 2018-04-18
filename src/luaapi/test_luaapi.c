#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "unittest/unittest.h"

#define TEST_LUAST_PATH "test_luaapi.lua"
#define LUA_LIB_PATH "/usr/local/lib/lua/5.1/?.so"

st_test(luast, lua_interfaces)
{
    lua_State *L = luaL_newstate();

    luaL_openlibs(L);

    lua_getglobal(L, "package");
    lua_getfield(L, -1, "cpath");

    char cpath[1024];
    snprintf(cpath, 1024, "%s;%s", lua_tostring(L, -1), LUA_LIB_PATH);

    lua_pop(L, 1);
    lua_pushlstring(L, cpath, strlen(cpath));
    lua_setfield(L, -2, "cpath");

    int status = luaL_dofile(L, TEST_LUAST_PATH);

    char *msg = "";
    if (status != 0) {
        msg = (char *)lua_tostring(L, -1);
    }

    st_ut_eq(0, status, "%s", msg);
}


st_ut_main;
