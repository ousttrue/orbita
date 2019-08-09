#include <catch.hpp>
#include <perilune.h>

static int g_new = 0;
static int g_copy = 0;

struct Value
{
    int m_id;
    Value()
        : m_id(g_new++)
    {
    }

    Value(const Value &rhs)
    {
        *this = rhs;
    }

    Value &operator=(const Value &rhs)
    {
        ++g_copy;
        m_id = rhs.m_id;
        return *this;
    }
};

TEST_CASE("value type test", "[value]")
{

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

print('hello')
local value = Value.new()
print(value.id)

)"");

    lua_close(L);

    // new in static method
    // placement new for userdata
    // copy to userdata from static method return
    REQUIRE(2 == g_new);
    REQUIRE(1 == g_copy);
}
