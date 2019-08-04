#pragma once
#include "common.h"

namespace perilune
{

template <typename T>
class IndexDispatcher
{
    using RawType = typename Traits<T>::RawType;

    IndexDispatcher(const IndexDispatcher &) = delete;
    IndexDispatcher &operator=(const IndexDispatcher &) = delete;

    LuaFunc m_closure;

    struct Value
    {
        bool IsFunction = false;
        LuaFunc Body;
    };
    std::unordered_map<std::string, Value> m_map;

    using LuaIndexGetterFunc = std::function<int(lua_State *, RawType *, lua_Integer)>;
    LuaIndexGetterFunc m_indexGetter;

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

    template <typename R, typename C, typename... ARGS, std::size_t... IS>
    void _Method(const char *name, R (C::*m)(ARGS...), std::index_sequence<IS...>)
    {
        // upvalue#2: userdata
        LuaFunc func = [m](lua_State *L) {
            auto value = Traits<T>::GetSelf(L, lua_upvalueindex(2));
            auto args = perilune_totuple<remove_const_ref<ARGS>::type...>(L, 1);
            return Applyer<R, RawType, ARGS...>::Apply(L, value, m, std::get<IS>(args)...);
        };
        m_map.insert(std::make_pair(name, Value{true, func}));
    }

    template <typename R, typename C, typename... ARGS, std::size_t... IS>
    void _ConstMethod(const char *name, R (C::*m)(ARGS...) const, std::index_sequence<IS...>)
    {
        // upvalue#2: userdata
        LuaFunc func = [m](lua_State *L) {
            auto value = Traits<T>::GetSelf(L, lua_upvalueindex(2));
            auto args = perilune_totuple<ARGS...>(L, 1);
            return ConstApplyer<R, RawType, ARGS...>::Apply(L, value, m, std::get<IS>(args)...);
        };
        m_map.insert(std::make_pair(name, Value{true, func}));
    }

    template <typename F, typename C, typename R>
    void _SetLambdaGetter(const char *name, const F &f, R (C::*)(RawType *) const)
    {
        // stack#1: self
        LuaFunc func = [f](lua_State *L) {
            auto value = Traits<T>::GetSelf(L, 1);
            R r = f(value);
            return LuaPush<R>::Push(L, r);
        };
        m_map.insert(std::make_pair(name, Value{false, func}));
    }

    // for field
    template <typename C, typename R>
    void _SetFieldGetter(const char *name, R C::*f)
    {
        // stack#1: self
        LuaFunc func = [f](lua_State *L) {
            auto value = Traits<T>::GetSelf(L, 1);
            R r = value->*f;
            return LuaPush<R>::Push(L, r);
        };
        m_map.insert(std::make_pair(name, Value{false, func}));
    }

public:
    IndexDispatcher()
    {
        m_closure = std::bind(&IndexDispatcher::Dispatch, this, std::placeholders::_1);
    }

    ~IndexDispatcher() {}

    LuaFunc *GetLuaFunc()
    {
        return &m_closure;
    }

    // for member function pointer
    template <typename R, typename C, typename... ARGS>
    void Method(const char *name, R (C::*m)(ARGS...))
    {
        _Method(name, m, std::index_sequence_for<ARGS...>());
    }

    // for const member function pointer
    template <typename R, typename C, typename... ARGS>
    void Method(const char *name, R (C::*m)(ARGS...) const)
    {
        _ConstMethod(name, m, std::index_sequence_for<ARGS...>());
    }

    void LuaMethod(const char *name, const LuaFunc &func)
    {
        m_map.insert(std::make_pair(name, Value{true, func}));
    }

    // for lambda
    template <typename F>
    void Getter(const char *name, F f)
    {
        _SetLambdaGetter(name, f, &decltype(f)::operator());
    }

    // for member field pointer
    template <typename C, typename R>
    void Getter(const char *name, R C::*f)
    {
        _SetFieldGetter(name, f);
    }

    void LuaIndexGetter(const LuaIndexGetterFunc &indexGetter)
    {
        m_indexGetter = indexGetter;
    }

    template <typename F, typename R, typename C>
    void _IndexGetter(const F &f, R (C::*m)(RawType *, int) const)
    {
        LuaIndexGetterFunc callback = [f](lua_State *L, RawType *l, lua_Integer i) {
            R r = f(l, (int)i);
            return LuaPush<R>::Push(L, r);
        };
        LuaIndexGetter(callback);
    }

    template <typename F>
    void IndexGetter(F f)
    {
        _IndexGetter(f, &decltype(f)::operator());
    }
};

} // namespace prerilune