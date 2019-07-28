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

template <typename T>
static int perilune_pushvalue(lua_State *L, const T &t)
{
    return ValueType<T>::PushValue(L, t);
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
class UserData
{
    UserData() = delete;

public:
    static T *New(lua_State *L, const T &value, const char *metatableName)
    {
        auto p = (T *)lua_newuserdata(L, sizeof(T));
        *p = value;

        // set metatable to type userdata
        luaL_getmetatable(L, metatableName);
        lua_setmetatable(L, -2);

        return p;
    }

    static T *Get(lua_State *L, int index)
    {
        return static_cast<T *>(lua_touserdata(L, index));
    }
};

class StaticMethodMap
{
    // arguments is in stack
    typedef std::function<int(lua_State *)> StaticMethod;

    std::unordered_map<std::string, StaticMethod> m_typeMap;

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
            perilune_pushvalue(L, t);
            return 1;
        };
        m_typeMap.insert(std::make_pair(name, func));
    }

public:
    template <typename F, typename C, typename R, typename... ARGS>
    void _StaticMethod(const char *name, const F &f, R (C::*m)(ARGS...) const)
    {
        __StaticMethod(name, f, m,
                       std::index_sequence_for<ARGS...>());
    }

    StaticMethod Get(const char *key)
    {
        auto found = m_typeMap.find(key);
        if (found == m_typeMap.end())
        {
            return StaticMethod();
        }
        return found->second;
    }
};

template <typename T>
class PropertyMap
{
    typedef std::function<int(lua_State *)> PropertyMethod;
    std::unordered_map<std::string, PropertyMethod> m_getterMap;

public:
    template <typename F, typename C, typename R>
    void _Getter(const char *name, const F &f, R (C::*)(const T &value) const)
    {
        PropertyMethod func = [f](lua_State *L) {
            auto value = UserData<T>::Get(L, 1);
            R r = f(*value);

            int result = perilune_pushvalue(L, r);
            return result;
        };
        m_getterMap.insert(std::make_pair(name, func));
    }

    // for field
    template <typename C, typename R>
    void _Getter(const char *name, R C::*f)
    {
        // auto self = this;
        PropertyMethod func = [f](lua_State *L) {
            auto &value = *UserData<T>::Get(L, 1);
            R r = value.*f;

            int result = perilune_pushvalue(L, r);
            return result;
        };
        m_getterMap.insert(std::make_pair(name, func));
    }

    PropertyMethod Get(const char *key)
    {
        auto found = m_getterMap.find(key);
        if (found == m_getterMap.end())
        {
            return PropertyMethod();
        }
        return found->second;
    }
};

template <typename T>
class MethodMap
{
    typedef std::function<int(lua_State *)> LuaMethod;
    std::unordered_map<std::string, LuaMethod> m_methodMap;
    ;

public:
    template <typename C, typename R, typename... ARGS, std::size_t... IS>
    void __Method(const char *name,
                  R (C::*m)(ARGS...) const,
                  std::index_sequence<IS...>)
    {
        // auto self = this;
        LuaMethod func = [m](lua_State *L) {
            auto value = UserData<T>::Get(L, lua_upvalueindex(1)); // from upvalue
            auto args = perilune_totuple<ARGS...>(L, 1);

            R r = (value->*m)(std::get<IS>(args)...);

            return perilune_pushvalue(L, r);
        };
        m_methodMap.insert(std::make_pair(name, func));
    }

    LuaMethod Get(const char *key)
    {
        auto found = m_methodMap.find(key);
        if (found == m_methodMap.end())
        {
            return LuaMethod();
        }
        return found->second;
    }
}; // namespace perilune

template <typename T>
class ValueType
{
    // userdata dummy for Type
    struct TypeUserData
    {
    };

    static const char *TypeMetatableName()
    {
        return typeid(ValueType).name();
    }

    static const char *InstanceMetatableName()
    {
        return typeid(T).name();
    }

    StaticMethodMap m_staticMethods;

    // stack 1:table(userdata), 2:key
    static int TypeIndexDispatch(lua_State *L)
    {
        lua_pushcclosure(L, &ValueType::TypeMethodDispatch, 2);
        return 1;
    }

