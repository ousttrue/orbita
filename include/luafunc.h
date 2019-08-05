#pragma once
#include "common.h"

namespace perilune
{

using LuaFunc = std::function<int(lua_State *)>;
/// usage
///
/// LuaFunc lf; // function body
/// lua_pushlightuserdata(L, &lf); // upvalue #1
/// lua_pushcclosure(L, &LuaFuncClosure, 1); // closure
/// return 1;
///
/// [additional upvalue]
/// LuaFunc lf; // function body
/// lua_pushlightuserdata(L, &lf); // upvalue #1
/// lua_pushlightuserdata(L, this); // upvalue #2
/// lua_pushcclosure(L, &LuaFuncClosure, 2); // closure
/// return 1;
///
inline int LuaFuncClosure(lua_State *L)
{
    try
    {
        // execute logic from upvalue
        auto lf = (LuaFunc *)lua_touserdata(L, lua_upvalueindex(1));
        return (*lf)(L);
    }
    catch (const std::exception &ex)
    {
        lua_pushfstring(L, ex.what());
        lua_error(L);
        return 1;
    }
    catch (...)
    {
        lua_pushfstring(L, "error in closure");
        lua_error(L);
        return 1;
    }
}

template <typename F, typename C, typename R, typename... ARGS, std::size_t... IS>
LuaFunc ToLuaFunc(const char *name,
                  const F &f,
                  R (C::*m)(ARGS...) const,
                  std::index_sequence<IS...>)
{
    return [f](lua_State *L) {
        auto args = LuaArgsToTuple<ARGS...>(L, 1);
        auto r = f(std::get<IS>(args)...);
        return LuaPush<R>::Push(L, r);
    };
}

#pragma region userdata by stack1

template <typename T, typename F, typename R, typename C, typename... ARGS>
LuaFunc MetaMethodSelfFromStack1(T *, MetaKey key, const F &f, R (C::*m)(ARGS...) const)
{
    // stack#1: userdata
    return [f](lua_State *L) {
        auto self = Traits<T>::GetSelf(L, 1);
        auto cdr = pop_front(LuaArgsToTuple<ARGS...>(L, 1));
        auto args = std::tuple_cat(std::make_tuple(self), cdr);
        R r = std::apply(f, args);
        return LuaPush<R>::Push(L, r);
    };
}

// void
template <typename T, typename F, typename C, typename... ARGS>
LuaFunc MetaMethodSelfFromStack1(T *, MetaKey key, const F &f, void (C::*m)(ARGS...) const)
{
    // stack#1: userdata
    return [f](lua_State *L) {
        auto self = Traits<T>::GetSelf(L, 1);
        auto cdr = pop_front(LuaArgsToTuple<ARGS...>(L, 1));
        auto args = std::tuple_cat(std::make_tuple(self), cdr);
        std::apply(f, args);
        return 0;
    };
}

template <typename T, typename F, typename C, typename R>
LuaFunc LambdaGetterSelfFromStack1(T *, const char *name, const F &f, R (C::*)(typename Traits<T>::RawType *) const)
{
    // stack#1: userdata
    return [f](lua_State *L) {
        auto value = Traits<T>::GetSelf(L, 1);
        R r = f(value);
        return LuaPush<R>::Push(L, r);
    };
}

// for field
template <typename T, typename C, typename R>
LuaFunc FieldGetterSelfFromStack1(T *, const char *name, R C::*f)
{
    // stack#1: userdata
    return [f](lua_State *L) {
        auto value = Traits<T>::GetSelf(L, 1);
        R r = value->*f;
        return LuaPush<R>::Push(L, r);
    };
}

#pragma endregion

#pragma region userdata by upvalue2

template <typename T, typename R, typename C, typename... ARGS, std::size_t... IS>
LuaFunc MethodSelfFromUpvalue2(T *, const char *name, R (C::*m)(ARGS...), std::index_sequence<IS...>)
{
    using RawType = typename Traits<T>::RawType;

    // upvalue#2: userdata
    return [m](lua_State *L) {
        auto value = Traits<T>::GetSelf(L, lua_upvalueindex(2));
        auto args = LuaArgsToTuple<remove_const_ref<ARGS>::type...>(L, 1);
        return Applyer<R, RawType, ARGS...>::Apply(L, value, m, std::get<IS>(args)...);
    };
}

template <typename T, typename R, typename C, typename... ARGS, std::size_t... IS>
LuaFunc ConstMethodSelfFromUpvalue2(T *, const char *name, R (C::*m)(ARGS...) const, std::index_sequence<IS...>)
{
    using RawType = typename Traits<T>::RawType;

    // upvalue#2: userdata
    return [m](lua_State *L) {
        auto value = Traits<T>::GetSelf(L, lua_upvalueindex(2));
        auto args = LuaArgsToTuple<ARGS...>(L, 1);
        return ConstApplyer<R, RawType, ARGS...>::Apply(L, value, m, std::get<IS>(args)...);
    };
}

#pragma endregion

} // namespace perilune