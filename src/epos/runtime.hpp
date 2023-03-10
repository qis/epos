#pragma once
#include <windows.h>
#include <wrl/client.h>

namespace epos {

template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

}  // namespace epos