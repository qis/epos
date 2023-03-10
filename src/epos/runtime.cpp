#include "runtime.hpp"

namespace epos {

std::wstring S2W(std::string_view str)
{
  if (str.empty()) {
    return {};
  }
  const auto ssize = static_cast<int>(str.size());
  const auto wsize = MultiByteToWideChar(CP_UTF8, 0, str.data(), ssize, nullptr, 0);
  if (wsize <= 0) {
    return {};
  }
  std::wstring wcs;
  wcs.resize(static_cast<std::size_t>(wsize));
  if (!MultiByteToWideChar(CP_UTF8, 0, str.data(), ssize, wcs.data(), wsize)) {
    return {};
  }
  return wcs;
}

std::string W2S(std::wstring_view wcs)
{
  if (wcs.empty()) {
    return {};
  }
  const auto wsize = static_cast<int>(wcs.size());
  const auto ssize = WideCharToMultiByte(CP_UTF8, 0, wcs.data(), wsize, nullptr, 0, nullptr, nullptr);
  if (ssize <= 0) {
    return {};
  }
  std::string str;
  str.resize(static_cast<std::size_t>(ssize));
  if (!WideCharToMultiByte(CP_UTF8, 0, wcs.data(), wsize, str.data(), ssize, nullptr, nullptr)) {
    return {};
  }
  return str;
}

}  // namespace epos