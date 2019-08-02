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

namespace internal
{

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
    using Type = T;

    static Type *GetSelf(lua_State *L, int index)
    {
        auto p = (T *)lua_topointer(L, index);
        return p;
    }
};

// for pointer type
template <typename T>
struct Traits<T *>
{
    using Type = T;

    static Type *GetSelf(lua_State *L, int index)
    {
        auto p = (T **)lua_topointer(L, index);
        return *p;
    }
};

template <typename T>
class UserData
{
    UserData() = delete;

    using Type = typename Traits<T>::Type;

public:
    static T *New(lua_State *L, const T &value, const char *metatableName)
    {
        auto p = (T *)lua_newuserdata(L, sizeof(T));
        //*p = value;
        memcpy((void *)p, &value, sizeof(T));

        // set metatable to type userdata
        auto pushedType = luaL_getmetatable(L, metatableName);
        if (pushedType == 0) // nil
        {
            lua_pop(L, 2);
            return nullptr;
        }

        lua_setmetatable(L, -2);

        return p;
    }

    static T *GetData(lua_State *L, int index)
    {
        auto t = lua_type(L, index);
        if (t == LUA_TUSERDATA)
        {
            auto p = lua_touserdata(L, index);
            return static_cast<T *>(p);
        }
        else if (t == LUA_TLIGHTUSERDATA)
        {
            auto p = lua_topointer(L, index);
            return (T *)p;
        }
        else
        {
            return nullptr;
        }
    }
}; // namespace internal

