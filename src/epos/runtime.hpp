#pragma once
#include <windows.h>
#include <wrl/client.h>
#include <string>
#include <string_view>

namespace epos {

template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

std::wstring S2W(std::string_view str);
std::string W2S(std::wstring_view wcs);

}  // namespace epos