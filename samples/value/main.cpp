#include <perilune/perilune.h>
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

    Vector3 operator+(const Vector3 &v) const
    {
        return Vector3(x + v.x, y + v.y, z + v.z);
    }

    float SqNorm() const
    {
        return x * x + y * y + z * z;
    }
};

namespace perilune
{

template <>
struct LuaTable<Vector3>
{
    static Vector3 Get(lua_State *L, int index)
    {
        Vector3 v;

        lua_pushinteger(L, 1);
        lua_gettable(L, -2);
        v.x = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);

        lua_pushinteger(L, 2);
        lua_gettable(L, -2);
        v.y = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);

        lua_pushinteger(L, 3);
        lua_gettable(L, -2);
        v.z = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);

        return v;
    }
};

} // namespace perilune

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
        .StaticMethod("New", [](float x, float y, float z) { return Vector3(x, y, z); })
        .MetaMethod(perilune::MetaKey::__add, [](Vector3 *a, Vector3 b) {
            return *a + b;
        })
        .MetaMethod(perilune::MetaKey::__tostring, [](Vector3 *v) {
            std::stringstream ss;
            ss << "[" << v->x << ", " << v->y << ", " << v->z << "]";
            return ss.str();
        })
        .MetaIndexDispatcher([](auto d) {
            d->Getter("x", [](Vector3 *value) {
                return value->x;
            });
            // member pointer
            d->Getter("y", &Vector3::y);
            d->Getter("z", &Vector3::z);
            d->Method("sqnorm", &Vector3::SqNorm);
        })
        // create and push lua stack
        .LuaNewType(lua.L);
    lua_setglobal(lua.L, "Vector3");

    typedef std::vector<Vector3> Vector3List;
    perilune::UserType<Vector3List *> vector3ListType;
    perilune::AddDefaultMethods(vector3ListType);
    vector3ListType
        .StaticMethod("New", []() { return new Vector3List; })
        .LuaNewType(lua.L);
    lua_setglobal(lua.L, "Vector3List");

    if (!lua.DoFile(argv[1]))
    {
        return 2;
    }

    return 0;
}
