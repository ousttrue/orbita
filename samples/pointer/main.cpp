#include <perilune.h>
#include <iostream>
#include "win32_window.h"
#include "dx11_context.h"
#include <plog/Log.h>

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

int main(int argc, char **argv)
{
    if (argc == 1)
    {
        std::cerr << "require argument" << std::endl;
        return 1;
    }

    Lua lua;

    static perilune::UserType<Win32Window *> windowType;
    windowType
        .StaticMethod("new", []() {
            return new Win32Window;
        })
        .Destructor([](Win32Window *p) {
            std::cerr << "destruct: " << p << std::endl;
            delete p;
        })
        .Method("create", &Win32Window::Create)
        .Method("is_running", &Win32Window::IsRunning)
        .Method("get_state", &Win32Window::GetState)
        .LuaNewType(lua.L);
    lua_setglobal(lua.L, "Window");

    static perilune::UserType<DX11Context *> dx11;
    dx11
        .StaticMethod("new", []() { return new DX11Context; })
        .Destructor([](DX11Context *p) { delete p; })
        .Method("create", &DX11Context::Create)
        .LuaNewType(lua.L);
    lua_setglobal(lua.L, "Dx11");

    if (!lua.DoFile(argv[1]))
    {
        return 2;
    }

    return 0;
}
