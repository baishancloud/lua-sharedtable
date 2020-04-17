#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <string.h>

#include "capi/capi.h"


typedef struct st_luaapi_ud_s st_luaapi_ud_t;
/**
 * userdata for each c share table.
 *
 */
struct st_luaapi_ud_s {
    st_tvalue_t table;
};

typedef union {
    int             i_value;
    double          d_value;
    st_bool         b_value;
    char*           s_value;
    st_luaapi_ud_t* u_value;
} st_luaapi_values_t;


#define ST_LUA_MODULE_NAME     "luast"
#define ST_LUA_ITER_METATABLE  "luast_iter_metatable"
#define ST_LUA_TABLE_METATABLE "luast_table_metatable"

#define st_luaapi_get_ud_err_ret(L, ud, index, mt_name) do {                \
    ud = luaL_checkudata((L), (index), (mt_name));                          \
    luaL_argcheck((L), (ud) != NULL, (index), "type " mt_name " expected"); \
} while (0);


static int
st_luaapi_iter_gc(lua_State *L)
{
    st_capi_iter_t *iter = NULL;
    st_luaapi_get_ud_err_ret(L, iter, 1, ST_LUA_ITER_METATABLE);

    int ret = st_capi_free_iterator(iter);
    if (ret != ST_OK) {
        return luaL_error(L, "failed to remove table ref from iter: %d", ret);
    }

    return 0;
}


static int
st_luaapi_get_array_len_cb(const st_tvalue_t *key, st_tvalue_t *val, void *arg)
{
    if (key->type == ST_TYPES_INTEGER) {
        int kvalue = *((int *)key->bytes);

        if (kvalue > 0) {
            *((int *)arg) = kvalue;
        }
    }

    return ST_ITER_FINISH;
}


int
st_luaapi_table_len(lua_State *L)
{
    st_luaapi_ud_t *ud;
    st_luaapi_get_ud_err_ret(L, ud, 1, ST_LUA_TABLE_METATABLE);

    st_table_t *table = st_table_get_table_addr_from_value(ud->table);

    int search_key = INT_MAX;
    st_tvalue_t key = st_capi_make_tvalue(search_key);

    int table_len = 0;
    /**
     * use st_capi_foreach to lock table, handle return value
     * and do error handling. st_luaapi_get_array_len_cb would
     * be called only once.
     */
    int ret = st_capi_foreach(table,
                              &key,
                              ST_SIDE_LEFT_EQ,
                              st_luaapi_get_array_len_cb,
                              (void *)&table_len);
    if (ret != ST_OK) {
        return luaL_error(L, "failed to get table length: %d", ret);
    }

    lua_pushnumber(L, table_len);

    return 1;
}


static int
st_luaapi_get_stack_value_info(lua_State *L,
                               int index,
                               st_luaapi_values_t *vbuf,
                               st_tvalue_t *value)
{
    st_assert_nonull(L);
    st_assert_nonull(vbuf);
    st_assert_nonull(value);
    st_assert(index >= 1);

    int i_value;
    double d_value;

    int vtype = lua_type(L, index);
    switch (vtype) {
        case LUA_TNIL:
        case LUA_TNONE:
            value->type = ST_TYPES_NIL;

            break;
        case LUA_TNUMBER:
            i_value = luaL_checkint(L, index);
            d_value = luaL_checknumber(L, index);

            if (i_value == d_value) {
                vbuf->i_value = i_value;
                *value = st_capi_make_tvalue(vbuf->i_value);
            }
            else {
                vbuf->d_value = d_value;
                *value = st_capi_make_tvalue(vbuf->d_value);
            }

            break;
        case LUA_TBOOLEAN:
            vbuf->b_value = (st_bool)lua_toboolean(L, index);
            *value = st_capi_make_tvalue(vbuf->b_value);

            break;
        case LUA_TSTRING:
            vbuf->s_value = (char *)luaL_checkstring(L, index);
            *value = st_capi_make_tvalue(vbuf->s_value);

            break;
        case LUA_TUSERDATA:
            st_luaapi_get_ud_err_ret(L,
                                     vbuf->u_value,
                                     index,
                                     ST_LUA_TABLE_METATABLE);
            *value = vbuf->u_value->table;

            break;
        default:
            derr("type %d is not supported", vtype);

            return ST_ARG_INVALID;
    }

    return ST_OK;
}


