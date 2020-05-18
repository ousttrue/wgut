#pragma once
#include <wrl/client.h>

namespace wgut
{

template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

// inline void ThrowIfFailed(HRESULT hr)
// {
//     if (FAILED(hr))
//     {
//         // Set a breakpoint on this line to catch Win32 API errors.
//         throw Platform::Exception::CreateException(hr);
//     }
// }

} // namespace wgut
