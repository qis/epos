#pragma once
#include <epos/overlay.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <deus.hpp>
#include <dwrite_3.h>
#include <dxgi1_2.h>
#include <atomic>
#include <thread>

namespace epos {

class overwatch : public overlay {
public:
  overwatch(HINSTANCE instance, HWND hwnd, long cx, long cy);

  overwatch(overwatch&& other) = delete;
  overwatch(const overwatch& other) = delete;
  overwatch& operator=(overwatch&& other) = delete;
  overwatch& operator=(const overwatch& other) = delete;

  ~overwatch() override;

protected:
  void draw() noexcept override;

private:
  boost::asio::awaitable<void> run() noexcept;

  ComPtr<IDWriteFactory6> factory_;
  ComPtr<IDWriteFontCollection2> fonts_;

  struct color {
    ComPtr<ID2D1SolidColorBrush> red;
    ComPtr<ID2D1SolidColorBrush> green;
    ComPtr<ID2D1SolidColorBrush> blue;
    ComPtr<ID2D1SolidColorBrush> black;
    ComPtr<ID2D1SolidColorBrush> white;
  } color_;

  struct format {
    ComPtr<IDWriteTextFormat> regular;
    ComPtr<IDWriteTextFormat> numeric;
    ComPtr<IDWriteTextFormat> monospace;
  } format_;

  FLOAT cx_{ 0.0f };
  FLOAT cy_{ 0.0f };

  std::wstring status_;
  std::wstring report_;

  deus::device device_;

  std::atomic_bool stop_{ false };
  boost::asio::io_context context_{ 1 };
  std::jthread thread_;
};

}  // namespace epos