#pragma once
#include "common.h"

namespace perilune
{

template <auto M, typename R, typename C, typename... ARGS>
constexpr auto _OpenMethod(R (C::*m)(ARGS...))
{
    struct inner
    {
        static R call(C *self, ARGS... args)
        {
            return (self->*M)(args...);
        }
    };
    return &inner::call;
}

template <auto M>
constexpr auto OpenMethod()
{
    return _OpenMethod<M>(M);
}

template <typename F, typename... ARGS, std::size_t... IS>
int _Apply(lua_State *L, F f, const std::tuple<ARGS...> &args, std::index_sequence<IS...>)
{
    return f(std::get<IS>(args)...);
}

template <typename F, typename... ARGS>
int Apply(lua_State *L, F f, const std::tuple<ARGS...> &args)
{
    return _Apply(L, f, args, std::index_sequence_for<ARGS...>());
}

template <typename R, typename T, typename... ARGS>
struct Applyer
{
    static int Apply(lua_State *L, typename Traits<T>::RawType *value, R (T::*m)(ARGS...), ARGS... args)
    {
        auto r = (value->*m)(args...);
        return LuaPush<R>::Push(L, r);
    }
};
template <typename R, typename T, typename... ARGS>
struct Applyer<R &, T, ARGS...>
{
    static int Apply(lua_State *L, typename Traits<T>::RawType *value, R &(T::*m)(ARGS...), ARGS... args)
    {
        auto &r = (value->*m)(args...);
        return LuaPush<R *>::Push(L, &r);
    }
};
template <typename T, typename... ARGS>
struct Applyer<void, T, ARGS...>
{
    static int Apply(lua_State *L, typename Traits<T>::RawType *value, void (T::*m)(ARGS...), ARGS... args)
    {
        (value->*m)(args...);
        return 0;
    }
};

template <typename R, typename T, typename... ARGS>
struct ConstApplyer
{
    static int Apply(lua_State *L, typename Traits<T>::RawType *value, R (T::*m)(ARGS...) const, ARGS... args)
    {
        auto r = (value->*m)(args...);
        return LuaPush<R>::Push(L, r);
    }
};
template <typename R, typename T, typename... ARGS>
struct ConstApplyer<R &, T, ARGS...>
{
    static int Apply(lua_State *L, typename Traits<T>::RawType *value, R &(T::*m)(ARGS...) const, ARGS... args)
    {
        auto &r = (value->*m)(args...);
        return LuaPush<R *>::Push(L, &r);
    }
};
template <typename T, typename... ARGS>
struct ConstApplyer<void, T, ARGS...>
{
    static int Apply(lua_State *L, typename Traits<T>::RawType *value, void (T::*m)(ARGS...) const, ARGS... args)
    {
        (value->*m)(args...);
        return 0;
    }
};

} // namespace perilune