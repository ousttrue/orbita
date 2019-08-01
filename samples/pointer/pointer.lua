
local window = Window.new()
local hwnd = window.create(640, 480, "pointer");
if not hwnd then
    print("fail to Window.create")
    return
end

local dx11 = Dx11.new()
local device = dx11.create(hwnd)
if not device then
    print("fail to Dx11.create")
    return
end

while window.is_running() do

    local state = window.get_state()

    local context = dx11.new_frame(state)
    -- do something

    dx11.present()

end
