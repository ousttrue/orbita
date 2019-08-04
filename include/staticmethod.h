#pragma once
#include "common.h"

namespace perilune
{

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

} // namespace perilune