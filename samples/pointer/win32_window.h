#pragma once
#include "window_state.h"
#include <string>

///
/// Windows API Window
///
/// * window size
/// * mouse inputs
///
class Win32Window
{
    class Win32WindowImpl *m_impl = nullptr;

public:
    Win32Window();
    ~Win32Window();
    void* Create(int w, int h, const std::wstring &title);
    bool IsRunning();
    const WindowState &GetState() const;
};