template <typename T>
struct UserTypePush
{
    static int PushValue(lua_State *L, const T &value)
    {
        auto name = MetatableName<T>::InstanceName();
        if (!UserData<T>::New(L, value, name))
        {
            // error
            lua_pushfstring(L, "unknown type [%s]", name);
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
        auto name = MetatableName<T *>::InstanceName();
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

template <typename T>
struct UserTypePush<T &>
{
    static int PushValue(lua_State *L, T &value)
    {
        auto name = UserType<T &>::InstanceMetatableName();
        if (!UserData<T &>::New(L, value, name))
        {
            // unknown
            lua_pushlightuserdata(L, &value);
            return 1;
        }
        else
        {
            // success
            return 1;
        }
    }
};

template <typename R, typename T, typename... ARGS>
struct Applyer
{
    static int Apply(lua_State *L, typename Traits<T>::Type *value, R (T::*m)(ARGS...), ARGS... args)
    {
        auto r = (value->*m)(args...);
        return perilune_pushvalue(L, r);
    }
};
template <typename R, typename T, typename... ARGS>
struct Applyer<R &, T, ARGS...>
{
    static int Apply(lua_State *L, typename Traits<T>::Type *value, R &(T::*m)(ARGS...), ARGS... args)
    {
        R &r = (value->*m)(args...);
        return perilune_pushvalue(L, r);
    }
};
template <typename T, typename... ARGS>
struct Applyer<void, T, ARGS...>
{
    static int Apply(lua_State *L, typename Traits<T>::Type *value, void (T::*m)(ARGS...), ARGS... args)
    {
        (value->*m)(args...);
        return 0;
    }
};

template <typename R, typename T, typename... ARGS>
struct ConstApplyer
{
    static int Apply(lua_State *L, typename Traits<T>::Type *value, R (T::*m)(ARGS...) const, ARGS... args)
    {
        auto r = (value->*m)(args...);
        return perilune_pushvalue(L, r);
    }
};
template <typename R, typename T, typename... ARGS>
struct ConstApplyer<R &, T, ARGS...>
{
    static int Apply(lua_State *L, typename Traits<T>::Type *value, R &(T::*m)(ARGS...) const, ARGS... args)
    {
        auto r = &(value->*m)(args...);
        auto name = MetatableName<R>::InstanceName();
        if (!UserData<R>::New(L, *r, name))
        {
            // unknown
            lua_pushlightuserdata(L, (void *)r);
            return 1;
        }
        else
        {
            // success
            return 1;
        }
        // return perilune_pushvalue(L, r);
    }
};
template <typename T, typename... ARGS>
struct ConstApplyer<void, T, ARGS...>
{
    static int Apply(lua_State *L, typename Traits<T>::Type *value, void (T::*m)(ARGS...) const, ARGS... args)
    {
        (value->*m)(args...);
        return 0;
    }
};

} // namespace internal

#pragma region push

static int perilune_pushvalue(lua_State *L, bool b)
{
    lua_pushboolean(L, b);
    return 1;
}

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
    return internal::UserTypePush<T>::PushValue(L, t);
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

static void perilune_getvalue(lua_State *L, int index, void **value)
{
    *value = const_cast<void *>(lua_topointer(L, index));
}

static void perilune_getvalue(lua_State *L, int index, const std::wstring *value)
{
    auto str = luaL_checkstring(L, index);
    // UNICODE
    *const_cast<std::wstring *>(value) = utf8_to_wstring(str);
}

template <typename T>
static void perilune_getvalue(lua_State *L, int index, T *value)
{
    /*
    auto get = internal::UserData<T>::GetData(L, index);
    if (get)
    {
        *value = *get;
    }
    */
    auto t = lua_type(L, index);
    if (t == LUA_TUSERDATA)
    {
        auto p = (T *)lua_touserdata(L, index);
        *value = *p;
    }
    else if (t == LUA_TLIGHTUSERDATA)
    {
        auto p = (T)lua_topointer(L, index);
        *value = p;
        // return (T *)p;
    }
    else
    {
        // return nullptr;
    }
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
    typedef std::function<int(lua_State *, T *)> PropertyMethod;
    std::unordered_map<std::string, PropertyMethod> m_getterMap;

public:
    template <typename F, typename C, typename R>
    void SetLambdaGetter(const char *name, const F &f, R (C::*)(const T &value) const)
    {
        PropertyMethod func = [f](lua_State *L, T *value) {
            // auto value = internal::UserData<T>::GetData(L, 1);
            R r = f(*value);

            int result = perilune_pushvalue(L, r);
            return result;
        };
        m_getterMap.insert(std::make_pair(name, func));
    }

    // for field
    template <typename C, typename R>
    void SetFieldGetter(const char *name, R C::*f)
    {
        // auto self = this;
        PropertyMethod func = [f](lua_State *L, T *value) {
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
    typedef std::function<int(lua_State *, T *)> LuaMethod;
    std::unordered_map<std::string, LuaMethod> m_methodMap;
    ;

public:
    template <typename R, typename... ARGS, std::size_t... IS>
    void Method(const char *name,
                R (T::*m)(ARGS...),
                std::index_sequence<IS...>)
    {
        LuaMethod func = [m](lua_State *L, T* value) {
            auto args = perilune_totuple<std::remove_reference<ARGS>::type...>(L, 1);
            return internal::Applyer<R, T, ARGS...>::Apply(L, value, m, std::get<IS>(args)...);
        };
        m_methodMap.insert(std::make_pair(name, func));
    }

    template <typename R, typename... ARGS, std::size_t... IS>
    void ConstMethod(const char *name,
                     R (T::*m)(ARGS...) const,
                     std::index_sequence<IS...>)
    {
        LuaMethod func = [m](lua_State *L, T* value) {
            auto args = perilune_totuple<ARGS...>(L, 1);
            return internal::ConstApplyer<R, T, ARGS...>::Apply(L, value, m, std::get<IS>(args)...);
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

    using Type = typename internal::Traits<T>::Type;

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
        assert(luaL_newmetatable(L, internal::MetatableName<T>::TypeName()) == 1);
        int metatable = lua_gettop(L);

        lua_pushcfunction(L, &UserType::TypeIndexDispatch);
        lua_setfield(L, metatable, "__index");
    }

    PropertyMap<Type> m_propertyMap;
    MethodMap<Type> m_methodMap;

    static int InstanceGCDispatch(lua_State *L)
    {
        std::cerr << "__GC" << std::endl;

        auto self = UserType<T>::GetFromRegistry(L);
        if (self->m_destructor)
        {
            auto value = internal::UserData<T>::GetData(L, -1);
            self->m_destructor(*value);
        }
        return 0;
    }

    // stack 1:table(userdata), 2:key
    static int InstanceIndexDispatch(lua_State *L)
    {
        auto type = GetFromRegistry(L);
        auto self = internal::Traits<T>::GetSelf(L, 1);
        auto key = lua_tostring(L, 2);

        {
            auto property = type->m_propertyMap.Get(key);
            if (property)
            {
                return property(L, self);
            }
        }

        lua_pushcclosure(L, &UserType::InstanceMethodDispatch, 2);
        return 1;
    }

    // upvalue 1:table(userdata), 2:key
    static int InstanceMethodDispatch(lua_State *L)
    {
        auto type = GetFromRegistry(L);
        auto self = internal::Traits<T>::GetSelf(L, lua_upvalueindex(1));
        auto key = lua_tostring(L, lua_upvalueindex(2));

        auto callback = type->m_methodMap.Get(key);
        if (callback)
        {
            return callback(L, self);
        }

        // error
        lua_pushfstring(L, "no %s method", key);
        lua_error(L);
        return 1;
    }

    void LuaNewInstanceMetaTable(lua_State *L)
    {
        luaL_newmetatable(L, internal::MetatableName<T>::InstanceName());

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
    UserType()
    {
    }
    ~UserType()
    {
        std::cerr << "~" << internal::MetatableName<T>::TypeName() << std::endl;
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
        m_propertyMap.SetLambdaGetter(name, f, &decltype(f)::operator());
        return *this;
    }

    // for member field pointer
    template <typename C, typename R>
    UserType &Getter(const char *name, R C::*f)
    {
        m_propertyMap.SetFieldGetter(name, f);
        return *this;
    }

    // for member function pointer
    template <typename R, typename C, typename... ARGS>
    UserType &Method(const char *name, R (C::*m)(ARGS...))
    {
        m_methodMap.Method(name, m, std::index_sequence_for<ARGS...>());
        return *this;
    }

    // for const member function pointer
    template <typename R, typename C, typename... ARGS>
    UserType &Method(const char *name, R (C::*m)(ARGS...) const)
    {
        m_methodMap.ConstMethod(name, m, std::index_sequence_for<ARGS...>());
        return *this;
    }

    void LuaNewType(lua_State *L)
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
        internal::UserData<TypeUserData>::New(L, TypeUserData{}, internal::MetatableName<T>::TypeName());
    }
};

} // namespace perilune