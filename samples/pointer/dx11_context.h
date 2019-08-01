#pragma once

struct WindowState;
class DX11ContextImpl;
class DX11Context
{
    DX11ContextImpl *m_impl = nullptr;
public:
    DX11Context();
    ~DX11Context();
    void* Create(void *hwnd);
    // Get I3D11DeviceContext that has backbuffer
    void* NewFrame(const WindowState *state);
    void Present();
    void *GetDeviceContext();
};
