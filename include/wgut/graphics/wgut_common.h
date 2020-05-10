#pragma once
#include <wrl/client.h>

namespace wgut
{

template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

}
