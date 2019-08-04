#pragma once

#include "common.h"

namespace perilune
{

template <typename T>
struct remove_const_ref
{
    using no_ref = typename std::remove_reference<T>::type;
    using type = typename std::remove_const<no_ref>::type;
};

template <typename T>
class UserType
{
    // nocopy
    UserType(const UserType &) = delete;
    UserType &operator=(const UserType &) = delete;

    using RawType = typename Traits<T>::RawType;

    // userdata dummy for Type
    struct UserTypeDummy
    {
    };

    StaticMethodMap m_staticMethods;

    std::unordered_map<MetaKey, LuaFunc> m_metamethodMap;

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
        assert(luaL_newmetatable(L, MetatableName<T>::TypeName()) == 1);
        int metatable = lua_gettop(L);

        lua_pushcfunction(L, &UserType::TypeIndexDispatch);
        lua_setfield(L, metatable, "__index");
    }

    void LuaNewInstanceMetaTable(lua_State *L)
    {
        std::cerr << "create: " << MetatableName<T>::InstanceName() << std::endl;
        luaL_newmetatable(L, MetatableName<T>::InstanceName());

        // first time
        int metatable = lua_gettop(L);

        auto indexFunc = m_indexDispatcher.GetLuaFunc();
        if (indexFunc)
        {
            lua_pushlightuserdata(L, indexFunc);
            lua_pushcclosure(L, &LuaFuncClosure, 1);
            lua_setfield(L, metatable, "__index");
        }

        for (auto &kv : m_metamethodMap)
        {
            lua_pushlightuserdata(L, &kv.second);
            lua_pushcclosure(L, &LuaFuncClosure, 1);
            lua_setfield(L, metatable, ToString(kv.first));
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

    template <typename F, typename R, typename C, typename... ARGS>
    void _MetaMethodLambda(MetaKey key, const F &f, R (C::*m)(ARGS...) const)
    {
        auto callback = [f](lua_State *L) {
            auto self = Traits<T>::GetData(L, 1);
            R r = f(*self);
            return LuaPush<R>::Push(L, r);
        };
        LuaMetaMethod(key, callback);
    }

    template <typename F, typename C, typename... ARGS>
    void _MetaMethodLambda(MetaKey key, const F &f, void (C::*m)(ARGS...) const)
    {
        auto callback = [f](lua_State *L) {
            auto self = Traits<T>::GetData(L, 1);
            f(*self);
            return 0;
        };
        LuaMetaMethod(key, callback);
    }

public:
    UserType()
    {
    }
    ~UserType()
    {
        std::cerr << "~" << MetatableName<T>::TypeName() << std::endl;
    }

    // for lambda
    template <typename F>
    UserType &StaticMethod(const char *name, F f)
    {
        m_staticMethods._StaticMethod(name, f, &decltype(f)::operator());
        return *this;
    }

    UserType &LuaMetaMethod(MetaKey key, const LuaFunc &f)
    {
        m_metamethodMap.insert(std::make_pair(key, f));
        return *this;
    }

    template <typename F>
    UserType &MetaMethod(MetaKey key, F f)
    {
        _MetaMethodLambda(key, f, &decltype(f)::operator());
        return *this;
    }

    IndexDispatcher<T> m_indexDispatcher;
    UserType &IndexDispatcher(const std::function<void(IndexDispatcher<T> *)> &f)
    {
        f(&m_indexDispatcher);
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
            auto p = (UserTypeDummy *)lua_newuserdata(L, sizeof(UserTypeDummy));
            // set metatable to type userdata
            auto pushedType = luaL_getmetatable(L, MetatableName<T>::TypeName());
            lua_setmetatable(L, -2);
        }
    }
};

// duck typing
template <typename T>
void AddDefaultMethods(UserType<T> &userType)
{
    using RawType = Traits<T>::RawType;

    userType
        .MetaMethod(perilune::MetaKey::__len, [](T p) {
            return p->size();
        })
        .IndexDispatcher([](perilune::IndexDispatcher<T> *d) {
            // upvalue#2: userdata
            d->LuaMethod("push_back", [](lua_State *L) {
                auto value = perilune::Traits<T>::GetSelf(L, lua_upvalueindex(2));
                auto v = perilune::LuaGet<RawType::value_type>::Get(L, 1);
                value->push_back(v);
                return 0;
            });
        });
}

} // namespace perilune