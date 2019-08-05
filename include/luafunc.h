#pragma once
#include "common.h"

namespace perilune
{

using LuaFunc = std::function<int(lua_State *)>;
inline int LuaFuncClosure(lua_State *L)
{
    try
    {
        // execute logic from upvalue
        auto func = (LuaFunc *)lua_touserdata(L, lua_upvalueindex(1));
        return (*func)(L);
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

template <typename T, typename F, typename R, typename C, typename... ARGS>
LuaFunc Stack1SelfMethod(T *, MetaKey key, const F &f, R (C::*m)(ARGS...) const)
{
    return [f](lua_State *L) {
        auto self = Traits<T>::GetSelf(L, 1);
        R r = f(self);
        return LuaPush<R>::Push(L, r);
    };
}

// void
template <typename T, typename F, typename C, typename... ARGS>
LuaFunc Stack1SelfMethod(T *, MetaKey key, const F &f, void (C::*m)(ARGS...) const)
{
    return [f](lua_State *L) {
        auto self = Traits<T>::GetSelf(L, 1);
        f(self);
        return 0;
    };
}

} // namespace perilune