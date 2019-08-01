#include <perilune.h>
#include <iostream>

struct Lua
{
    lua_State *L;

    Lua()
        : L(luaL_newstate())
    {
        luaL_requiref(L, "_G", luaopen_base, 1);
        lua_pop(L, 1);
    }

    ~Lua()
    {
        lua_close(L);
    }

    void PrintLuaError()
    {
        std::cerr << lua_tostring(L, -1) << std::endl;
    }

    bool DoFile(const char *file)
    {
        if (luaL_dofile(L, file))
        {
            PrintLuaError();
            return false;
        }
        return true;
    }
};

struct Vector3
{
    float x;
    float y;
    float z;

    Vector3()
        : x(0), y(0), z(0)
    {
    }

    Vector3::Vector3(float x_, float y_, float z_)
        : x(x_), y(y_), z(z_)
    {
    }

    float SqNorm() const
    {
        return x * x + y * y + z * z;
    }
};

int main(int argc, char **argv)
{
    if (argc == 1)
    {
        std::cerr << "require argument" << std::endl;
        return 1;
    }

    Lua lua;

    std::cout << typeid(&Vector3::x).name() << std::endl;

    perilune::UserType<Vector3> vector3Type;
    vector3Type
        // lambda
        .StaticMethod("Zero", []() { return Vector3(); })
        .StaticMethod("Vector3", [](float x, float y, float z) { return Vector3(x, y, z); })
        .Getter("x", [](const Vector3 &value) {
            return value.x;
        })
        // member pointer
        .Getter("y", &Vector3::y)
        .Getter("z", &Vector3::z)
        .Method("sqnorm", &Vector3::SqNorm)
        // create and push lua stack
        .LuaNewType(lua.L);
    lua_setglobal(lua.L, "Vector3");

    if (!lua.DoFile(argv[1]))
    {
        return 2;
    }

    return 0;
}
