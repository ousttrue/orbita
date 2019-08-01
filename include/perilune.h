#pragma once

#include <Windows.h>
#include <string>

std::wstring utf8_to_wstring(const std::string &src)
{
    auto required = MultiByteToWideChar(CP_UTF8, 0, src.data(), (int)src.size(), nullptr, 0);
    std::wstring dst(required, 0);
    MultiByteToWideChar(CP_UTF8, 0, src.data(), (int)src.size(), dst.data(), required);
    return dst;
}

extern "C"
{
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

#include <iostream>
#include <functional>
#include <unordered_map>
#include <string>
#include <tuple>
#include <type_traits>
#include <assert.h>

namespace perilune
{

// normal type
template <typename T>
struct Traits
{
    using Type = T;

    static Type *GetThis(T *t)
    {
        return t;
    }
};

// for pointer type
template <typename T>
struct Traits<T *>
{
    using Type = T;

    static Type *GetThis(T **t)
    {
        return *t;
    }
};

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
    return UserTypePush<T>::PushValue(L, t);
}
#pragma endregion

#pragma region get tuple
static void perilune_getvalue(lua_State *L, int index, int *value)
{
    *value = (int)luaL_checkinteger(L, index);
}

static void perilune_getvalue(lua_State *L, int index, float *value)
{
    *value = (float)luaL_checknumber(L, index);
}

static void perilune_getvalue(lua_State *L, int index, const std::wstring *value)
{
    auto str = luaL_checkstring(L, index);
    // UNICODE
    *const_cast<std::wstring *>(value) = utf8_to_wstring(str);
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

    using Type = typename Traits<T>::Type;

public:
    static T *New(lua_State *L, const T &value, const char *metatableName)
    {
        auto p = (T *)lua_newuserdata(L, sizeof(T));
        *p = value;

        // set metatable to type userdata
        auto pushedType = luaL_getmetatable(L, metatableName);
        if (pushedType == 0) // nil
        {
            return nullptr;
        }

        lua_setmetatable(L, -2);

        return p;
    }

    static T *GetData(lua_State *L, int index)
    {
        return static_cast<T *>(lua_touserdata(L, index));
    }

    static Type *GetThis(lua_State *L, int index)
    {
        auto data = GetData(L, index);
        return Traits<T>::GetThis(data);
    }
};

class StaticMethodMap
{
    typedef std::function<int(lua_State *)> StaticMethod;

    std::unordered_map<std::string, StaticMethod> m_methodMap;

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
        m_methodMap.insert(std::make_pair(name, func));
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
        auto found = m_methodMap.find(key);
        if (found == m_methodMap.end())
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
            auto value = UserData<T>::GetData(L, 1);
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
            auto value = UserData<T>::GetThis(L, 1);
            R r = value->*f;

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

    using Traits = typename Traits<T>;

public:
    template <typename R, typename C, typename... ARGS, std::size_t... IS>
    void Method(const char *name,
                R (C::*m)(ARGS...),
                std::index_sequence<IS...>)
    {
        // auto self = this;
        LuaMethod func = [m](lua_State *L) {
            auto value = UserData<T>::GetThis(L, lua_upvalueindex(1)); // from upvalue
            auto args = perilune_totuple<std::remove_reference<ARGS>::type...>(L, 1);

            R r = (value->*m)(std::get<IS>(args)...);

            return perilune_pushvalue(L, r);
        };
        m_methodMap.insert(std::make_pair(name, func));
    }

    template <typename R, typename C, typename... ARGS, std::size_t... IS>
    void ConstMethod(const char *name,
                     R (C::*m)(ARGS...) const,
                     std::index_sequence<IS...>)
    {
        // auto self = this;
        LuaMethod func = [m](lua_State *L) {
            auto value = UserData<T>::GetThis(L, lua_upvalueindex(1)); // from upvalue
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
class UserType
{
    // nocopy
    UserType(const UserType &) = delete;
    UserType &operator=(const UserType &) = delete;

    using Type = typename Traits<T>::Type;

    // userdata dummy for Type
    struct TypeUserData
    {
    };

    StaticMethodMap m_staticMethods;

    std::function<void(T &)> m_destructor;

    // stack 1:table(userdata), 2:key
    static int TypeIndexDispatch(lua_State *L)
    {
        lua_pushcclosure(L, &UserType::TypeMethodDispatch, 2);
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

        lua_pushcfunction(L, &UserType::TypeIndexDispatch);
        lua_setfield(L, metatable, "__index");
    }

    PropertyMap<T> m_propertyMap;
    MethodMap<T> m_methodMap;

    static int InstanceGCDispatch(lua_State *L)
    {
        std::cerr << "__GC" << std::endl;

        auto self = UserType<T>::GetFromRegistry(L);
        if (self->m_destructor)
        {
            auto value = UserData<T>::GetData(L, -1);
            self->m_destructor(*value);
        }
        return 0;
    }

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

        lua_pushcclosure(L, &UserType::InstanceMethodDispatch, 2);
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
        lua_pushcfunction(L, &UserType::InstanceIndexDispatch);
        lua_setfield(L, metatable, "__index");

        if (m_destructor)
        {
            lua_pushcfunction(L, &UserType::InstanceGCDispatch);
            lua_setfield(L, metatable, "__gc");
        }
    }

    static UserType *GetFromRegistry(lua_State *L)
    {
        lua_pushlightuserdata(L, (void *)typeid(UserType).hash_code()); // key
        lua_gettable(L, LUA_REGISTRYINDEX);
        auto p = (UserType *)lua_topointer(L, -1);
        lua_pop(L, 1);
        return p;
    }

public:
    static const char *TypeMetatableName()
    {
        return typeid(UserType).name();
    }

    static const char *InstanceMetatableName()
    {
        return typeid(T).name();
    }

    UserType()
    {
    }
    ~UserType()
    {
        std::cerr << "~UserType" << std::endl;
    }

    // for lambda
    template <typename F>
    UserType &StaticMethod(const char *name, F f)
    {
        m_staticMethods._StaticMethod(name, f, &decltype(f)::operator());
        return *this;
    }

    UserType &Destructor(const std::function<void(T &)> &destructor)
    {
        if (m_destructor)
        {
            throw std::exception("destructor already exits");
        }
        m_destructor = destructor;
        if (m_destructor)
        {
            std::cerr << "destructor" << std::endl;
        }
        return *this;
    }

    // for lambda
    template <typename F>
    UserType &Getter(const char *name, F f)
    {
        m_propertyMap._Getter(name, f, &decltype(f)::operator());
        return *this;
    }

    // for member field pointer
    template <typename C, typename R>
    UserType &Getter(const char *name, R C::*f)
    {
        m_propertyMap._Getter(name, f);
        return *this;
    }

    // for member function pointer
    template <typename R, typename C, typename... ARGS>
    UserType &Method(const char *name, R (C::*m)(ARGS...))
    {
        m_methodMap.Method(name, m, std::index_sequence_for<ARGS...>());
        return *this;
    }

    template <typename R, typename C, typename... ARGS>
    UserType &Method(const char *name, R (C::*m)(ARGS...) const)
    {
        m_methodMap.ConstMethod(name, m, std::index_sequence_for<ARGS...>());
        return *this;
    }

    void NewType(lua_State *L)
    {
        // store this to registory
        lua_pushlightuserdata(L, (void *)typeid(UserType).hash_code()); // key
        lua_pushlightuserdata(L, this);                                 // value
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
};

template <typename T>
struct UserTypePush
{
    static int PushValue(lua_State *L, const T &value)
    {
        auto name = UserType<T>::InstanceMetatableName();
        if (!UserData<T>::New(L, value, name))
        {
            // error
            lua_pushfstring(L, "unknown type %s", name);
            lua_error(L);
            return 1;
        }
        else
        {
            // success
            return 1;
        }
    }
};

template <typename T>
struct UserTypePush<T *>
{
    static int PushValue(lua_State *L, T *value)
    {
        auto name = UserType<T *>::InstanceMetatableName();
        if (!UserData<T *>::New(L, value, name))
        {
            // unknown
            lua_pushlightuserdata(L, value);
            return 1;
        }
        else
        {
            // success
            return 1;
        }
    }
};

} // namespace perilune