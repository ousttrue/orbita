print('hello perilune')

print(Vector3)

local zero = Vector3.Zero()
print(zero)
print(zero.x)

local v = Vector3.New(1, 2, 3)
print(v)
print(v.x)
print(v.y)
print(v.z)
local n = v.sqnorm()
print(n)

local list = Vector3List.New()
print(list, #list)
list.push_back(v)
list.push_back(v)
list.push_back(v)
print(list, #list)

for i, x in ipairs(list) do print(i, x) end

local y = v + Vector3.New(1, 2, 3)
print(y)
local z = y + {1, 2, 3}
print(z)
