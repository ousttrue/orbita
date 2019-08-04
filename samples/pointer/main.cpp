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
        .MetaMethod(perilune::MetaKey::__gc, [](Win32Window *p) {
            std::cerr << "destruct: " << p << std::endl;
            delete p;
        })
        .MetaIndexDispatcher([](auto d) {
            d->Method("create", &Win32Window::Create);
            d->Method("is_running", &Win32Window::IsRunning);
            d->Method("get_state", &Win32Window::GetState);
        })
        .LuaNewType(lua.L);
    lua_setglobal(lua.L, "Window");

    static perilune::UserType<DX11Context *> dx11;
    dx11
        .StaticMethod("new", []() { return new DX11Context; })
        .MetaMethod(perilune::MetaKey::__gc, [](DX11Context *p) { delete p; })
        .MetaIndexDispatcher([](auto d) {
            d->Method("create", &DX11Context::Create);
            d->Method("new_frame", &DX11Context::NewFrame);
            d->Method("present", &DX11Context::Present);
            d->Method("get_context", &DX11Context::GetDeviceContext);
        })
        .LuaNewType(lua.L);
    lua_setglobal(lua.L, "Dx11");

    if (!lua.DoFile(argv[1]))
    {
        return 2;
    }

    return 0;
}
