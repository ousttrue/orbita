#pragma once

#include "common.h"
#include "luafunc.h"

namespace perilune
{

template <typename T>
class UserType
{
    // nocopy
    UserType(const UserType &) = delete;
    UserType &operator=(const UserType &) = delete;

    // static method dispatcher(simple static functions)
    struct UserTypeDummy
    {
    };
    StaticMethodMap m_staticMethods;
    LuaFunc m_typeIndexClosure;

    // instance method dispatcher(object methods bind this pointer)
    std::unordered_map<MetaKey, LuaFunc> m_metamethodMap;
    IndexDispatcher<T> m_indexDispatcher;
    LuaFunc m_instanceIndexClosure;

public:
    UserType()
    {
        m_typeIndexClosure = std::bind(&StaticMethodMap::Dispatch, &m_staticMethods, std::placeholders::_1);
        m_instanceIndexClosure = std::bind(&IndexDispatcher<T>::Dispatch, &m_indexDispatcher, std::placeholders::_1);
    }

    ~UserType()
    {
        // std::cerr << "~" << MetatableName<T>::TypeName() << std::endl;
    }

    template <typename... ARGS>
    UserType &PlacementNew(const char *name)
    {
        m_staticMethods.StaticMethod(name, [](lua_State *L) {
            auto args = LuaArgsToTuple<ARGS...>(L, 1);
            return LuaPush<T>::New(L, args);
        });
        MetaMethod(perilune::MetaKey::__gc, [](T *p) { p->~T(); });
        return *this;
    }

    UserType &DefaultConstructorAndDestructor()
    {
        using RawType = typename Traits<T>::RawType;
        StaticMethod("new", []() { return new RawType; });
        MetaMethod(perilune::MetaKey::__gc, [](T p) { delete p; });
        return *this;
    }

    // for lambda
    template <typename F>
    UserType &StaticMethod(const char *name, F f)
    {
        m_staticMethods.StaticMethod(name, f, &decltype(f)::operator());
        return *this;
    }

    UserType &LuaMetaMethod(MetaKey key, const LuaFunc &lf)
    {
        m_metamethodMap.insert(std::make_pair(key, lf));
        return *this;
    }

    template <typename F>
    UserType &MetaMethod(MetaKey key, F f)
    {
        auto lf = MetaMethodSelfFromStack1((T *)nullptr, key, f, &decltype(f)::operator());
        m_metamethodMap.insert(std::make_pair(key, lf));
        return *this;
    }

    UserType &MetaIndexDispatcher(const std::function<void(IndexDispatcher<T> *)> &f)
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
        // LuaNewTypeMetaTable(L);
        {
            assert(luaL_newmetatable(L, MetatableName<T>::TypeName()) == 1);
            int metatable = lua_gettop(L);

            {
                lua_pushlightuserdata(L, &m_typeIndexClosure);
                lua_pushcclosure(L, &LuaFuncClosure, 1);
                lua_setfield(L, metatable, "__index");
            }

            lua_pop(L, 1);
        }

        // create metatable for instance userdata
        // LuaNewInstanceMetaTable(L);
        {
            // std::cerr << "create: " << MetatableName<T>::InstanceName() << std::endl;
            LuaNewMetatable<T>(L);

            // first time
            int metatable = lua_gettop(L);

            {
                lua_pushlightuserdata(L, &m_instanceIndexClosure);
                lua_pushcclosure(L, &LuaFuncClosure, 1);
                lua_setfield(L, metatable, "__index");
            }

            Traits<T>::SetPlacementDelete(L, metatable);

            for (auto &kv : m_metamethodMap)
            {
                lua_pushlightuserdata(L, &kv.second);
                lua_pushcclosure(L, &LuaFuncClosure, 1);
                lua_setfield(L, metatable, ToString(kv.first));
            }

            lua_pop(L, 1);
        }

        {
            // push userdata for Type
            auto p = (UserTypeDummy *)lua_newuserdata(L, sizeof(UserTypeDummy));
            // set metatable to type userdata
            auto pushedType = luaL_getmetatable(L, MetatableName<T>::TypeName());
            lua_setmetatable(L, -2);
        }
    }
};

// for std::vector
template <typename T>
void AddDefaultMethods(UserType<T> &userType)
{
    using RawType = Traits<T>::RawType;

    userType
        .MetaMethod(perilune::MetaKey::__len, [](T p) {
            return p->size();
        })
        .MetaIndexDispatcher([](perilune::IndexDispatcher<T> *d) {
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