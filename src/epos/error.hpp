#pragma once
#include <windows.h>
#include <format>
#include <source_location>
#include <system_error>

namespace epos {

std::error_category& error_category() noexcept;

std::error_code error(DWORD code) noexcept;
std::error_code error(HRESULT result) noexcept;

__forceinline void check(
  HRESULT result,
  const char* what = nullptr,
  const std::source_location location = std::source_location::current())
{
  if (FAILED(result)) {
    auto message = std::format(
      "{} {}:{} {}",
      location.file_name(),
      location.line(),
      location.column(),
      location.function_name());
    if (what) {
      message.append(": ");
      message.append(what);
    }
    throw std::system_error{ result, epos::error_category(), message };
  }
}

#ifdef NDEBUG
#  define HR(result) result
#else
#  define HR(result) check(result)
#endif

}  // namespace epos