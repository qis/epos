#pragma once
#include <epos/com.hpp>
#include <dwrite_3.h>
#include <format>
#include <string>
#include <string_view>
#include <vector>

namespace epos {

class text {
public:
  struct style {
    DWRITE_TEXT_RANGE range{};
    IUnknown* effect{ nullptr };
  };

  operator bool() const noexcept
  {
    return !string_.empty();
  }

  void append(WCHAR c)
  {
    string_.push_back(c);
  }

  template <class T>
  void append(ComPtr<T>& effect, WCHAR c)
  {
    const DWRITE_TEXT_RANGE range{ static_cast<UINT32>(string_.size()), 1 };
    styles_.emplace_back(range, effect.Get());
    string_.push_back(c);
  }

  void append(std::wstring_view string)
  {
    string_.append(string);
  }

  template <class T>
  void append(ComPtr<T>& effect, std::wstring_view string)
  {
    const DWRITE_TEXT_RANGE range{
      static_cast<UINT32>(string_.size()),
      static_cast<UINT32>(string.size()),
    };
    styles_.emplace_back(range, effect.Get());
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
    DWRITE_TEXT_RANGE range{ static_cast<UINT32>(string_.size()), 0 };
    std::format_to(std::back_inserter(string_), format, std::forward<Args>(args)...);
    range.length = static_cast<UINT32>(string_.size()) - range.startPosition;
    styles_.emplace_back(range, effect.Get());
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
        for (auto& style : styles_) {
          (*layout)->SetDrawingEffect(style.effect, style.range);
        }
      }
    }
  }

private:
  std::wstring string_;
  std::vector<style> styles_;
};

}  // namespace epos