#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <string.h>

#include "sharetable/sharetable.h"


typedef struct st_lua_ud_s         st_lua_ud_t;
/**
 * userdata for each c share table.
 *
 */
struct st_lua_ud_s {
    st_tvalue_t table;
};

typedef union {
    int          i_value;
    double       d_value;
    st_bool      b_value;
    char*        s_value;
    st_lua_ud_t* u_value;
} st_lua_values_t;


#define ST_LUA_MODULE_NAME     "luast"
#define ST_LUA_ITER_METATABLE  "luast_iter_metatable"
#define ST_LUA_TABLE_METATABLE "luast_table_metatable"

#define st_lua_get_ud_err_ret(L, ud, index, mt_name) do {            \
    ud = luaL_checkudata(L, index, mt_name);                         \
    luaL_argcheck(L, ud != NULL, index, "type " mt_name "expected"); \
} while (0);


static int
st_lua_iter_gc(lua_State *L)
{
    st_sharetable_iter_t *iter = NULL;
    st_lua_get_ud_err_ret(L, iter, 1, ST_LUA_ITER_METATABLE);

    int ret = st_sharetable_free_iterator(iter);
    if (ret != ST_OK) {
        return luaL_error(L, "failed to remove table ref from iter: %d", ret);
    }

    return 0;
}


static int
st_lua_get_array_length_cb(const st_tvalue_t *key, st_tvalue_t *val, void *arg)
{
    int kvalue = 0;
    st_tvalue_t *ret_key = (st_tvalue_t *)arg;

    if (key->type == ST_TYPES_INTEGER) {
        kvalue = *((int *)key->bytes);

        kvalue = kvalue > 0 ? kvalue : 0;
    }

    *((int *)ret_key->bytes) = kvalue;

    return ST_ITER_STOP;
}


int
st_lua_table_len(lua_State *L)
{
    st_lua_ud_t *ud;
    st_lua_get_ud_err_ret(L, ud, 1, ST_LUA_TABLE_METATABLE);

    st_table_t *table = st_table_get_table_addr_from_value(ud->table);

    int len = INT_MAX;
    st_tvalue_t key = st_str_null;
    st_sharetable_make_tvalue(key, len);

    int ret = st_sharetable_foreach(table,
                                    &key,
                                    ST_SIDE_LEFT_EQ,
                                    st_lua_get_array_length_cb,
                                    (void *)&key);
    if (ret == ST_OK) {
        len = 0;
    }
    else if (ret != ST_ITER_STOP) {
        return luaL_error(L, "failed to get table length: %d", ret);
    }

    lua_pushnumber(L, len);

    return 1;
}


static int
st_lua_get_stack_value_info(lua_State *L,
                            int index,
                            st_lua_values_t *value,
                            st_types_t *type)
{
    st_must(L != NULL, ST_ARG_INVALID);
    st_must(value != NULL, ST_ARG_INVALID);
    st_must(type != NULL, ST_ARG_INVALID);
    st_must(index >= 1, ST_ARG_INVALID);

    int iarg;
    double darg;

    int arg_type = lua_type(L, index);
    switch (arg_type) {
    case LUA_TNIL:
    case LUA_TNONE:
        *type = ST_TYPES_NIL;

        break;
    case LUA_TNUMBER:
        iarg = luaL_checkint(L, index);
        darg = luaL_checknumber(L, index);

        if (iarg == darg) {
            *type = ST_TYPES_INTEGER;
            value->i_value = iarg;
        }
        else {
            *type = ST_TYPES_NUMBER;
            value->d_value = darg;
        }

        break;
    case LUA_TBOOLEAN:
        value->b_value = (st_bool)lua_toboolean(L, index);
        *type = ST_TYPES_BOOLEAN;

        break;
    case LUA_TSTRING:
        value->s_value = (char *)luaL_checkstring(L, index);
        *type = ST_TYPES_STRING;

        break;
    case LUA_TUSERDATA:
        st_lua_get_ud_err_ret(L, value->u_value, index, ST_LUA_TABLE_METATABLE);
        *type = ST_TYPES_TABLE;

        break;
    default:
        derr("type %d is not supported", arg_type);

        return ST_ARG_INVALID;
    }

    return ST_OK;
}


static void
st_lua_push_table_to_stack(lua_State *L, st_tvalue_t *tvalue)
{
    st_lua_ud_t *ud = lua_newuserdata(L, sizeof(*ud));

    luaL_getmetatable(L, ST_LUA_TABLE_METATABLE);
    lua_setmetatable(L, -2);

    ud->table = *tvalue;
}


