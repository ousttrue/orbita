#pragma once
#include "common.h"
#include "luafunc.h"

namespace perilune
{

class StaticMethodMap
{
    std::unordered_map<std::string, LuaFunc> m_methodMap;

public:
    template <typename F, typename C, typename R, typename... ARGS>
    void StaticMethod(const char *name, const F &f, R (C::*m)(ARGS...) const)
    {
        auto lf = ToLuaFunc(name, f, m, std::index_sequence_for<ARGS...>());
        m_methodMap.insert(std::make_pair(name, lf));
    }

    // stack#1: userdata
    // stack#2: key
    int Dispatch(lua_State *L)
    {
        auto key = lua_tostring(L, 2);
        if (key)
        {
            auto found = m_methodMap.find(key);
            if (found != m_methodMap.end())
            {
                // upvalue#1
                lua_pushlightuserdata(L, &found->second);

                // return closure
                lua_pushcclosure(L, &LuaFuncClosure, 1);
                return 1;
            }
            else
            {
                lua_pushfstring(L, "'%s' not found", key);
                lua_error(L);
                return 1;
            }
        }
        else
        {
            lua_pushfstring(L, "unknown key type '%s'", lua_typename(L, 2));
            lua_error(L);
            return 1;
        }
    }
};

} // namespace perilune