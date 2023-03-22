#pragma once
#ifdef DEUS_DRIVER
#include <ntifs.h>
#include <ntddk.h>
#else
#include <windows.h>
#include <winioctl.h>
#endif
#include <string_view>

#if defined(DEUS_DRIVER) && !defined(BYTE)
typedef unsigned char BYTE;
#endif

#if defined(DEUS_DRIVER) && !defined(DWORD)
typedef ULONG DWORD;
#else
static_assert(sizeof(DWORD) == sizeof(ULONG));
#endif

namespace deus {

// clang-format off
enum class code : ULONG {
  version = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_IN_DIRECT,  FILE_SPECIAL_ACCESS),
  open    = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_IN_DIRECT,  FILE_SPECIAL_ACCESS),
  close   = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_NEITHER,    FILE_SPECIAL_ACCESS),
  modules = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_OUT_DIRECT, FILE_SPECIAL_ACCESS),
  regions = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x804, METHOD_OUT_DIRECT, FILE_SPECIAL_ACCESS),
  read    = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x805, METHOD_OUT_DIRECT, FILE_SPECIAL_ACCESS),
  write   = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x806, METHOD_OUT_DIRECT, FILE_SPECIAL_ACCESS),
  watch   = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x807, METHOD_IN_DIRECT,  FILE_SPECIAL_ACCESS),
  update  = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x808, METHOD_NEITHER,    FILE_SPECIAL_ACCESS),
  stop    = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x809, METHOD_NEITHER,    FILE_SPECIAL_ACCESS),
};
// clang-format on

constexpr DWORD version = 4;

namespace memory {

static constexpr UINT_PTR min = 0x00000000'00400000;
static constexpr UINT_PTR max = 0x000F0000'00000000;

}  // namespace memory

/// Information about modules loaded by the process.
struct alignas(16) module : SLIST_ENTRY {
  UINT_PTR base{ 0 };
  SIZE_T length{ 0 };

  std::wstring_view name() const noexcept
  {
    if (length) {
      return { reinterpret_cast<PCWSTR>(this + 1), length / sizeof(WCHAR) };
    }
    return {};
  }
};

/// Information about a range of committed pages in the virtual address space of a process.
/// No reported regions will have PAGE_NOACCESS or PAGE_GUARD protect flags set.
struct alignas(16) region : SLIST_ENTRY {
  /// A pointer to the base address of the region of pages.
  UINT_PTR base_address{ 0 };

  /// A pointer to the base address of a range of pages allocated by the application.
  /// The page pointed to by @ref base_address is contained within this allocation range.
  UINT_PTR allocation_base{ 0 };

  /// The memory protection option when the region was initially allocated.
  /// https://learn.microsoft.com/en-us/windows/win32/memory/memory-protection-constants
  ULONG allocation_protect{ 0 };

  /// The size of the region beginning at @ref base_address in which all pages have identical attributes.
  SIZE_T region_size{ 0 };

  /// The access protection of the pages in the region.
  /// https://learn.microsoft.com/en-us/windows/win32/memory/memory-protection-constants
  ULONG protect{ 0 };

  /// The type of pages in the region. The following types are defined.
  /// https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-memory_basic_information
  ULONG type{ 0 };
};

/// Regions filter parameters and result.
struct alignas(16) regions {
  /// Bitmask for allowed type of pages in the region.
  /// Example: MEM_MAPPED | MEM_PRIVATE
  ULONG type{ 0xFFFFFFFF };

  /// Bitmask for allowed access protection of the pages in the region.
  /// Example: PAGE_READONLY | PAGE_READWRITE
  ULONG protect{ 0xFFFFFFFF };

  /// Bitmask for allowed memory protection option when the region was initially allocated.
  /// Example: PAGE_READONLY | PAGE_READWRITE
  ULONG allocation_protect{ 0xFFFFFFFF };

  /// Allowed region size.
  SIZE_T region_size{ 0 };

  /// Linked list with region entries.
  PSLIST_HEADER result{ nullptr };
};

struct alignas(32) copy {
  UINT_PTR src{ 0 };
  UINT_PTR dst{ 0 };
  SIZE_T size{ 0 };
  SIZE_T copied{ 0 };
};

}  // namespace deus