static int
st_lua_push_value_to_stack(lua_State *L, st_tvalue_t *tvalue)
{
    switch(tvalue->type) {
    case ST_TYPES_NIL:
        lua_pushnil(L);

        break;
    case ST_TYPES_STRING:
        lua_pushlstring(L, (const char *)tvalue->bytes, tvalue->len - 1);

        break;
    case ST_TYPES_BOOLEAN:
        lua_pushboolean(L, *((st_bool *)tvalue->bytes));

        break;
    case ST_TYPES_NUMBER:
        lua_pushnumber(L, *((double *)tvalue->bytes));

        break;
    case ST_TYPES_TABLE:
        st_lua_push_table_to_stack(L, tvalue);

        break;
    case ST_TYPES_INTEGER:
        lua_pushinteger(L, *(int *)tvalue->bytes);

        break;
    default:
        return ST_ARG_INVALID;
    }

    return ST_OK;
}


static int
st_lua_get_key(lua_State *L, st_table_t *table, int index)
{
    st_types_t ktype;
    st_tvalue_t value;
    st_lua_values_t key;

    int ret = st_lua_get_stack_value_info(L, index, &key, &ktype);
    if (ret != ST_OK) {
        derr("invalid key type for index: %d", ret);

        return ret;
    }

    if (ktype != ST_TYPES_NIL) {
        ret = st_sharetable_do_get(table, (void *)&key, ktype, &value);

        if (ret == ST_NOT_FOUND) {
            value.type = ST_TYPES_NIL;
            ret = ST_OK;
        }

        if (ret != ST_OK) {
            derr("failed to get value: %d", ret);

            return ret;
        }
    }
    else {
        value.type = ST_TYPES_NIL;
    }

    ret = st_lua_push_value_to_stack(L, &value);
    if (ret != ST_OK) {
        derr("failed to push value to stack: %d", ret);
    }

    if (value.type != ST_TYPES_NIL && !st_types_is_table(value.type)) {
        st_assert(st_sharetable_free(&value) == ST_OK);
    }

    return ret;
}


int
st_lua_table_index(lua_State *L)
{
    st_lua_ud_t *ud = NULL;
    st_lua_get_ud_err_ret(L, ud, 1, ST_LUA_TABLE_METATABLE);

    st_table_t *table = st_table_get_table_addr_from_value(ud->table);

    int ret = st_lua_get_key(L, table, 2);
    if (ret != ST_OK) {
        return luaL_error(L, "failed to get value: %d", ret);
    }

    return 1;
}


static int
st_lua_set_remove(lua_State *L, st_table_t *table, int index)
{
    st_types_t ktype;
    st_types_t vtype;
    st_lua_values_t key;
    st_lua_values_t value;

    int ret = st_lua_get_stack_value_info(L, index, &key, &ktype);
    if (ret != ST_OK || ktype == ST_TYPES_NIL) {
        derr("invalid key type for index: %d", ret);

        return luaL_argerror(L, index, "invalid key type");
    }

    ret = st_lua_get_stack_value_info(L, index + 1, &value, &vtype);
    if (ret != ST_OK) {
        derr("failed to get value from stack: %d", ret);

        return luaL_argerror(L, index+1, "failed to get value from stack");
    }

    if (vtype == ST_TYPES_NIL) {
        /** remove */
        ret = st_sharetable_do_remove_key(table, (void *)&key, ktype);
        ret = (ret == ST_NOT_FOUND ? ST_OK : ret);
    }
    else {
        /** set */
        void *val_to_set = (void *)&value;
        if (vtype == ST_TYPES_TABLE) {
            val_to_set = (void *)value.u_value->table.bytes;
        }

        ret = st_sharetable_do_add(table,
                                   (void *)&key, ktype,
                                   val_to_set, vtype,
                                   1);
    }

    return ret;
}


int
st_lua_table_newindex(lua_State *L)
{
    st_lua_ud_t *ud = NULL;
    st_lua_get_ud_err_ret(L, ud, 1, ST_LUA_TABLE_METATABLE);

    st_table_t *table = st_table_get_table_addr_from_value(ud->table);

    int ret = st_lua_set_remove(L, table, 2);
    if (ret != ST_OK) {
        derr("failed to add or remove key/value: %d", ret);

        return luaL_error(L, "failed to add or remove key/value: %d", ret);
    }

    return 0;
}