    // upvalue 1:table(userdata), 2:key
    static int TypeMethodDispatch(lua_State *L)
    {
        auto type = GetFromRegistry(L);
        auto key = lua_tostring(L, lua_upvalueindex(2));

        auto callback = type->m_staticMethods.Get(key);
        if (callback)
        {
            return callback(L);
        }

        lua_pushfstring(L, "no %s method", key);
        lua_error(L);
        return 1;
    }

    static void LuaNewTypeMetaTable(lua_State *L)
    {
        assert(luaL_newmetatable(L, TypeMetatableName()) == 1);
        int metatable = lua_gettop(L);

        lua_pushcfunction(L, &ValueType::TypeIndexDispatch);
        lua_setfield(L, metatable, "__index");
    }

    PropertyMap<T> m_propertyMap;
    MethodMap<T> m_methodMap;

    // stack 1:table(userdata), 2:key
    static int InstanceIndexDispatch(lua_State *L)
    {
        auto type = GetFromRegistry(L);
        auto key = lua_tostring(L, 2);

        {
            auto property = type->m_propertyMap.Get(key);
            if (property)
            {
                return property(L);
            }
        }

        lua_pushcclosure(L, &ValueType::InstanceMethodDispatch, 2);
        return 1;
    }

    // upvalue 1:table(userdata), 2:key
    static int InstanceMethodDispatch(lua_State *L)
    {
        auto type = GetFromRegistry(L);
        auto key = lua_tostring(L, lua_upvalueindex(2));

        auto callback = type->m_methodMap.Get(key);
        if (callback)
        {
            return callback(L);
        }

        // error
        lua_pushfstring(L, "no %s method", key);
        lua_error(L);
        return 1;
    }

    void LuaNewInstanceMetaTable(lua_State *L)
    {
        luaL_newmetatable(L, InstanceMetatableName());

        // first time
        int metatable = lua_gettop(L);
        lua_pushcfunction(L, &ValueType::InstanceIndexDispatch);
        lua_setfield(L, metatable, "__index");
    }

    static ValueType *GetFromRegistry(lua_State *L)
    {
        lua_pushlightuserdata(L, (void *)typeid(ValueType).hash_code()); // key
        lua_gettable(L, LUA_REGISTRYINDEX);
        auto p = (ValueType *)lua_topointer(L, -1);
        lua_pop(L, 1);
        return p;
    }

public:
    // for lambda
    template <typename F>
    ValueType &StaticMethod(const char *name, F f)
    {
        m_staticMethods._StaticMethod(name, f, &decltype(f)::operator());
        return *this;
    }

    // for lambda
    template <typename F>
    ValueType &Getter(const char *name, F f)
    {
        m_propertyMap._Getter(name, f, &decltype(f)::operator());
        return *this;
    }

    // for member field pointer
    template <typename C, typename R>
    ValueType &Getter(const char *name, R C::*f)
    {
        m_propertyMap._Getter(name, f);
        return *this;
    }

    // for member function pointer
    template <typename C, typename R, typename... ARGS>
    ValueType &Method(const char *name, R (C::*m)(ARGS...) const)
    {
        m_methodMap.__Method(name, m, std::index_sequence_for<ARGS...>());
        return *this;
    }

    void NewType(lua_State *L)
    {
        // store this to registory
        lua_pushlightuserdata(L, (void *)typeid(ValueType).hash_code()); // key
        lua_pushlightuserdata(L, this);                                  // value
        lua_settable(L, LUA_REGISTRYINDEX);

        // create metatable for type userdata
        LuaNewTypeMetaTable(L);
        lua_pop(L, 1);

        // create metatable for instance userdata
        LuaNewInstanceMetaTable(L);
        lua_pop(L, 1);

        // push type userdata to stack
        UserData<TypeUserData>::New(L, TypeUserData{}, TypeMetatableName());
    }

    static int PushValue(lua_State *L, const T &value)
    {
        UserData<T>::New(L, value, InstanceMetatableName());
        return 1;
    }
};

} // namespace perilune