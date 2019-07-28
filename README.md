# perilune

header only lua binding

## features

* [x] function table to registry
* [x] getter
* [ ] setter
* [ ] operator
* [ ] indexer
* [ ] generic typed array
* [ ] PointerType
* [ ] metatable use hash

## usage

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
    perilune::ValueType<Vector3> vector3Type;
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
        .NewType(lua.L);
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
