local window = Window.new()
print(window)
local hwnd = window.create(640, 480, "pointer")
if not hwnd then
    print("fail to Window.create")
    return
end
print(hwnd)

local dx11 = Dx11.new()
print(dx11)
local device = dx11.create(hwnd)
if not device then
    print("fail to Dx11.create")
    return
end
print(device)

local context = dx11.get_context()
print('dx11.get_context', context)

while window.is_running() do

    local state = window.get_state()

    local context = dx11.new_frame(state)
    -- do something

    dx11.present()
end