int
st_lua_table_equal(lua_State *L)
{
    st_lua_ud_t *ud1 = NULL;
    st_lua_ud_t *ud2 = NULL;

    st_lua_get_ud_err_ret(L, ud1, 1, ST_LUA_TABLE_METATABLE);
    st_lua_get_ud_err_ret(L, ud2, 1, ST_LUA_TABLE_METATABLE);

    st_table_t *tbl1 = st_table_get_table_addr_from_value(ud1->table);
    st_table_t *tbl2 = st_table_get_table_addr_from_value(ud2->table);

    lua_pushnumber(L, (intptr_t)tbl1 == (intptr_t)tbl2);

    return 1;
}


int
st_lua_table_gc(lua_State *L)
{
    /** luaL_argcheck does not work in gc */
    st_lua_ud_t *ud = luaL_checkudata(L, 1, ST_LUA_TABLE_METATABLE);
    st_assert(ud != NULL);

    int ret = st_sharetable_free(&ud->table);
    st_assert(ret == ST_OK);

    ud->table = (st_tvalue_t)st_str_null;

    return 0;
}


int
st_lua_worker_init(lua_State *L)
{
    int ret = st_sharetable_worker_init();

    lua_pushnumber(L, ret);

    return 1;
}


int
st_lua_destroy(lua_State *L)
{
    int ret = st_sharetable_destroy();

    lua_pushnumber(L, ret);

    return 1;
}


int
st_lua_get(lua_State *L)
{
    st_sharetable_process_state_t *pstate = st_sharetable_get_process_state();
    st_table_t *g_root = pstate->lib_state->g_root;

    int ret = st_lua_get_key(L, g_root, 1);
    if (ret != ST_OK) {
        return luaL_error(L, "failed to get key: %d", ret);
    }

    return 1;
}


int
st_lua_register(lua_State *L)
{
    st_sharetable_process_state_t *pstate = st_sharetable_get_process_state();
    st_table_t *g_root = pstate->lib_state->g_root;

    int ret = st_lua_set_remove(L, g_root, 1);
    if (ret != ST_OK) {
        return luaL_error(L, "failed to register key/value: %d", ret);
    }

    return 0;
}


// TODO: lsl, add description for each err code to return ?
int
st_lua_new(lua_State *L)
{
    st_tvalue_t table = st_str_null;

    int ret = st_sharetable_new(&table);
    if (ret != ST_OK) {
        char *err_msg = "failed to new table";

        derr("%s: %d", err_msg, ret);

        lua_pushnil(L);
        lua_pushnumber(L, ret);
        lua_pushlstring(L, err_msg, strlen(err_msg));

        return 3;
    }

    st_lua_push_table_to_stack(L, &table);

    return 1;
}


static int
st_lua_common_iterator(lua_State *L, int is_ipairs)
{
    st_sharetable_iter_t *iter = NULL;

    st_lua_get_ud_err_ret(L, iter, 1, ST_LUA_ITER_METATABLE);

    st_tvalue_t key;
    st_tvalue_t value;

    int ret = st_sharetable_next(iter, &key, &value);
    if (ret == ST_TABLE_MODIFIED) {
        return luaL_error(L, "table modified during iterate: %d", ret);
    }

    if (ret == ST_NOT_FOUND) {
        return 0;
    }

    st_assert(ret == ST_OK);

    if (is_ipairs && key.type != ST_TYPES_INTEGER) {
        st_assert(st_sharetable_free(&key));
        st_assert(st_sharetable_free(&value));

        return 0;
    }

    st_lua_push_value_to_stack(L, &key);
    st_lua_push_value_to_stack(L, &value);

    st_sharetable_free(&key);
    if (!st_types_is_table(value.type)) {
        st_sharetable_free(&value);
    }

    return 2;
}


static int
st_lua_ipairs_iterator(lua_State *L)
{
    return st_lua_common_iterator(L, 1);
}


static int
st_lua_pairs_iterator(lua_State *L)
{
    return st_lua_common_iterator(L, 0);
}


static int
st_lua_common_init_iter(lua_State *L,
                        st_tvalue_t *init_key,
                        int expected_side,
                        lua_CFunction iter_func)
{
    st_lua_ud_t *ud = NULL;
    st_lua_get_ud_err_ret(L, ud, 1, ST_LUA_TABLE_METATABLE);

    lua_pushcfunction(L, iter_func);

    st_sharetable_iter_t *iter = lua_newuserdata(L, sizeof(*iter));
    luaL_getmetatable(L, ST_LUA_ITER_METATABLE);
    lua_setmetatable(L, -2);

    int ret = st_sharetable_init_iterator(&ud->table,
                                          iter,
                                          init_key,
                                          expected_side);
    if (ret != ST_OK) {
        return luaL_error(L, "failed to init iterator: %d", ret);
    }

    /** control variable as nil */
    return 2;
}


