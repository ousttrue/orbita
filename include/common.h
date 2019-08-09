#pragma once

extern "C"
{
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

#include <assert.h>
#include <exception>
#include <functional>

namespace perilune
{

enum class MetaKey
{
    __gc,
    __len,
    __ipairs,
    __pairs,
    __tostring,
    __call,
    __newindex,
    __add,
    __concat,
    // __index, use IndexDispatcher
};

static const char *ToString(MetaKey key)
{
    switch (key)
    {
    case MetaKey::__gc:
        return "__gc";
    case MetaKey::__len:
        return "__len";
    case MetaKey::__ipairs:
        return "__ipairs";
    case MetaKey::__pairs:
        return "__pairs";
    case MetaKey::__tostring:
        return "__tostring";
    case MetaKey::__call:
        return "__call";
    case MetaKey::__newindex:
        return "__newindex";
    case MetaKey::__add:
        return "__add";
    case MetaKey::__concat:
        return "__concat";
    }

    throw std::exception("unknown key");
}

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
    using RawType = T;

    static RawType *GetSelf(lua_State *L, int index)
    {
        auto p = (T *)lua_touserdata(L, index);
        return p;
    }

    static int Destruct(lua_State *L)
    {
        auto self = GetSelf(L, 1);
        self->~T();
        return 0;
    }

    static void SetPlacementDelete(lua_State *L, int index)
    {
        lua_pushcfunction(L, &Destruct);
        lua_setfield(L, index, "__gc");
    }
};

// for pointer type
template <typename T>
struct Traits<T *>
{
    using RawType = T;

    using PT = T *;

    static RawType *GetSelf(lua_State *L, int index)
    {
        return *(PT *)lua_touserdata(L, index);
    }

    static void SetPlacementDelete(lua_State *L, int index)
    {
        // do nothing
    }
};

// for shared_ptr
template <typename T>
struct Traits<std::shared_ptr<T>>
{
    using RawType = T;

    using PT = std::shared_ptr<T>;

    static RawType *GetSelf(lua_State *L, int index)
    {
        return ((PT *)lua_touserdata(L, index))->get();
    }

    static int Destruct(lua_State *L)
    {
        auto self = GetSelf(L, 1);
        self->~T();
        return 0;
    }

    static void SetPlacementDelete(lua_State *L, int index)
    {
        lua_pushcfunction(L, &Destruct);
        lua_setfield(L, index, "__gc");
    }
};

template <typename T>
struct remove_const_ref
{
    using no_ref = typename std::remove_reference<T>::type;
    using type = typename std::remove_const<no_ref>::type;
};

template <typename Tuple, std::size_t... Is>
auto pop_front_impl(const Tuple &tuple, std::index_sequence<Is...>)
{
    return std::make_tuple(std::get<1 + Is>(tuple)...);
}

template <typename Tuple>
auto pop_front(const Tuple &tuple)
{
    return pop_front_impl(tuple,
                          std::make_index_sequence<std::tuple_size<Tuple>::value - 1>());
}

} // namespace perilune