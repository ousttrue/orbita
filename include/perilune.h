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
static int perilune_pushvalue(lua_State *L, int n)
{
    lua_pushinteger(L, n);
    return 1;
}

static int perilune_pushvalue(lua_State *L, float n)
{
    lua_pushnumber(L, n);
    return 1;
}
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
    struct ValueWithType
    {
        T Value;
        const ValueType *Type;
    };

public:
#pragma region Type
    std::string GetTypeName() const
    {
        static std::string name = std::string("Type ") + typeid(T).name();
        return name;
    }

    // arguments is in stack
    typedef std::function<int(lua_State *)> LuaStaticMethod;

    std::unordered_map<std::string, LuaStaticMethod> m_typeMap;
    template <typename F, typename C, typename R, typename... ARGS, std::size_t... IS>
    void __StaticMethod(const char *name,
                        const F &f,
                        R (C::*m)(ARGS...) const,
                        std::index_sequence<IS...>)
    {
        auto type = this;
        auto func = [type, f](lua_State *L) {
            auto args = perilune_totuple<ARGS...>(L, 1);
            auto t = f(std::get<IS>(args)...);
            PushValue(L, t, type);
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

    // stack 1:table(userdata), 2:key
    static int TypeIndexDispatch(lua_State *L)
    {
        lua_pushcclosure(L, &ValueType::TypeMethodDispatch, 2);
        return 1;
    }

    // upvalue 1:table(userdata), 2:key
    static int TypeMethodDispatch(lua_State *L)
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

        lua_pushcfunction(L, &ValueType::TypeIndexDispatch);
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
    static std::string GetInstanceName()
    {
        static std::string name = typeid(T).name();
        return name;
    }
    // static int InstanceStackToUpvalue(lua_State *L)
    // {
    //     lua_pushcclosure(L, &ValueType::InstanceDispatch, 2);
    //     return 1;
    // }

    typedef std::function<int(lua_State *, ValueWithType *)> LuaMethod;
    std::unordered_map<std::string, LuaMethod> m_getterMap;
    std::unordered_map<std::string, LuaMethod> m_methodMap;
    ;

    template <typename F, typename C, typename R>
    void _Getter(const char *name, const F &f, R (C::*)(const T &value) const)
    {
        LuaMethod func = [f](lua_State *L, ValueWithType *self) {
            R r = f(self->Value);

            int result = PushValue(L, r, self->Type);
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
        LuaMethod func = [f](lua_State *L, ValueWithType *self) {
            R r = self->Value.*f;

            int result = PushValue(L, r, self->Type);
            return result;
        };
        m_getterMap.insert(std::make_pair(name, func));

        return *this;
    }

    template <typename C, typename R, typename... ARGS, std::size_t... IS>
    void __Method(const char *name,
                  R (C::*m)(ARGS...) const,
                  std::index_sequence<IS...>)
    {
        // auto self = this;
        LuaMethod func = [m](lua_State *L, ValueWithType *self) {
            auto args = perilune_totuple<ARGS...>(L, 1);

            R r = (self->Value.*m)(std::get<IS>(args)...);

            return self->Type->PushValue(L, r, self->Type);
        };
        m_methodMap.insert(std::make_pair(name, func));
    }

    template <typename C, typename R, typename... ARGS>
    ValueType &Method(const char *name, R (C::*m)(ARGS...) const)
    {
        __Method(name, m, std::index_sequence_for<ARGS...>());
        return *this;
    }

    static ValueWithType *GetSelf(lua_State *L, int index)
    {
        return static_cast<ValueWithType *>(lua_touserdata(L, index));
    }

    // stack 1:table(userdata), 2:key
    static int InstanceIndexDispatch(lua_State *L)
    {
        auto self = GetSelf(L, 1);
        auto type = self->Type;
        auto key = lua_tostring(L, 2);

        {
            auto found = type->m_getterMap.find(key);
            if (found != type->m_getterMap.end())
            {
                // property
                return found->second(L, self);
            }
        }

        lua_pushcclosure(L, &ValueType::InstanceMethodDispatch, 2);
        return 1;
    }

    // upvalue 1:table(userdata), 2:key
    static int InstanceMethodDispatch(lua_State *L)
    {
        auto self = GetSelf(L, lua_upvalueindex(1));
        auto type = self->Type;
        auto key = lua_tostring(L, lua_upvalueindex(2));

        {
            auto found = type->m_methodMap.find(key);
            if (found != type->m_methodMap.end())
            {
                // property
                return found->second(L, self);
            }
        }

        // error
        lua_pushfstring(L, "no %s method", key);
        lua_error(L);
        return 1;
    }

    void LuaNewInstanceMetaTable(lua_State *L)
    {
        auto name = GetInstanceName();
        luaL_newmetatable(L, name.c_str());

        // first time
        int metatable = lua_gettop(L);
        lua_pushcfunction(L, &ValueType::InstanceIndexDispatch);
        lua_setfield(L, metatable, "__index");
    }

    template <typename R>
    static int PushValue(lua_State *L, R value, const ValueType *type)
    {
        return perilune_pushvalue(L, value);
    }

    static int PushValue(lua_State *L, const T &value, const ValueType *type)
    {
        return PushValue(L, ValueWithType{
                                .Value = value,
                                .Type = type,
                            });
    }

    /// <summary>
    /// construct instance value(push T to stack)
    /// </summary>
    static int PushValue(lua_State *L, const ValueWithType &self)
    {
        // type userdata
        auto p = (ValueWithType *)lua_newuserdata(L, sizeof(ValueWithType));
        *p = self;

        // set metatable to type userdata
        luaL_getmetatable(L, GetInstanceName().c_str());
        lua_setmetatable(L, -2);

        return 1;
    }
#pragma endregion
};

} // namespace perilune