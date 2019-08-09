#pragma once

extern "C"
{
#include <lua.h>
}

#include <stdint.h>
#include <string>
#include <vector>

namespace perilune
{

template <typename T>
struct LuaPush
{
    template <typename... ARGS, std::size_t... IS>
    static int _New(lua_State *L, std::tuple<ARGS...> args, std::index_sequence<IS...>)
    {
        auto p = (T *)lua_newuserdata(L, sizeof(T));
        // memset(p, 0, sizeof(T));
        auto pushedType = LuaGetMetatable<T>(L);
        if (pushedType)
        {
            // set metatable to type userdata
            lua_setmetatable(L, -2);
            new (p) T(std::get<IS>(args)...); // initialize. see Traits::Destruct
            return 1;
        }
        else
        {
            // no metatable
            lua_pop(L, 1);

            // error
            lua_pushfstring(L, "push unknown type [%s]", typeid(T).name());
            lua_error(L);
            return 1;
        }
    }

    template <typename... ARGS>
    static int New(lua_State *L, std::tuple<ARGS...> args)
    {
        return _New(L, args, std::index_sequence_for<ARGS...>());
    }

    static int Push(lua_State *L, const T &value)
    {
        auto p = (T *)lua_newuserdata(L, sizeof(T));
        // memset(p, 0, sizeof(T));
        auto pushedType = LuaGetMetatable<T>(L);
        if (pushedType)
        {
            // set metatable to type userdata
            lua_setmetatable(L, -2);
            new (p) T; // initialize. see Traits::Destruct
            *p = value;
            return 1;
        }
        else
        {
            // no metatable
            lua_pop(L, 1);

            // error
            lua_pushfstring(L, "push unknown type [%s]", typeid(T).name());
            lua_error(L);
            return 1;
        }
    }
};

template <typename T>
struct LuaPush<T *>
{
    using PT = T *;
    static int Push(lua_State *L, T *value)
    {
        if (!value)
        {
            return 0;
        }

        auto p = (PT *)lua_newuserdata(L, sizeof(T *));
        auto pushedType = LuaGetMetatable<T *>(L);
        if (pushedType)
        {
            // set metatable to type userdata
            lua_setmetatable(L, -2);
            *p = value;
            return 1;
        }
        else
        {
            // no metatable
            lua_pop(L, 1);

            lua_pushlightuserdata(L, (void *)value);
            return 1;
        }
    }
};

template <typename T>
struct LuaPush<T &>
{
    using PT = T *;
    static int Push(lua_State *L, const T &value)
    {
        auto p = (PT *)lua_newuserdata(L, sizeof(T));
        memset(p, 0, sizeof(T));
        auto pushedType = LuaGetMetatable<T *>(L);
        if (pushedType)
        {
            // set metatable to type userdata
            lua_setmetatable(L, -2);
            *p = &value;
            return 1;
        }
        else
        {
            // no metatable
            lua_pop(L, 1);

            auto pvalue = &value;
            lua_pushlightuserdata(L, (void *)pvalue);
            return 1;
        }
    }
};

template <>
struct LuaPush<bool>
{
    static int Push(lua_State *L, bool b)
    {
        lua_pushboolean(L, b);
        return 1;
    }
};

template <>
struct LuaPush<int32_t>
{
    static int Push(lua_State *L, int32_t n)
    {
        lua_pushinteger(L, n);
        return 1;
    }
};

template <>
struct LuaPush<int64_t>
{
    static int Push(lua_State *L, int64_t n)
    {
        lua_pushinteger(L, n);
        return 1;
    }
};

template <>
struct LuaPush<uint64_t>
{
    static int Push(lua_State *L, uint64_t n)
    {
        lua_pushinteger(L, n);
        return 1;
    }
};

template <>
struct LuaPush<float>
{
    static int Push(lua_State *L, float n)
    {
        lua_pushnumber(L, n);
        return 1;
    }
};

template <>
struct LuaPush<double>
{
    static int Push(lua_State *L, double n)
    {
        lua_pushnumber(L, n);
        return 1;
    }
};

template <>
struct LuaPush<void *>
{
    static int Push(lua_State *L, void *n)
    {
        lua_pushlightuserdata(L, n);
        return 1;
    }
};

template <>
struct LuaPush<std::string>
{
    static int Push(lua_State *L, const std::string &s)
    {
        lua_pushstring(L, s.c_str());
        return 1;
    }
};

template <typename T>
struct LuaIndexer
{
    static int Push(lua_State *L, T *t, lua_Integer luaIndex)
    {
        lua_pushfstring(L, "no integer index getter");
        lua_error(L);
        return 1;
    }
};

template <typename T>
struct LuaIndexer<std::vector<T>>
{
    static int Push(lua_State *L, std::vector<T> *t, lua_Integer luaIndex)
    {
        auto index = luaIndex - 1;
        if (index < 0)
            return 0;
        if (index >= (lua_Integer)t->size())
            return 0;

        auto value = (*t)[index];
        return LuaPush<T>::Push(L, value);
    }
};

} // namespace perilune
