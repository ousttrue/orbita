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
        .IndexDispatcher([](auto d) {
            d->Getter("x", [](const Vector3 &value) {
                return value.x;
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
    vector3ListType
        .StaticMethod("New", []() { return new Vector3List; })
        .MetaMethod(perilune::MetaKey::__len, [](Vector3List *p) {
            return p->size();
        })
        .IndexDispatcher([](perilune::IndexDispatcher<Vector3List *> *d) {
            // upvalue#2: userdata
            d->Method("push_back", [](lua_State *L) {
                auto value = perilune::internal::Traits<Vector3List *>::GetSelf(L, lua_upvalueindex(2));
                auto v = perilune::internal::LuaGet<Vector3>::Get(L, 1);
                value->push_back(v);
                return 0;
            });
        })
        .LuaNewType(lua.L);
    lua_setglobal(lua.L, "Vector3List");

    if (!lua.DoFile(argv[1]))
    {
        return 2;
    }

    return 0;
}
