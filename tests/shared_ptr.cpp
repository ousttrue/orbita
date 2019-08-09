#include <catch.hpp>
#include <perilune.h>

TEST_CASE("shared_ptr", "[shared_ptr]")
{
    static int s_new = 0;
    static int s_dest = 0;

    struct Shared
    {
        int m_value;
        int m_id;
        Shared(int value)
            : m_id(s_new++), m_value(value)
        {
        }

        ~Shared()
        {
            ++s_dest;
        }

        Shared(const Shared &rhs) = delete;
        Shared &operator=(const Shared &rhs) = delete;
    };

    auto L = luaL_newstate();
    luaL_openlibs(L);

    {
        static perilune::UserType<std::shared_ptr<Shared>> valueType;
        valueType
            .StaticMethod("new", [](int n) {
                return std::make_shared<Shared>(n);
                // return std::shared_ptr<Shared>(new Shared(n));
            })
            .MetaIndexDispatcher([](auto d) {
                d->Getter("value", &Shared::m_value);
            })
            .LuaNewType(L);
        lua_setglobal(L, "Shared");
    }

    luaL_dostring(L, R""(
local s = Shared.new(2)
-- return s
)"");

    // REQUIRE(2 == lua_tointeger(L, -1));

    // auto p = (std::shared_ptr<Shared> *)lua_touserdata(L, -1);
    // REQUIRE(1 == p->use_count());
    REQUIRE(0 == s_dest);

    lua_close(L);

    REQUIRE(1 == s_new);
    REQUIRE(1 == s_dest);
}