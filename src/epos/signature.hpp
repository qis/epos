#pragma once
#include "deus.h"
#include <concepts>
#include <string_view>
#include <vector>
#include <cassert>
#include <cstddef>

namespace epos {

// clang-format off

template <class T>
concept SignatureScanCallback = requires(T&& callback, void* data) {
  { std::forward<T>(callback)(data) } -> std::convertible_to<bool>;
};

template <class T>
concept SignatureScanConstCallback = requires(T&& callback, const void* data) {
  { std::forward<T>(callback)(data) } -> std::convertible_to<bool>;
};

// clang-format on

class signature {
public:
  signature(const void* data, std::size_t size) noexcept :
    data_(static_cast<const std::byte*>(data), static_cast<const std::byte*>(data) + size),
    size_(size)
  {}

  signature(const void* data, const void* mask, std::size_t size) noexcept :
    data_(size * 2), size_(size)
  {
    std::memcpy(data_.data(), data, size);
    std::memcpy(data_.data() + size, mask, size);
  }

  signature(std::string_view signature) noexcept : size_((signature.size() + 1) / 3)
  {
    assert((signature.size() + 1) / 3 > 0);
    assert((signature.size() + 1) % 3 == 0);

    if (signature.empty()) {
      return;
    }
    if (signature.find('?') != std::string_view::npos) {
      data_.resize(size_ * 2);
      const auto mask = data_.data() + size_;
      for (std::size_t i = 0; i < size_; i++) {
        mask[i] = mask_cast(signature[i * 3]) << 4 | mask_cast(signature[i * 3 + 1]);
      }
    } else {
      data_.resize(size_);
    }
    const auto data = data_.data();
    for (std::size_t i = 0; i < size_; i++) {
      data[i] = data_cast(signature[i * 3]) << 4 | data_cast(signature[i * 3 + 1]);
      assert(i == 0 || signature[i * 3 - 1] == ' ');
    }
  }

  __forceinline void* scan(void* begin, void* end) const noexcept
  {
    const auto bytes_begin = reinterpret_cast<std::byte*>(begin);
    const auto bytes_end = reinterpret_cast<std::byte*>(end);
    const auto pos = find(bytes_begin, bytes_end, data(), mask(), size());
    return reinterpret_cast<void*>(bytes_begin + pos);
  }

  __forceinline const void* scan(const void* begin, const void* end) const noexcept
  {
    const auto bytes_begin = reinterpret_cast<const std::byte*>(begin);
    const auto bytes_end = reinterpret_cast<const std::byte*>(end);
    const auto pos = find(bytes_begin, bytes_end, data(), mask(), size());
    return reinterpret_cast<const void*>(bytes_begin + pos);
  }

  __forceinline void scan(void* begin, void* end, SignatureScanCallback auto&& callback) const
  {
    const auto signature_data = data();
    const auto signature_mask = mask();
    const auto signature_size = size();
    const auto bytes_begin = reinterpret_cast<std::byte*>(begin);
    const auto bytes_end = reinterpret_cast<std::byte*>(end);
    for (auto address = bytes_begin; address != bytes_end; address++) {
      address += find(address, bytes_end, signature_data, signature_mask, signature_size);
      if (address == bytes_end || !callback(address)) {
        break;
      }
    }
  }

  __forceinline void scan(const void* begin, const void* end, SignatureScanConstCallback auto&& callback) const
  {
    const auto signature_data = data();
    const auto signature_mask = mask();
    const auto signature_size = size();
    const auto bytes_begin = reinterpret_cast<const std::byte*>(begin);
    const auto bytes_end = reinterpret_cast<const std::byte*>(end);
    for (auto address = bytes_begin; address != bytes_end; address++) {
      address += find(address, bytes_end, signature_data, signature_mask, signature_size);
      if (address == bytes_end || !callback(address)) {
        break;
      }
    }
  }

  constexpr std::byte* data() noexcept
  {
    return data_.data();
  }

  constexpr const std::byte* data() const noexcept
  {
    return data_.data();
  }

  constexpr std::byte* mask() noexcept
  {
    return data_.size() == size_ ? nullptr : data_.data() + size_;
  }

  constexpr const std::byte* mask() const noexcept
  {
    return data_.size() == size_ ? nullptr : data_.data() + size_;
  }

  constexpr std::size_t size() const noexcept
  {
    return size_;
  }

private:
  static std::size_t find(
    const std::byte* begin,
    const std::byte* end,
    const std::byte* data,
    const std::byte* mask,
    std::size_t size) noexcept;

  static constexpr std::byte data_cast(char c) noexcept
  {
    if (c >= '0' && c <= '9') {
      return std::byte(c - '0');
    }
    if (c >= 'A' && c <= 'F') {
      return std::byte(c - 'A' + 0xA);
    }
    if (c >= 'a' && c <= 'f') {
      return std::byte(c - 'a' + 0xA);
    }
    assert(c == '?');
    return std::byte(0);
  }

  static constexpr std::byte mask_cast(char c) noexcept
  {
    return std::byte(c == '?' ? 0x0 : 0xF);
  }

  std::vector<std::byte> data_;
  std::size_t size_{ 0 };
};

}  // namespace epos