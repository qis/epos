#pragma once
#include <windows.h>
#include <source_location>
#include <system_error>

namespace epos {

std::error_category& error_category() noexcept;

std::error_code error(DWORD code) noexcept;
std::error_code error(HRESULT result) noexcept;

void check(
  HRESULT result,
  const char* what = nullptr,
  const std::source_location location = std::source_location::current());

#ifdef NDEBUG
#  define HR(result) result
#else
#  define HR(result) check(result)
#endif

}  // namespace epos