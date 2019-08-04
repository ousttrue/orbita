# perilune

header only lua binding

## features

* [x] this pointer by upvalue
* [x] getter
* [ ] setter
* [ ] operator
* [x] indexer
* [ ] generic typed array
* [x] pointer type
* [ ] metatable use hash
* [x] return unknown pointer type to lightuesrdata
* [x] return unknown value type to error
* [ ] placement new

## usage

### value type

for the type that copy side effect is safe.

```c++
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
```

```c++
    static perilune::ValueType<Vector3> vector3Type;
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
```

```lua
local zero = Vector3.Zero()
print(zero)
print(zero.x)

local v = Vector3.Vector3(1, 2, 3)
print(v)
print(v.x)
print(v.y)
print(v.z)
local n = v.sqnorm()
print(n)
```

### pointer type

for the type that cannot copy or copy side effect is not safe.
template specialized.

```cpp
    static perilune::UserType<Win32Window *> windowType;
    windowType
        .StaticMethod("new", []() {
            return new Win32Window;
        })
        .MetaMethod(perilune::MetaKey::__gc, [](Win32Window *p) {
            std::cerr << "destruct: " << p << std::endl;
            delete p;
        })
        .IndexDispatcher([](auto d) {
            d->Method("create", &Win32Window::Create);
            d->Method("is_running", &Win32Window::IsRunning);
            d->Method("get_state", &Win32Window::GetState);
        })
        .LuaNewType(lua.L);
```
