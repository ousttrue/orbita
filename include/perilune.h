#pragma once

extern "C"
{
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

#include <functional>
#include <unordered_map>
#include <string>
#include <tuple>
#include <assert.h>

namespace perilune
{

#pragma region tuple
void perilune_setvalue(lua_State *L, int n)
{
    lua_pushinteger(L, n);
}

void perilune_getvalue(lua_State *L, int index, int *value)
{
    *value = (int)luaL_checkinteger(L, index);
}

void perilune_setvalue(lua_State *L, float n)
{
    lua_pushnumber(L, n);
}

void perilune_getvalue(lua_State *L, int index, float *value)
{
    *value = (float)luaL_checknumber(L, index);
}

std::tuple<> perilune_totuple(lua_State *L, int index, std::tuple<> *)
{
    return std::tuple<>();
}

template <typename A, typename... ARGS>
std::tuple<A, ARGS...> perilune_totuple(lua_State *L, int index, std::tuple<A, ARGS...> *)
{
    A a;
    perilune_getvalue(L, index, &a);
    std::tuple<A> t = std::make_tuple(a);
    return std::tuple_cat(std::move(t),
                          perilune_totuple<ARGS...>(L, index + 1));
}

template <typename... ARGS>
std::tuple<ARGS...> perilune_totuple(lua_State *L, int index)
{
    std::tuple<ARGS...> *p = nullptr;
    return perilune_totuple(L, index, p);
}
#pragma endregion

template <typename T>
class ValueType
{
    // stack 1:table(userdata), 2:key
    static int StackToUpvalue(lua_State *L)
    {
        lua_pushcclosure(L, &ValueType::TypeDispatch, 2);
        return 1;
    }

public:
#pragma region Type
    typedef std::function<int(lua_State *)> LuaTypeMethod;
    std::unordered_map<std::string, LuaTypeMethod> m_typeMap;
    template <typename F, typename C, typename R, typename... ARGS, std::size_t... IS>
    void __StaticMethod(const char *name,
                        const F &f,
                        R (C::*m)(ARGS...) const,
                        std::index_sequence<IS...>)
    {
        auto self = this;
        auto func = [self, f](lua_State *L) {
            auto args = perilune_totuple<ARGS...>(L, 1);
            auto t = f(std::get<IS>(args)...);
            self->PushValue(L, t);
            return 1;
        };
        m_typeMap.insert(std::make_pair(name, func));
    }

    template <typename F, typename C, typename R, typename... ARGS>
    void _StaticMethod(const char *name, const F &f, R (C::*m)(ARGS...) const)
    {
        __StaticMethod(name, f, m,
                       std::index_sequence_for<ARGS...>());
    }

    template <typename F>
    ValueType &StaticMethod(const char *name, F f)
    {
        _StaticMethod(name, f, &decltype(f)::operator());
        return *this;
    }

    static ValueType *ToTypeData(lua_State *L, int index)
    {
        auto obj = static_cast<ValueType **>(lua_touserdata(L, index));
        if (!obj)
        {
            return nullptr;
        }
        return *obj;
    }

    // upvalue 1:table(userdata), 2:key
    static int TypeDispatch(lua_State *L)
    {
        auto type = ToTypeData(L, lua_upvalueindex(1));
        auto key = lua_tostring(L, lua_upvalueindex(2));

        auto found = type->m_typeMap.find(key);
        if (found == type->m_typeMap.end())
        {
            lua_pushfstring(L, "no %s method", key);
            lua_error(L);
            return 1;
        }

        return found->second(L);
    }

    void LuaPushTypeMetaTable(lua_State *L)
    {
        auto name = typeid(T).name();
        assert(luaL_newmetatable(L, name) == 1);
        int metatable = lua_gettop(L);

        lua_pushcfunction(L, &ValueType::StackToUpvalue);
        lua_setfield(L, metatable, "__index");
    }

    void PushType(lua_State *L)
    {
        // type userdata
        auto p = (ValueType **)lua_newuserdata(L, sizeof(this));
        *p = this;

        // set metatable to type userdata
        LuaPushTypeMetaTable(L);
        lua_setmetatable(L, -2);
    }
#pragma endregion

#pragma region Value
    struct ValueWithType
    {
        T Value;
        ValueType *Type;
    };

    /// <summary>
    /// construct instance value(push T to stack)
    /// </summary>
    void PushValue(lua_State *L, const T &value)
    {
        // type userdata
        auto p = (ValueWithType *)lua_newuserdata(L, sizeof(ValueWithType));
        p->Type = this;
        p->Value = value;
    }
#pragma endregion
};

} // namespace perilune