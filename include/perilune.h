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

#pragma region push
#pragma endregion

#pragma region get tuple
void perilune_getvalue(lua_State *L, int index, int *value)
{
    *value = (int)luaL_checkinteger(L, index);
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
public:
#pragma region Type
    std::string GetTypeName() const
    {
        static std::string name = std::string("Type ") + typeid(T).name();
        return name;
    }
    static int TypeStackToUpvalue(lua_State *L)
    {
        lua_pushcclosure(L, &ValueType::TypeDispatch, 2);
        return 1;
    }
    typedef std::function<int(lua_State *)> LuaMethod;
    std::unordered_map<std::string, LuaMethod> m_typeMap;
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
            self->perilune_pushvalue(L, t);
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

    static ValueType *GetType(lua_State *L, int index)
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
        auto type = GetType(L, lua_upvalueindex(1));
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

    void LuaNewTypeMetaTable(lua_State *L)
    {
        auto name = GetTypeName();
        assert(luaL_newmetatable(L, name.c_str()) == 1);
        int metatable = lua_gettop(L);

        lua_pushcfunction(L, &ValueType::TypeStackToUpvalue);
        lua_setfield(L, metatable, "__index");
    }

    void PushType(lua_State *L)
    {
        // type userdata
        auto p = (ValueType **)lua_newuserdata(L, sizeof(this));
        *p = this;

        // set metatable to type userdata
        LuaNewTypeMetaTable(L);
        lua_setmetatable(L, -2);

        // 
        LuaNewInstanceMetaTable(L);
        lua_pop(L, 1);
    }
#pragma endregion

#pragma region Value
    std::string GetInstanceName() const
    {
        static std::string name = typeid(T).name();
        return name;
    }
    // static int InstanceStackToUpvalue(lua_State *L)
    // {
    //     lua_pushcclosure(L, &ValueType::InstanceDispatch, 2);
    //     return 1;
    // }
    std::unordered_map<std::string, LuaMethod> m_getterMap;
    struct ValueWithType
    {
        T Value;
        ValueType *Type;
    };

    template <typename F, typename C, typename R>
    void _Getter(const char *name, const F &f, R (C::*)(const T &value) const)
    {
        LuaMethod func = [f](lua_State *L) {
            
            auto self = GetValue(L, 1);
            T &value = self->Value;
            R r = f(value);

            int result = self->Type->perilune_pushvalue(L, r);
            return result;
        };
        m_getterMap.insert(std::make_pair(name, func));
    }

    template <typename F>
    ValueType &Getter(const char *name, F f)
    {
        _Getter(name, f, &decltype(f)::operator());
        return *this;
    }

    // for field
    template <typename C, typename R>
    ValueType &Getter(const char *name, R C::*f)
    {
        // auto self = this;
        LuaMethod func = [f](lua_State *L) {
            
             auto self = GetValue(L, 1);
             T &value = self->Value;

             R r = value.*f;

            int result = self->Type->perilune_pushvalue(L, r);
            return result;
        };
        m_getterMap.insert(std::make_pair(name, func));

        return *this;
    }

    static ValueWithType *GetValue(lua_State *L, int index)
    {
        return static_cast<ValueWithType *>(lua_touserdata(L, index));
    }

    // upvalue 1:table(userdata), 2:key
    static int InstanceDispatch(lua_State *L)
    {
        auto value = GetValue(L, 1);
        auto type = value->Type;
        auto key = lua_tostring(L, 2);

        auto found = type->m_getterMap.find(key);
        if (found == type->m_getterMap.end())
        {
            lua_pushfstring(L, "no %s method", key);
            lua_error(L);
            return 1;
        }

        return found->second(L);
    }

    void LuaNewInstanceMetaTable(lua_State *L)
    {
        auto name = GetInstanceName();
        luaL_newmetatable(L, name.c_str());

        // first time
        int metatable = lua_gettop(L);
        lua_pushcfunction(L, &ValueType::InstanceDispatch);
        lua_setfield(L, metatable, "__index");
    }

    /// <summary>
    /// construct instance value(push T to stack)
    /// </summary>
    int perilune_pushvalue(lua_State *L, const T &value)
    {
        // type userdata
        auto p = (ValueWithType *)lua_newuserdata(L, sizeof(ValueWithType));
        p->Type = this;
        p->Value = value;

        // set metatable to type userdata
        luaL_getmetatable(L, GetInstanceName().c_str());
        lua_setmetatable(L, -2);

        return 1;
    }

    int perilune_pushvalue(lua_State *L, int n)
    {
        lua_pushinteger(L, n);
        return 1;
    }

    int perilune_pushvalue(lua_State *L, float n)
    {
        lua_pushnumber(L, n);
        return 1;
    }
#pragma endregion
};

} // namespace perilune