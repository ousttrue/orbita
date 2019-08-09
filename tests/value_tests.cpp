#include <catch.hpp>
#include <perilune/perilune.h>

TEST_CASE("value type test", "[value]")
{
    static int s_new = 0;
    static int s_copy = 0;

    struct Value
    {
        int m_id;
        Value()
            : m_id(s_new++)
        {
        }

        Value(const Value &rhs)
        {
            *this = rhs;
        }

        Value &operator=(const Value &rhs)
        {
            ++s_copy;
            m_id = rhs.m_id;
            return *this;
        }
    };

    auto L = luaL_newstate();
    luaL_openlibs(L);

    {
        static perilune::UserType<Value> valueType;
        valueType
            .StaticMethod("new", []() { return Value(); })
            .MetaIndexDispatcher([](auto d) {
                d->Getter("id", &Value::m_id);
            })
            .LuaNewType(L);
        lua_setglobal(L, "Value");
    }

    luaL_dostring(L, R""(

-- print('hello')
local value = Value.new()
-- access to id
local id = value.id
-- print(value.id)

)"");

    lua_close(L);

    // new in static method
    // placement new for userdata
    // copy to userdata from static method return
    REQUIRE(2 == s_new);
    REQUIRE(1 == s_copy);
}

TEST_CASE("placement new", "[value]")
{
    static int s_new = 0;
    static int s_copy = 0;

    struct Value
    {
        int m_id;
        Value()
            : m_id(s_new++)
        {
        }

        Value(const Value &rhs)
        {
            *this = rhs;
        }

        Value &operator=(const Value &rhs)
        {
            ++s_copy;
            m_id = rhs.m_id;
            return *this;
        }
    };

    auto L = luaL_newstate();
    luaL_openlibs(L);

    {
        static perilune::UserType<Value> valueType;
        valueType
            .PlacementNew("new")
            .MetaIndexDispatcher([](auto d) {
                d->Getter("id", &Value::m_id);
            })
            .LuaNewType(L);
        lua_setglobal(L, "Value");
    }

    luaL_dostring(L, R""(

-- print('hello')
local value = Value.new()
-- access to id
local id = value.id
-- print(value.id)

)"");

    lua_close(L);

    // new in static method
    // placement new for userdata
    // copy to userdata from static method return
    REQUIRE(1 == s_new);
    REQUIRE(0 == s_copy);
}

TEST_CASE("reference", "[value]")
{
    static int s_new = 0;
    static int s_copy = 0;
    static int s_dest = 0;

    struct Value
    {
        int m_id;
        float N;

        Value(float n)
            : m_id(s_new++), N(n)
        {
        }

        ~Value()
        {
            ++s_dest;
        }

        Value(const Value &rhs)
        {
            *this = rhs;
        }

        Value &operator=(const Value &rhs)
        {
            ++s_copy;
            // m_id = rhs.m_id;
            N = rhs.N;
            return *this;
        }
    };

    auto L = luaL_newstate();
    luaL_openlibs(L);

    {
        static perilune::UserType<Value> valueType;
        valueType
            .PlacementNew<float>("new")
            .StaticMethod("get", [](const Value &v) {
                return v.N;
            })
            .LuaNewType(L);
        lua_setglobal(L, "Value");
    }

    luaL_dostring(L, R""(

local value = Value.new(2)
return Value.get(value)

)"");

    // new in static method
    // placement new for userdata
    // copy to userdata from static method return
    REQUIRE(2 == lua_tointeger(L, -1));

    lua_close(L);

    REQUIRE(0 == s_copy);
    REQUIRE(1 == s_dest);
}