#pragma once

namespace perilune
{

#include <Windows.h>
#include <string>

inline std::wstring utf8_to_wstring(const std::string &src)
{
    auto required = MultiByteToWideChar(CP_UTF8, 0, src.data(), (int)src.size(), nullptr, 0);
    std::wstring dst(required, 0);
    MultiByteToWideChar(CP_UTF8, 0, src.data(), (int)src.size(), dst.data(), required);
    return dst;
}

} // namespace perilune
