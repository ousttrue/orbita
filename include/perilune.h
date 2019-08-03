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

#include <sstream>
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
    using Type = T;

    using PT = T *;

    static Type *GetSelf(lua_State *L, int index)
    {
        return *(PT *)lua_touserdata(L, index);
    }

    static T **GetData(lua_State *L, int index)
    {
        return (PT *)lua_touserdata(L, index);
    }
};

template <typename R, typename T, typename... ARGS>
struct Applyer
{
    static int Apply(lua_State *L, typename Traits<T>::Type *value, R (T::*m)(ARGS...), ARGS... args)
    {
        auto r = (value->*m)(args...);
        return LuaPush<R>::Push(L, r);
    }
};
template <typename R, typename T, typename... ARGS>
struct Applyer<R &, T, ARGS...>
{
    static int Apply(lua_State *L, typename Traits<T>::Type *value, R &(T::*m)(ARGS...), ARGS... args)
    {
        auto &r = (value->*m)(args...);
        return LuaPush<R *>::Push(L, &r);
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
        return LuaPush<R>::Push(L, r);
    }
};
template <typename R, typename T, typename... ARGS>
struct ConstApplyer<R &, T, ARGS...>
{
    static int Apply(lua_State *L, typename Traits<T>::Type *value, R &(T::*m)(ARGS...) const, ARGS... args)
    {
        auto &r = (value->*m)(args...);
        return LuaPush<R *>::Push(L, &r);
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

#pragma region push
template <typename T>
struct LuaPush
{
    static int Push(lua_State *L, const T &value)
    {
        auto p = (T *)lua_newuserdata(L, sizeof(T));
        auto pushedType = luaL_getmetatable(L, MetatableName<T>::InstanceName());
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

            // error
            lua_pushfstring(L, "unknown type [%s]", MetatableName<T>::InstanceName());
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
        auto pushedType = luaL_getmetatable(L, MetatableName<T *>::InstanceName());
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
        auto pushedType = luaL_getmetatable(L, MetatableName<T *>::InstanceName());
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
struct LuaPush<int>
{
    static int Push(lua_State *L, int n)
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
struct LuaPush<void *>
{
    static int Push(lua_State *L, void *n)
    {
        lua_pushlightuserdata(L, n);
        return 1;
    }
};
#pragma endregion

#pragma region get tuple
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
        // else if (t == LUA_TLIGHTUSERDATA)
        // {
        //     return (T)lua_touserdata(L, index);
        // }
        else
        {
            // return nullptr;
            std::stringstream ss;
            ss << "LuaGet<" << typeid(T).name() << "> is not implemented";
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
            return (T *)lua_touserdata(L, index);
        }
        else if (t == LUA_TLIGHTUSERDATA)
        {
            return (T *)lua_touserdata(L, index);
        }
        else
        {
            // return nullptr;
            throw std::exception("not implemented");
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

std::tuple<> perilune_totuple(lua_State *L, int index, std::tuple<> *)
{
    return std::tuple<>();
}

template <typename A, typename... ARGS>
std::tuple<A, ARGS...> perilune_totuple(lua_State *L, int index, std::tuple<A, ARGS...> *)
{
    A a = LuaGet<A>::Get(L, index);
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
            auto r = f(std::get<IS>(args)...);
            return LuaPush<R>::Push(L, r);
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
            return LuaPush<R>::Push(L, r);
        };
        m_getterMap.insert(std::make_pair(name, func));
    }

    // for field
    template <typename C, typename R>
    void SetFieldGetter(const char *name, R C::*f)
    {
        // auto self = this;
        PropertyMethod func = [f](lua_State *L, T *value) {
            if (!value)
            {
                throw std::exception("null");
            }
            R r = value->*f;
            return LuaPush<R>::Push(L, r);
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
struct remove_const_ref
{
    using no_ref = typename std::remove_reference<T>::type;
    using type = typename std::remove_const<no_ref>::type;
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
        LuaMethod func = [m](lua_State *L, T *value) {
            auto args = perilune_totuple<remove_const_ref<ARGS>::type...>(L, 1);
            return internal::Applyer<R, T, ARGS...>::Apply(L, value, m, std::get<IS>(args)...);
        };
        m_methodMap.insert(std::make_pair(name, func));
    }

    template <typename R, typename... ARGS, std::size_t... IS>
    void ConstMethod(const char *name,
                     R (T::*m)(ARGS...) const,
                     std::index_sequence<IS...>)
    {
        LuaMethod func = [m](lua_State *L, T *value) {
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
};
} // namespace internal

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

    internal::StaticMethodMap m_staticMethods;

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

    internal::PropertyMap<Type> m_propertyMap;
    internal::MethodMap<Type> m_methodMap;

    static int InstanceGCDispatch(lua_State *L)
    {
        std::cerr << "__GC" << std::endl;

        auto self = UserType<T>::GetFromRegistry(L);
        if (self->m_destructor)
        {
            auto value = internal::Traits<T>::GetData(L, -1);
            self->m_destructor(*value);
        }
        return 0;
    }

    // stack 1:table(userdata), 2:key
    static int InstanceIndexDispatch(lua_State *L)
    {
        auto type = GetFromRegistry(L);
        auto key = lua_tostring(L, 2);
        auto self = internal::Traits<T>::GetSelf(L, 1);

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
            try
            {
                return callback(L, self);
            }
            catch (const std::exception &ex)
            {
                lua_pushfstring(L, ex.what());
                lua_error(L);
                return 1;
            }
            catch (...)
            {
                lua_pushfstring(L, "fail to %s", key);
                lua_error(L);
                return 1;
            }
        }

        // error
        lua_pushfstring(L, "no %s method", key);
        lua_error(L);
        return 1;
    }

    void LuaNewInstanceMetaTable(lua_State *L)
    {
        std::cerr << "create: " << internal::MetatableName<T>::InstanceName() << std::endl;
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
        auto p = (UserType *)lua_touserdata(L, -1);
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
            // std::cerr << "destructor" << std::endl;
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

        {
            // push userdata for Type
            auto p = (TypeUserData *)lua_newuserdata(L, sizeof(TypeUserData));
            // set metatable to type userdata
            auto pushedType = luaL_getmetatable(L, internal::MetatableName<T>::TypeName());
            lua_setmetatable(L, -2);
        }
    }
};

} // namespace perilune