int
st_lua_ipairs(lua_State *L)
{
    int start = 1;
    st_tvalue_t init_key = st_str_null;
    st_sharetable_make_tvalue(init_key, start);

    return st_lua_common_init_iter(L,
                                   &init_key,
                                   ST_SIDE_RIGHT_EQ,
                                   st_lua_ipairs_iterator);
}


int
st_lua_pairs(lua_State *L)
{
    return st_lua_common_init_iter(L, NULL, 0, st_lua_pairs_iterator);
}


int
st_lua_collect_garbage(lua_State *L)
{
    st_sharetable_process_state_t *pstate = st_sharetable_get_process_state();

    int ret = st_gc_run(&pstate->lib_state->table_pool.gc);
    if (ret != ST_OK && ret != ST_NO_GC_DATA) {
        derr("run gc failed: %d", ret);
    }

    lua_pushnumber(L, ret);

    return 1;
}


int
st_lua_proc_crash_detection(lua_State *L)
{
    st_types_t num_type;
    st_lua_values_t value;

    int ret = st_lua_get_stack_value_info(L, 1, &value, &num_type);
    if (ret != ST_OK) {
        return luaL_error(L, "failed to get num from stack: %d", ret);
    }

    if (num_type == ST_TYPES_NIL) {
        value.i_value = 0;
        num_type = ST_TYPES_INTEGER;
    }

    if (num_type != ST_TYPES_INTEGER) {
        return luaL_error(L, "invalid max number type: %d, %d", ret, num_type);
    }

    int recycled = 0;
    ret = st_sharetable_recycle_roots(value.i_value, &recycled);
    if (ret != ST_OK) {
        derr("failed to run proc crash detection: %d", ret);
    }

    lua_pushnumber(L, (ret == ST_OK ? recycled : -1));

    return 1;
}


/** metamethods of iterator */
static const luaL_Reg st_lua_iter_metamethods[] = {
    { "__gc", st_lua_iter_gc },

    { NULL,   NULL           },
};


/** metamethods of share table object */
static const luaL_Reg st_lua_table_metamethods[] = {
    /** get array length */
    { "__len",      st_lua_table_len      },
    /** get key/value from table */
    { "__index",    st_lua_table_index    },
    /** set key/value to table */
    { "__newindex", st_lua_table_newindex },
    /** equals */
    { "__eq",       st_lua_table_equal    },
    /** remove table from proot */
    { "__gc",       st_lua_table_gc       },

    { NULL,         NULL                  },
};


/** module methods */
static const luaL_Reg st_lua_module_methods[] = {
    /** worker init */
    { "worker_init",          st_lua_worker_init          },
    /** destroy the whole module */
    { "destroy",              st_lua_destroy              },
    /** get from groot */
    { "get",                  st_lua_get                  },
    /** add/remove key from/to groot */
    { "register",             st_lua_register             },
    /** create a table */
    { "new",                  st_lua_new                  },
    /** iterate array */
    { "ipairs",               st_lua_ipairs               },
    /** iterate dictionary */
    { "pairs",                st_lua_pairs                },
    /** force triger gc */
    { "collectgarbage",       st_lua_collect_garbage      },
    /** monitor process crash */
    { "proc_crash_detection", st_lua_proc_crash_detection },

    { NULL,                   NULL                        },
};


int
luaopen_libluast(lua_State *L)
{
    /** register metatable for sharetable ud in lua */
    luaL_newmetatable(L, ST_LUA_TABLE_METATABLE);
    luaL_register(L, NULL, st_lua_table_metamethods);

    /** register metatable for sharetable iterator in lua */
    luaL_newmetatable(L, ST_LUA_ITER_METATABLE);
    luaL_register(L, NULL, st_lua_iter_metamethods);

    /** module lua table */
    lua_newtable(L);
    luaL_register(L, ST_LUA_MODULE_NAME, st_lua_module_methods);

    if (st_sharetable_init() != ST_OK) {
        /**
         * this function is called when loading library,
         * so it should only be called once in master process.
         * if failed, lua require() would return nil,
         * so caller defines the error handling logic.
         */
        return 0;
    }

    return 1;
}
