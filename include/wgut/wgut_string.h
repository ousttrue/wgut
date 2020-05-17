#pragma once
#include <string>
#include <windows.h>

namespace wgut
{

inline std::wstring ToUnicode(const std::string &src, UINT cp)
{
    auto size = MultiByteToWideChar(cp, 0, src.data(), -1, nullptr, 0);
    std::wstring dst;
    dst.resize(size);
    MultiByteToWideChar(cp, 0, src.data(), -1, (wchar_t *)dst.data(), static_cast<int>(dst.size()));
    return dst;
}

inline std::wstring CP932ToUnicode(const std::string &src)
{
    return ToUnicode(src, 932);
}

} // namespace wgut
