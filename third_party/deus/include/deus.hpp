#pragma once
#include "deus.h"
#include <winioctl.h>
#include <winternl.h>
#include <expected>
#include <format>
#include <iterator>
#include <span>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <cstddef>

namespace deus {
namespace detail {

class error_category : public std::error_category {
public:
  const char* name() const noexcept override final
  {
    return "deus";
  }

  std::string message(int ev) const override final
  {
    const auto code = static_cast<DWORD>(ev);
    const auto status = static_cast<NTSTATUS>(ev);
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

inline std::error_category& error_category() noexcept
{
  static detail::error_category category;
  return category;
}

inline std::error_code error(DWORD code) noexcept
{
  return { static_cast<int>(code), error_category() };
}

inline std::error_code error(NTSTATUS status) noexcept
{
  return { static_cast<int>(status), error_category() };
}

template <typename T>
using result = std::expected<T, std::error_code>;

template <class T>
concept ListEntry = std::is_base_of<SLIST_ENTRY, T>::value;

template <ListEntry T>
class list {
public:
  class iterator {
  public:
    using iterator_category = std::input_iterator_tag;
    using difference_type = std::size_t;
    using value_type = T;
    using reference = value_type&;
    using pointer = value_type*;

    constexpr iterator() noexcept = default;
    constexpr iterator(PSLIST_ENTRY entry) noexcept : entry_(entry) {}

    constexpr bool operator==(const iterator& other) const noexcept
    {
      return entry_ == other.entry_;
    }

    constexpr bool operator!=(const iterator& other) const noexcept
    {
      return !(*this == other);
    }

    constexpr iterator& operator++() noexcept
    {
      entry_ = entry_->Next;
      return *this;
    }

    iterator operator++(int) = delete;

    constexpr reference operator*() noexcept
    {
      return *operator->();
    }

    constexpr const reference operator*() const noexcept
    {
      return *operator->();
    }

    constexpr pointer operator->() noexcept
    {
      return static_cast<pointer>(entry_);
    }

    constexpr const pointer operator->() const noexcept
    {
      return static_cast<const pointer>(entry_);
    }

  private:
    PSLIST_ENTRY entry_ = nullptr;
  };

  list() noexcept
  {
    constexpr size_t size = sizeof(SLIST_HEADER);
    constexpr size_t alignment = MEMORY_ALLOCATION_ALIGNMENT;
    header_ = static_cast<PSLIST_HEADER>(_aligned_malloc(size, alignment));
    if (header_) {
      InitializeSListHead(header_);
    }
  }

  constexpr list(list&& other) noexcept : header_(std::exchange(other.header_, nullptr)) {}

  list(const list& other) = delete;

  list& operator=(list&& other) noexcept
  {
    clear();
    if (const auto header = std::exchange(header_, std::exchange(other.header_, nullptr))) {
      _aligned_free(header);
    }
    return *this;
  }

  list& operator=(const list& other) = delete;

  ~list()
  {
    clear();
    if (header_) {
      _aligned_free(header_);
    }
  }

  iterator begin() const noexcept
  {
    if (header_) {
      if (const auto entry = InterlockedPopEntrySList(header_)) {
        InterlockedPushEntrySList(header_, entry);
        return entry;
      }
    }
    return {};
  }

  constexpr iterator end() const noexcept
  {
    return {};
  }

  bool empty() const noexcept
  {
    return begin() == end();
  }

  std::size_t size() const noexcept
  {
    if (header_) {
      return QueryDepthSList(header_);
    }
    return 0;
  }

  void clear() noexcept
  {
    if (header_) {
      for (auto entry = InterlockedFlushSList(header_); entry;) {
        const auto next = entry->Next;
        VirtualFree(static_cast<T*>(entry), 0, MEM_RELEASE);
        entry = next;
      }
    }
  }

  constexpr PSLIST_HEADER header() const noexcept
  {
    return header_;
  }

private:
  PSLIST_HEADER header_ = nullptr;
};

class device {
public:
  constexpr device() noexcept = default;

  constexpr device(device&& other) noexcept :
    handle_(std::exchange(other.handle_, INVALID_HANDLE_VALUE))
  {}

  device(const device& other) = delete;

  device& operator=(device&& other) noexcept
  {
    destroy();
    handle_ = std::exchange(other.handle_, INVALID_HANDLE_VALUE);
    return *this;
  }

  device& operator=(const device& other) = delete;

  ~device()
  {
    destroy();
  }

  result<void> create() noexcept
  {
    if (handle_ != INVALID_HANDLE_VALUE) {
      return std::unexpected(error(ERROR_ALREADY_INITIALIZED));
    }
    const DWORD access = GENERIC_READ | GENERIC_WRITE;
    const DWORD share_mode = OPEN_EXISTING;
    const DWORD attributes = FILE_ATTRIBUTE_NORMAL;
    handle_ = CreateFile("\\\\.\\Deus", access, 0, nullptr, share_mode, attributes, nullptr);
    if (handle_ == INVALID_HANDLE_VALUE) {
      return std::unexpected(error(static_cast<NTSTATUS>(GetLastError())));
    }
    auto data = version;
    if (const auto rv = control(code::version, &data, sizeof(data)); !rv) {
      CloseHandle(std::exchange(handle_, INVALID_HANDLE_VALUE));
      return std::unexpected(rv.error());
    }
    return {};
  }

  result<void> destroy() noexcept
  {
    if (handle_ == INVALID_HANDLE_VALUE) {
      return std::unexpected(error(STATUS_INVALID_HANDLE));
    }
    if (!CloseHandle(std::exchange(handle_, INVALID_HANDLE_VALUE))) {
      return std::unexpected(error(GetLastError()));
    }
    return {};
  }

  result<void> open(DWORD pid) noexcept
  {
    if (const auto rv = control(code::open, &pid, sizeof(pid)); !rv) {
      return std::unexpected(rv.error());
    }
    return {};
  }

  result<void> close() noexcept
  {
    if (const auto rv = control(code::close); !rv) {
      return std::unexpected(rv.error());
    }
    return {};
  }

  result<list<module>> modules() noexcept
  {
    list<module> list;
    if (const auto rv = control(code::modules, list.header(), sizeof(*list.header())); !rv) {
      return std::unexpected(rv.error());
    }
    return list;
  }

  result<list<region>> regions(
    ULONG type = 0xFFFFFFFF,
    ULONG protect = 0xFFFFFFFF,
    ULONG allocation_protect = 0xFFFFFFFF,
    SIZE_T region_size = 0) noexcept
  {
    list<region> list;
    deus::regions data{ type, protect, allocation_protect, region_size, list.header() };
    if (const auto rv = control(code::regions, &data, sizeof(data)); !rv) {
      return std::unexpected(rv.error());
    }
    return list;
  }

  result<SIZE_T> read(UINT_PTR src, void* dst, SIZE_T size) noexcept
  {
    deus::copy data{ src, reinterpret_cast<UINT_PTR>(dst), size, 0 };
    if (const auto rv = control(code::read, &data, sizeof(data)); !rv) {
      return std::unexpected(rv.error());
    }
    return data.copied;
  }

  template <typename T>
  result<void> read(UINT_PTR src, T& dst) noexcept
  {
    if (const auto rv = read(src, &dst, sizeof(dst)); !rv) {
      return std::unexpected(rv.error());
    } else if (*rv != sizeof(dst)) {
      return std::unexpected(error(ERROR_PARTIAL_COPY));
    }
    return {};
  }

  result<SIZE_T> write(const void* src, UINT_PTR dst, SIZE_T size) noexcept
  {
    deus::copy data{ reinterpret_cast<UINT_PTR>(src), dst, size, 0 };
    if (const auto rv = control(code::write, &data, sizeof(data)); !rv) {
      return std::unexpected(rv.error());
    }
    return data.copied;
  }

  template <typename T>
  result<void> write(T& src, UINT_PTR dst) noexcept
  {
    if (const auto rv = write(&src, dst, sizeof(src)); !rv) {
      return std::unexpected(rv.error());
    } else if (*rv != sizeof(src)) {
      return std::unexpected(error(ERROR_PARTIAL_COPY));
    }
    return {};
  }

  result<void> watch(std::span<copy> data) noexcept
  {
    if (const auto rv = control(code::watch, data.data(), sizeof(copy) * data.size()); !rv) {
      return std::unexpected(rv.error());
    }
    return {};
  }

  result<void> update() noexcept
  {
    if (const auto rv = control(code::update); !rv) {
      return std::unexpected(rv.error());
    }
    return {};
  }

  result<void> stop() noexcept
  {
    if (const auto rv = control(code::stop); !rv) {
      return std::unexpected(rv.error());
    }
    return {};
  }

  __forceinline result<DWORD> control(code code) noexcept
  {
    return control(static_cast<ULONG>(code), nullptr, 0, 0);
  }

  __forceinline result<DWORD> control(code code, PVOID data, ULONG size) noexcept
  {
    return control(static_cast<ULONG>(code), data, size, size);
  }

  __forceinline result<DWORD> control(code code, PVOID data, ULONG isize, ULONG osize) noexcept
  {
    return control(static_cast<ULONG>(code), data, isize, osize);
  }

private:
  __forceinline result<DWORD> control(ULONG code, PVOID data, ULONG isize, ULONG osize) noexcept
  {
    DWORD size = 0;
    if (!DeviceIoControl(handle_, code, data, isize, data, osize, &size, nullptr)) {
      if (const auto ev = GetLastError(); ev != ERROR_PARTIAL_COPY) {
        return std::unexpected(error(ev));
      }
    }
    return size;
  }

  HANDLE handle_{ INVALID_HANDLE_VALUE };
};

}  // namespace deus