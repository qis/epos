#pragma once
#include <epos/runtime.hpp>
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

  __forceinline void reset() noexcept
  {
    string_.clear();
    styles_.clear();
  }

  __forceinline void reset(wchar_t c)
  {
    string_.assign(1, c);
    styles_.clear();
  }

  template <class T>
  __forceinline void reset(ComPtr<T>& effect, wchar_t c)
  {
    reset(c);
    styles_.emplace_back(0, 1, effect.Get());
  }

  __forceinline void reset(std::size_t count, wchar_t c)
  {
    string_.assign(count, c);
    styles_.clear();
  }

  template <class T>
  __forceinline void reset(ComPtr<T>& effect, std::size_t count, wchar_t c)
  {
    reset(count, c);
    styles_.emplace_back(0, count, effect.Get());
  }

  __forceinline void reset(std::string_view string)
  {
    reset(S2W(string));
  }

  __forceinline void reset(std::wstring_view string)
  {
    string_.assign(string);
    styles_.clear();
  }

  template <class T>
  __forceinline void reset(ComPtr<T>& effect, std::string_view string)
  {
    reset(effect, S2W(string));
  }

  template <class T>
  __forceinline void reset(ComPtr<T>& effect, std::wstring_view string)
  {
    reset(string);
    styles_.emplace_back(0, string_.size(), effect.Get());
  }

  template <class Arg, class... Args>
  void reset(std::format_string<Arg, Args...> format, Arg&& arg, Args&&... args)
  {
    reset(S2W(std::format(format, std::forward<Arg>(arg), std::forward<Args>(args)...)));
  }

  template <class Arg, class... Args>
  void reset(std::wformat_string<Arg, Args...> format, Arg&& arg, Args&&... args)
  {
    string_.clear();
    std::format_to(back_inserter(), format, std::forward<Arg>(arg), std::forward<Args>(args)...);
    styles_.clear();
  }

  template <class T, class Arg, class... Args>
  void reset(ComPtr<T>& effect, std::format_string<Arg, Args...> format, Arg&& arg, Args&&... args)
  {
    reset(effect, S2W(std::format(format, std::forward<Arg>(arg), std::forward<Args>(args)...)));
  }

  template <class T, class Arg, class... Args>
  void reset(ComPtr<T>& effect, std::wformat_string<Arg, Args...> format, Arg&& arg, Args&&... args)
  {
    reset(format, std::forward<Arg>(arg), std::forward<Args>(args)...);
    styles_.emplace_back(0, string_.size(), effect.Get());
  }

  __forceinline void write(wchar_t c)
  {
    string_.push_back(c);
  }

  template <class T>
  __forceinline void write(ComPtr<T>& effect, wchar_t c)
  {
    styles_.emplace_back(string_.size(), 1, effect.Get());
    write(c);
  }

  __forceinline void write(std::size_t count, wchar_t c)
  {
    string_.append(count, c);
  }

  template <class T>
  __forceinline void write(ComPtr<T>& effect, std::size_t count, wchar_t c)
  {
    styles_.emplace_back(string_.size(), count, effect.Get());
    write(count, c);
  }

  __forceinline void write(std::string_view string)
  {
    write(S2W(string));
  }

  __forceinline void write(std::wstring_view string)
  {
    string_.append(string);
  }

  template <class T>
  __forceinline void write(ComPtr<T>& effect, std::string_view string)
  {
    write(effect, S2W(string));
  }

  template <class T>
  __forceinline void write(ComPtr<T>& effect, std::wstring_view string)
  {
    styles_.emplace_back(string_.size(), string.size(), effect.Get());
    write(string);
  }

  template <class Arg, class... Args>
  void write(std::format_string<Arg, Args...> format, Arg&& arg, Args&&... args)
  {
    write(S2W(std::format(format, std::forward<Arg>(arg), std::forward<Args>(args)...)));
  }

  template <class Arg, class... Args>
  void write(std::wformat_string<Arg, Args...> format, Arg&& arg, Args&&... args)
  {
    std::format_to(back_inserter(), format, std::forward<Arg>(arg), std::forward<Args>(args)...);
  }

  template <class T, class Arg, class... Args>
  void write(ComPtr<T>& effect, std::format_string<Arg, Args...> format, Arg&& arg, Args&&... args)
  {
    write(effect, S2W(std::format(format, std::forward<Arg>(arg), std::forward<Args>(args)...)));
  }

  template <class T, class Arg, class... Args>
  void write(ComPtr<T>& effect, std::wformat_string<Arg, Args...> format, Arg&& arg, Args&&... args)
  {
    const auto start = string_.size();
    write(format, std::forward<Arg>(arg), std::forward<Args>(args)...);
    styles_.emplace_back(start, string_.size() - start, effect.Get());
  }

  void visualize(const void* data, std::size_t size)
  {
    string_.reserve(string_.size() + size * 3 + size / 16);
    auto dst = back_inserter();
    auto src = reinterpret_cast<const BYTE*>(data);
    if (!string_.empty() && string_.back() != L'\n') {
      *dst++ = L'\n';
    }
    if (size) {
      const auto byte = *src++;
      *dst++ = format_byte_segment(byte >> 4);
      *dst++ = format_byte_segment(byte & 0x0F);
    }
    for (std::size_t i = 1; i < size; i++) {
      const auto byte = *src++;
      *dst++ = i % 16 == 0 ? L'\n' : i % 4 == 0 ? L' ' : L'\x2004';
      *dst++ = format_byte_segment(byte >> 4);
      *dst++ = format_byte_segment(byte & 0x0F);
    }
    if (size) {
      *dst = L'\n';
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

  void create(
    auto& factory,
    const ComPtr<IDWriteTextFormat>& format,
    FLOAT cx,
    FLOAT cy,
    IDWriteTextLayout** layout) const
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

  std::string get() const
  {
    return W2S(string_);
  }

private:
  constexpr std::back_insert_iterator<std::wstring> back_inserter() noexcept
  {
    return std::back_inserter(string_);
  }

  static constexpr wchar_t format_byte_segment(BYTE byte) noexcept
  {
    return (byte < 0x0A ? L'0' : L'A' - 0x0A) + byte;
  }

  std::wstring string_;
  std::vector<style> styles_;
};

}  // namespace epos