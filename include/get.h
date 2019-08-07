#pragma once
#include <sstream>
#include "string_win32.h"

namespace perilune
{

template <typename T>
struct LuaTable
{
    static T Get(lua_State *L, int index)
    {
        std::stringstream ss;
        ss << "LuaTable<" << typeid(T).name() << "> is not implemented";
        throw std::exception(ss.str().c_str());
    }
};

template <typename T>
struct LuaGet
{
    static T Get(lua_State *L, int index)
    {
        auto t = lua_type(L, index);
        if (t == LUA_TUSERDATA)
        {
            return *(T *)lua_touserdata(L, index);
        }
        else if (t == LUA_TTABLE)
        {
            return LuaTable<T>::Get(L, index);
        }
        else
        {
            std::stringstream ss;
            ss << "LuaGet<" << typeid(T).name() << "> is " << lua_typename(L, t);
            throw std::exception(ss.str().c_str());
        }
    }
};
template <typename T>
struct LuaGet<T *>
{
    static T *Get(lua_State *L, int index)
    {
        auto t = lua_type(L, index);
        if (t == LUA_TUSERDATA)
        {
            // auto p = (T *)lua_touserdata(L, index);
            // return p;
            return Traits<T *>::GetSelf(L, index);
        }
        else if (t == LUA_TLIGHTUSERDATA)
        {
            auto p = (T *)lua_touserdata(L, index);
            return p;
        }
        else
        {
            // return nullptr;
            std::stringstream ss;
            ss << "LuaGet<" << typeid(T).name() << "*> is " << lua_typename(L, t);
            throw std::exception(ss.str().c_str());
        }
    }
};
template <>
struct LuaGet<int>
{
    static int Get(lua_State *L, int index)
    {
        return (int)luaL_checkinteger(L, index);
    }
};
template <>
struct LuaGet<float>
{
    static float Get(lua_State *L, int index)
    {
        return (float)luaL_checknumber(L, index);
    }
};
template <>
struct LuaGet<void *>
{
    static void *Get(lua_State *L, int index)
    {
        return const_cast<void *>(lua_touserdata(L, index));
    }
};
template <>
struct LuaGet<std::string>
{
    static std::string Get(lua_State *L, int index)
    {
        auto str = luaL_checkstring(L, index);
        if (str)
        {
            return std::string(str);
        }
        else
        {
            return "";
        }
    }
};

template <>
struct LuaGet<std::wstring>
{
    static std::wstring Get(lua_State *L, int index)
    {
        auto str = luaL_checkstring(L, index);
        return utf8_to_wstring(str);
    }
};

template <typename... ARGS>
struct LuaGet<std::tuple<ARGS...>>
{
    static std::tuple<ARGS...> Get(lua_State *L, int index)
    {
        return LuaTableToTuple<ARGS...>(L, index, 1);
    }
};

#pragma region LuaArgsToTuple
inline std::tuple<> LuaArgsToTuple(lua_State *L, int index, std::tuple<> *)
{
    return std::tuple<>();
}

template <typename A, typename... ARGS>
std::tuple<A, ARGS...> LuaArgsToTuple(lua_State *L, int index, std::tuple<A, ARGS...> *)
{
    A a = LuaGet<A>::Get(L, index);
    std::tuple<A> t = std::make_tuple(a);
    return std::tuple_cat(std::move(t),
                          LuaArgsToTuple<ARGS...>(L, index + 1));
}

template <typename... ARGS>
std::tuple<ARGS...> LuaArgsToTuple(lua_State *L, int index)
{
    std::tuple<ARGS...> *p = nullptr;
    return LuaArgsToTuple(L, index, p);
}

#pragma endregion

#pragma region LuaTableToTuple
template <typename T>
static T LuaTableGet(lua_State *L, int index, int tableIndex)
{
    lua_pushinteger(L, tableIndex);
    lua_gettable(L, index);
    auto t = LuaGet<T>::Get(L, -1);
    lua_pop(L, 1);
    return t;
}

inline std::tuple<> LuaTableToTuple(lua_State *L, int index, int tableIndex, std::tuple<> *)
{
    return std::tuple<>();
}

template <typename A, typename... ARGS>
std::tuple<A, ARGS...> LuaTableToTuple(lua_State *L, int index, int tableIndex, std::tuple<A, ARGS...> *)
{
    A a = LuaTableGet<A>(L, index, tableIndex);
    std::tuple<A> t = std::make_tuple(a);
    return std::tuple_cat(std::move(t),
                          LuaTableToTuple<ARGS...>(L, index, tableIndex + 1));
}

template <typename... ARGS>
std::tuple<ARGS...> LuaTableToTuple(lua_State *L, int index, int tableIndex = 1)
{
    std::tuple<ARGS...> *p = nullptr;
    return LuaTableToTuple(L, index, tableIndex, p);
}

#pragma endregion

} // namespace perilune