static void
st_luaapi_push_table_to_stack(lua_State *L, st_tvalue_t *tvalue)
{
    st_luaapi_ud_t *ud = lua_newuserdata(L, sizeof(*ud));

    luaL_getmetatable(L, ST_LUA_TABLE_METATABLE);
    lua_setmetatable(L, -2);

    ud->table = *tvalue;
}


static int
st_luaapi_push_value_to_stack(lua_State *L, st_tvalue_t *tvalue)
{
    switch(tvalue->type) {
        case ST_TYPES_NIL:
            lua_pushnil(L);

            break;
        case ST_TYPES_STRING:
            lua_pushlstring(L, (const char *)tvalue->bytes, tvalue->len);

            break;
        case ST_TYPES_BOOLEAN:
            lua_pushboolean(L, *((st_bool *)tvalue->bytes));

            break;
        case ST_TYPES_NUMBER:
            lua_pushnumber(L, *((double *)tvalue->bytes));

            break;
        case ST_TYPES_TABLE:
            st_luaapi_push_table_to_stack(L, tvalue);

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
st_luaapi_get_key(lua_State *L, st_table_t *table, int index)
{
    st_tvalue_t key;
    st_tvalue_t value;
    st_luaapi_values_t kbuf;

    int ret = st_luaapi_get_stack_value_info(L, index, &kbuf, &key);
    if (ret != ST_OK) {
        derr("invalid key type for index: %d", ret);

        return ret;
    }

    if (key.type != ST_TYPES_NIL) {
        ret = st_capi_do_get(table, key, &value);

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

    ret = st_luaapi_push_value_to_stack(L, &value);
    if (ret != ST_OK) {
        derr("failed to push value to stack: %d", ret);
    }

    if (value.type != ST_TYPES_NIL && !st_types_is_table(value.type)) {
        st_assert(st_capi_free(&value) == ST_OK);
    }

    return ret;
}


int
st_luaapi_table_index(lua_State *L)
{
    st_luaapi_ud_t *ud = NULL;
    st_luaapi_get_ud_err_ret(L, ud, 1, ST_LUA_TABLE_METATABLE);

    st_table_t *table = st_table_get_table_addr_from_value(ud->table);

    /** the position of key on stack is 2 */
    int ret = st_luaapi_get_key(L, table, 2);
    if (ret != ST_OK) {
        return luaL_error(L, "failed to get value: %d", ret);
    }

    return 1;
}


/** if the value to set is nil, this function removes key in table */
static int
st_luaapi_set_key(lua_State *L, st_table_t *table, int index)
{
    st_tvalue_t key;
    st_tvalue_t value;
    st_luaapi_values_t kbuf;
    st_luaapi_values_t vbuf;

    int ret = st_luaapi_get_stack_value_info(L, index, &kbuf, &key);
    if (ret != ST_OK || key.type == ST_TYPES_NIL) {
        derr("invalid key type for index: %d", ret);

        return luaL_argerror(L, index, "invalid key type");
    }

    ret = st_luaapi_get_stack_value_info(L, index + 1, &vbuf, &value);
    if (ret != ST_OK) {
        derr("failed to get value from stack: %d", ret);

        return luaL_argerror(L, index+1, "failed to get value from stack");
    }

    if (value.type == ST_TYPES_NIL) {
        /** remove */
        ret = st_capi_do_remove_key(table, key);
        ret = (ret == ST_NOT_FOUND ? ST_OK : ret);
    }
    else {
        /** set */
        ret = st_capi_do_add(table, key, value, 1);
    }

    return ret;
}


int
st_luaapi_table_newindex(lua_State *L)
{
    st_luaapi_ud_t *ud = NULL;
    st_luaapi_get_ud_err_ret(L, ud, 1, ST_LUA_TABLE_METATABLE);

    st_table_t *table = st_table_get_table_addr_from_value(ud->table);

    /** the position of key on stack is 2 */
    int ret = st_luaapi_set_key(L, table, 2);
    if (ret != ST_OK) {
        derr("failed to add or remove key/value: %d", ret);

        return luaL_error(L, "failed to add or remove key/value: %d", ret);
    }

    return 0;
}


int
st_luaapi_table_equal(lua_State *L)
{
    st_luaapi_ud_t *ud1 = NULL;
    st_luaapi_ud_t *ud2 = NULL;

    st_luaapi_get_ud_err_ret(L, ud1, 1, ST_LUA_TABLE_METATABLE);
    st_luaapi_get_ud_err_ret(L, ud2, 1, ST_LUA_TABLE_METATABLE);

    st_table_t *tbl1 = st_table_get_table_addr_from_value(ud1->table);
    st_table_t *tbl2 = st_table_get_table_addr_from_value(ud2->table);

    lua_pushnumber(L, (intptr_t)tbl1 == (intptr_t)tbl2);

    return 1;
}


int
st_luaapi_table_gc(lua_State *L)
{
    /** luaL_argcheck does not work in gc */
    st_luaapi_ud_t *ud = luaL_checkudata(L, 1, ST_LUA_TABLE_METATABLE);
    st_assert(ud != NULL);

    int ret = st_capi_free(&ud->table);
    st_assert(ret == ST_OK);

    ud->table = (st_tvalue_t)st_str_null;

    return 0;
}


int
st_luaapi_worker_init(lua_State *L)
{
    int ret = st_capi_worker_init();

    lua_pushnumber(L, ret);

    return 1;
}


int
st_luaapi_destroy(lua_State *L)
{
    int ret = st_capi_destroy();

    lua_pushnumber(L, ret);

    return 1;
}


int
st_luaapi_new(lua_State *L)
{
    st_tvalue_t table = st_str_null;

    int ret = st_capi_new(&table);
    if (ret != ST_OK) {
        lua_pushnil(L);
        lua_pushnumber(L, ret);
        lua_pushliteral(L, "failed to new table");

        return 3;
    }

    st_luaapi_push_table_to_stack(L, &table);

    return 1;
}


static int
st_luaapi_common_iterator(lua_State *L, int is_ipairs)
{
    st_capi_iter_t *iter = NULL;

    st_luaapi_get_ud_err_ret(L, iter, 1, ST_LUA_ITER_METATABLE);

    st_tvalue_t key;
    st_tvalue_t value;

    int ret = st_capi_next(iter, &key, &value);
    if (ret == ST_TABLE_MODIFIED) {
        return luaL_error(L, "table modified during iterate: %d", ret);
    }

    if (ret == ST_ITER_FINISH) {
        return 0;
    }

    st_assert(ret == ST_OK);

    if (is_ipairs && key.type != ST_TYPES_INTEGER) {
        st_assert(st_capi_free(&key));
        st_assert(st_capi_free(&value));

        return 0;
    }

    st_luaapi_push_value_to_stack(L, &key);
    st_luaapi_push_value_to_stack(L, &value);

    st_capi_free(&key);
    if (!st_types_is_table(value.type)) {
        st_capi_free(&value);
    }

    return 2;
}


static int
st_luaapi_ipairs_iterator(lua_State *L)
{
    return st_luaapi_common_iterator(L, 1);
}


static int
st_luaapi_pairs_iterator(lua_State *L)
{
    return st_luaapi_common_iterator(L, 0);
}


static int
st_luaapi_common_init_iter(lua_State *L,
                           st_tvalue_t *init_key,
                           int expected_side,
                           lua_CFunction iter_func)
{
    st_luaapi_ud_t *ud = NULL;
    st_luaapi_get_ud_err_ret(L, ud, 1, ST_LUA_TABLE_METATABLE);

    lua_pushcfunction(L, iter_func);

    st_capi_iter_t *iter = lua_newuserdata(L, sizeof(*iter));
    luaL_getmetatable(L, ST_LUA_ITER_METATABLE);
    lua_setmetatable(L, -2);

    int ret = st_capi_init_iterator(&ud->table, iter, init_key, expected_side);
    if (ret != ST_OK) {
        return luaL_error(L, "failed to init iterator: %d", ret);
    }

    /** control variable as nil */
    return 2;
}


int
st_luaapi_ipairs(lua_State *L)
{
    int start = 1;
    st_tvalue_t init_key = st_capi_make_tvalue(start);

    return st_luaapi_common_init_iter(L,
                                      &init_key,
                                      ST_SIDE_RIGHT_EQ,
                                      st_luaapi_ipairs_iterator);
}


int
st_luaapi_pairs(lua_State *L)
{
    return st_luaapi_common_init_iter(L, NULL, 0, st_luaapi_pairs_iterator);
}


int
st_luaapi_gc(lua_State *L)
{
    st_capi_process_t *pstate = st_capi_get_process_state();

    int ret = st_gc_run(&pstate->lib_state->table_pool.gc);
    if (ret != ST_OK && ret != ST_NO_GC_DATA) {
        derr("run gc failed: %d", ret);
    }

    lua_pushnumber(L, ret);

    return 1;
}


int
st_luaapi_proc_crash_detection(lua_State *L)
{
    st_tvalue_t num;
    st_luaapi_values_t value;

    /** the position of recycle number on stack is 1 */
    int ret = st_luaapi_get_stack_value_info(L, 1, &value, &num);
    if (ret != ST_OK) {
        return luaL_error(L, "failed to get num from stack: %d", ret);
    }

    if (num.type == ST_TYPES_NIL) {
        value.i_value = 0;
        num.type = ST_TYPES_INTEGER;
    }

    if (num.type != ST_TYPES_INTEGER) {
        return luaL_error(L, "invalid max number type: %d, %d", ret, num.type);
    }

    int recycled = 0;
    ret = st_capi_clean_dead_proot(value.i_value, &recycled);
    if (ret != ST_OK) {
        derr("failed to run proc crash detection: %d", ret);
    }

    lua_pushnumber(L, (ret == ST_OK ? recycled : -1));

    return 1;
}


/** metamethods of iterator */
static const luaL_Reg st_luaapi_iter_metamethods[] = {
    { "__gc", st_luaapi_iter_gc },

    { NULL,   NULL           },
};


/** metamethods of share table object */
static const luaL_Reg st_luaapi_table_metamethods[] = {
    /** get array length */
    { "__len",      st_luaapi_table_len      },
    /** get key/value from table */
    { "__index",    st_luaapi_table_index    },
    /** set key/value to table */
    { "__newindex", st_luaapi_table_newindex },
    /** equals */
    { "__eq",       st_luaapi_table_equal    },
    /** remove table from proot */
    { "__gc",       st_luaapi_table_gc       },

    { NULL,         NULL                  },
};


static int
st_luaapi_module_init(lua_State *L)
{
    st_tvalue_t value;
    st_luaapi_values_t fn;

    /** 1st arg in stack is shared memory file name */
    int ret = st_luaapi_get_stack_value_info(L, 1, &fn, &value);
    if (ret != ST_OK || value.type != ST_TYPES_STRING) {
        derr("invalid shm fn type for index: %d, %ld", ret, value.type);

        return luaL_argerror(L, 1, "invalid shm fn type");
    }

    ret = st_capi_init(fn.s_value);
    if (ret != ST_OK) {
        return luaL_error(L, "failed to init module: %d, %s", ret, fn.s_value);
    }

    return 0;
}


int
st_luaapi_get_registry(lua_State *L)
{
    st_tvalue_t groot = st_str_null;

    int ret = st_capi_get_groot(&groot);
    if (ret != ST_OK) {
        return luaL_error(L, "failed to get registry: %d", ret);
    }

    ret = st_luaapi_push_value_to_stack(L, &groot);
    if (ret != ST_OK) {
        st_assert_ok(st_capi_free(&groot), "failed to release registry");

        return luaL_error(L, "failed to push registry to stack: %d", ret);
    }

    return 1;
}


/** module methods */
static const luaL_Reg st_luaapi_module_methods[] = {
    /** module init */
    { "module_init",          st_luaapi_module_init          },
    /** worker init */
    { "worker_init",          st_luaapi_worker_init          },
    /** destroy the whole module */
    { "destroy",              st_luaapi_destroy              },
    /** get groot */
    { "get_registry",         st_luaapi_get_registry         },
    /** create a table */
    { "new",                  st_luaapi_new                  },
    /** iterate array */
    { "ipairs",               st_luaapi_ipairs               },
    /** iterate dictionary */
    { "pairs",                st_luaapi_pairs                },
    /** force triger gc */
    { "gc",                   st_luaapi_gc                   },
    /** monitor process crash */
    { "proc_crash_detection", st_luaapi_proc_crash_detection },

    { NULL,                   NULL                           },
};


int
luaopen_libluast(lua_State *L)
{
    /** register metatable for capi ud in lua */
    luaL_newmetatable(L, ST_LUA_TABLE_METATABLE);
    luaL_register(L, NULL, st_luaapi_table_metamethods);

    /** register metatable for capi iterator in lua */
    luaL_newmetatable(L, ST_LUA_ITER_METATABLE);
    luaL_register(L, NULL, st_luaapi_iter_metamethods);

    /** module lua table */
    lua_newtable(L);
    luaL_register(L, ST_LUA_MODULE_NAME, st_luaapi_module_methods);

    return 1;
}
