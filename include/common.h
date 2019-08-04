#pragma once

extern "C"
{
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

#include <assert.h>
#include <exception>
#include <functional>

namespace perilune
{

enum class MetaKey
{
    __gc,
    __len,
    __ipairs,
    __pairs,
    __tostring,
    __call,
    __newindex,
    __concat,
    // __index, use IndexDispatcher
};

static const char *ToString(MetaKey key)
{
    switch (key)
    {
    case MetaKey::__gc:
        return "__gc";
    case MetaKey::__len:
        return "__len";
    case MetaKey::__ipairs:
        return "__ipairs";
    case MetaKey::__pairs:
        return "__pairs";
    case MetaKey::__tostring:
        return "__tostring";
    case MetaKey::__call:
        return "__call";
    case MetaKey::__newindex:
        return "__newindex";
    case MetaKey::__concat:
        return "__concat";
    }

    throw std::exception("unknown key");
}

template <typename T>
struct MetatableName
{
    static const char *TypeName()
    {
        return typeid(MetatableName).name();
    }

    static const char *InstanceName()
    {
        return typeid(T).name();
    }
};

// normal type
template <typename T>
struct Traits
{
    using RawType = T;

    static RawType *GetSelf(lua_State *L, int index)
    {
        auto p = (T *)lua_touserdata(L, index);
        return p;
    }

    // value type. only full userdata
    static T *GetData(lua_State *L, int index)
    {
        return (T *)lua_touserdata(L, index);
    }
};

// for pointer type
template <typename T>
struct Traits<T *>
{
    using RawType = T;

    using PT = T *;

    static RawType *GetSelf(lua_State *L, int index)
    {
        return *(PT *)lua_touserdata(L, index);
    }

    static T **GetData(lua_State *L, int index)
    {
        return (PT *)lua_touserdata(L, index);
    }
};


using LuaFunc = std::function<int(lua_State *)>;
static int LuaFuncClosure(lua_State *L)
{
    try
    {
        // execute logic from upvalue
        auto func = (LuaFunc *)lua_touserdata(L, lua_upvalueindex(1));
        return (*func)(L);
    }
    catch (const std::exception &ex)
    {
        lua_pushfstring(L, ex.what());
        lua_error(L);
        return 1;
    }
    catch (...)
    {
        lua_pushfstring(L, "error in closure");
        lua_error(L);
        return 1;
    }
}

} // namespace perilune