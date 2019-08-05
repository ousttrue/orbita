#pragma once
#include "common.h"
#include "luafunc.h"

namespace perilune
{

template <typename T>
class IndexDispatcher
{
    using RawType = typename Traits<T>::RawType;

    IndexDispatcher(const IndexDispatcher &) = delete;
    IndexDispatcher &operator=(const IndexDispatcher &) = delete;

    struct MetaValue
    {
        bool IsFunction = false;
        LuaFunc Body;
    };
    std::unordered_map<std::string, MetaValue> m_map;

    using LuaIndexGetterFunc = std::function<int(lua_State *, RawType *, lua_Integer)>;
    LuaIndexGetterFunc m_indexGetter;

    int DispatchIndex(lua_State *L)
    {
        auto value = perilune::Traits<T>::GetSelf(L, 1);
        auto index = lua_tointeger(L, 2);

        if (m_indexGetter)
        {
            return m_indexGetter(L, value, index);
        }

        return LuaIndexer<RawType>::Push(L, value, index);
    }

    int DispatchStringKey(lua_State *L)
    {
        auto key = lua_tostring(L, 2);
        auto found = m_map.find(key);
        if (found == m_map.end())
        {
            lua_pushfstring(L, "'%s' is not found in __index", key);
            lua_error(L);
            return 1;
        }

        if (!found->second.IsFunction)
        {
            try
            {
                // execute getter or setter
                return found->second.Body(L);
            }
            catch (const std::exception &ex)
            {
                lua_pushfstring(L, ex.what());
                lua_error(L);
                return 1;
            }
            catch (...)
            {
                lua_pushfstring(L, "'%s' error", key);
                lua_error(L);
                return 1;
            }
        }

        // upvalue#1: body
        lua_pushlightuserdata(L, &found->second.Body);
        // upvalue#2: userdata
        lua_pushvalue(L, -3);
        // closure
        lua_pushcclosure(L, &LuaFuncClosure, 2);
        return 1;
    }

public:
    IndexDispatcher()
    {
    }

    ~IndexDispatcher() {}

    // stack#1: userdata
    // stack#2: key
    int Dispatch(lua_State *L)
    {
        if (lua_isinteger(L, 2))
        {
            return DispatchIndex(L);
        }

        if (lua_isstring(L, 2))
        {
            return DispatchStringKey(L);
        }

        lua_pushfstring(L, "unknown key type '%s'", lua_typename(L, 2));
        lua_error(L);
        return 1;
    }

    // for member function pointer
    template <typename R, typename C, typename... ARGS>
    void Method(const char *name, R (C::*m)(ARGS...))
    {
        auto lf = MethodSelfFromUpvalue2((T *)nullptr, name, m, std::index_sequence_for<ARGS...>());
        m_map.insert(std::make_pair(name, MetaValue{true, lf}));
    }

    // for const member function pointer
    template <typename R, typename C, typename... ARGS>
    void Method(const char *name, R (C::*m)(ARGS...) const)
    {
        auto lf = ConstMethodSelfFromUpvalue2((T *)nullptr, name, m, std::index_sequence_for<ARGS...>());
        m_map.insert(std::make_pair(name, MetaValue{true, lf}));
    }

    void LuaMethod(const char *name, const LuaFunc &func)
    {
        m_map.insert(std::make_pair(name, MetaValue{true, func}));
    }

    // for lambda
    template <typename F>
    void Getter(const char *name, F f)
    {
        auto lf = LambdaGetterSelfFromStack1((T *)nullptr, name, f, &decltype(f)::operator());
        m_map.insert(std::make_pair(name, MetaValue{false, lf}));
    }

    // for member field pointer
    template <typename C, typename R>
    void Getter(const char *name, R C::*f)
    {
        auto lf = FieldGetterSelfFromStack1((T *)nullptr, name, f);
        m_map.insert(std::make_pair(name, MetaValue{false, lf}));
    }

private:
    template <typename F, typename R, typename C>
    void _IndexGetter(const F &f, R (C::*m)(RawType *, int) const)
    {
        LuaIndexGetterFunc callback = [f](lua_State *L, RawType *l, lua_Integer i) {
            R r = f(l, (int)i);
            return LuaPush<R>::Push(L, r);
        };
        LuaIndexGetter(callback);
    }

public:
    template <typename F>
    void IndexGetter(F f)
    {
        _IndexGetter(f, &decltype(f)::operator());
    }

    void LuaIndexGetter(const LuaIndexGetterFunc &indexGetter)
    {
        m_indexGetter = indexGetter;
    }
};

} // namespace perilune