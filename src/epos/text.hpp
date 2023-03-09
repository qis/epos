#pragma once
#include <epos/com.hpp>
#include <dwrite_3.h>
#include <format>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace epos {

class text {
public:
  struct style {
    std::size_t start;
    std::size_t size;
    IUnknown* effect{ nullptr };
  };

  operator bool() const noexcept
  {
    return !string_.empty();
  }

  void append(wchar_t c)
  {
    string_.push_back(c);
  }

  template <class T>
  void append(ComPtr<T>& effect, wchar_t c)
  {
    styles_.emplace_back(string_.size(), 1, effect.Get());
    string_.push_back(c);
  }

  void append(std::wstring_view string)
  {
    string_.append(string);
  }

  template <class T>
  void append(ComPtr<T>& effect, std::wstring_view string)
  {
    styles_.emplace_back(string_.size(), string.size(), effect.Get());
    string_.append(string);
  }

  template <class... Args>
  void format(std::wformat_string<Args...> format, Args&&... args)
  {
    std::format_to(std::back_inserter(string_), format, std::forward<Args>(args)...);
  }

  template <class T, class... Args>
  void format(ComPtr<T>& effect, std::wformat_string<Args...> format, Args&&... args)
  {
    const auto start = string_.size();
    std::format_to(std::back_inserter(string_), format, std::forward<Args>(args)...);
    styles_.emplace_back(start, string_.size() - start, effect.Get());
  }

  void visualize(const void* data, std::size_t size)
  {
    string_.reserve(string_.size() + size * 3 + size / 16);
    auto dst = std::back_inserter(string_);
    auto src = reinterpret_cast<const BYTE*>(data);
    if (size) {
      const auto byte = *src++;
      *dst++ = format_byte_segment(byte >> 4);
      *dst++ = format_byte_segment(byte & 0x0F);
    }
    for (std::size_t i = 1; i < size; i++) {
      const auto byte = *src++;
      *dst++ = i % 16 == 0 ? L'\n' : L'\x2004';
      *dst++ = format_byte_segment(byte >> 4);
      *dst++ = format_byte_segment(byte & 0x0F);
    }
  }

  void visualize(const void* data, std::size_t size, std::span<style> styles)
  {
    const auto start = string_.size();
    visualize(data, size);
    const auto end = string_.size();
    for (const auto& style : styles) {
      const auto offset = style.start * 3;
      if (offset >= end) {
        continue;
      }
      auto length = style.size * 3;
      if (offset + length >= end) {
        length = end - offset;
      }
      styles_.emplace_back(start + offset, length, style.effect);
    }
  }

  void clear() noexcept
  {
    string_.clear();
    styles_.clear();
  }

  void create(auto& factory, ComPtr<IDWriteTextFormat>& format, FLOAT cx, FLOAT cy, IDWriteTextLayout** layout)
  {
    if (!string_.empty()) {
      const auto data = string_.data();
      const auto size = static_cast<UINT32>(string_.size());
      if (SUCCEEDED(factory->CreateTextLayout(data, size, format.Get(), cx, cy, layout))) {
        DWRITE_TEXT_RANGE range;
        for (auto& style : styles_) {
          range.startPosition = static_cast<UINT32>(style.start);
          range.length = static_cast<UINT32>(style.size);
          (*layout)->SetDrawingEffect(style.effect, range);
        }
      }
    }
  }

private:
  static constexpr wchar_t format_byte_segment(BYTE byte) noexcept
  {
    return (byte < 0x0A ? L'0' : L'A' - 0x0A) + byte;
  }

  std::wstring string_;
  std::vector<style> styles_;
};

}  // namespace epos