#include "error.hpp"
#include <format>

namespace epos {
namespace detail {

class error_category : public std::error_category {
public:
  const char* name() const noexcept override final
  {
    return "epos";
  }

  std::string message(int ev) const override final
  {
    const auto code = static_cast<DWORD>(ev);
    const auto status = static_cast<LONG>(ev);
    auto text = std::format("error 0x{:08X} ({}): ", code, status);
    char* data = nullptr;
    DWORD size = FormatMessage(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
      nullptr,
      code,
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      reinterpret_cast<LPSTR>(&data),
      0,
      nullptr);
    if (data) {
      if (size) {
        text.append(data, size);
      }
      LocalFree(data);
      if (size) {
        return text;
      }
    }
    switch (code >> 30) {
    case 0:
      text.append("Unknown success.");
      break;
    case 1:
      text.append("Unknown information.");
      break;
    case 2:
      text.append("Unknown warning.");
      break;
    case 3:
      text.append("Unknown error.");
      break;
    default:
      text.append("Unknown status.");
      break;
    }
    return text;
  }
};

}  // namespace detail

std::error_category& error_category() noexcept
{
  static detail::error_category category;
  return category;
}

std::error_code error(DWORD code) noexcept
{
  return { static_cast<int>(code), error_category() };
}

std::error_code error(HRESULT result) noexcept
{
  return { static_cast<int>(result), error_category() };
}

}  // namespace epos