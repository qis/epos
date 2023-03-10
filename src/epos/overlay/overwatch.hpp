#pragma once
#include <epos/clock.hpp>
#include <epos/input.hpp>
#include <epos/overlay.hpp>
#include <epos/runtime.hpp>
#include <epos/text.hpp>
#include <epos/timer.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <deus.hpp>
#include <dxgi1_2.h>
#include <array>
#include <atomic>
#include <thread>

namespace epos {

class overwatch : public overlay {
public:
  // Display width and height.
  static constexpr auto dw = 2560;
  static constexpr auto dh = 1080;

  // Screen width and height.
  static constexpr auto sw = 1920;
  static constexpr auto sh = dh;

  // Screen offsets.
  static constexpr auto sx = (dw - sw) / 2;
  static constexpr auto sy = 0;

  // Scene regions.
  struct region {
    static constexpr D2D1_RECT_F status{ 0, 0, sx, sh - 30 };
    static constexpr D2D1_RECT_F screen{ sx, sy, sx + sw, sy + sh };
    static constexpr D2D1_RECT_F report{ sx + sw, 0, dw, sh - 30 };

    // Text regions.
    struct text {
      static constexpr auto mx = 10;
      static constexpr auto my = 5;
      static constexpr D2D1_RECT_F status{
        region::status.left,
        region::status.top + my,
        region::status.right - mx,
        region::status.bottom - my,
      };
      static constexpr D2D1_RECT_F report{
        region::report.left + mx,
        region::report.top + my,
        region::report.right,
        region::report.bottom - my,
      };
      static constexpr D2D1_RECT_F duration{
        region::report.left,
        region::report.bottom - my - 32,
        region::report.right - mx,
        region::report.bottom - my,
      };
    };
  };

  overwatch(HINSTANCE instance, HWND hwnd, long cx, long cy);

  overwatch(overwatch&& other) = delete;
  overwatch(const overwatch& other) = delete;
  overwatch& operator=(overwatch&& other) = delete;
  overwatch& operator=(const overwatch& other) = delete;

  ~overwatch() override;

  void render() noexcept override;

private:
  boost::asio::awaitable<void> update(
    std::chrono::steady_clock::duration wait = {},
    bool reset = false) noexcept;

  boost::asio::awaitable<void> run() noexcept;

  boost::asio::awaitable<bool> on_open() noexcept;
  boost::asio::awaitable<bool> on_process() noexcept;
  boost::asio::awaitable<void> on_close() noexcept;

  static __forceinline void draw(
    auto& dc,
    std::wstring_view text,
    const D2D1_RECT_F& rect,
    ComPtr<IDWriteTextFormat>& format,
    ComPtr<ID2D1SolidColorBrush>& brush) noexcept
  {
    if (const auto size = static_cast<UINT32>(text.size())) {
      dc->DrawText(text.data(), size, format.Get(), rect, brush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }
  }

  static __forceinline void draw(
    auto& dc,
    const D2D1_POINT_2F& origin,
    ComPtr<IDWriteTextLayout>& layout,
    ComPtr<ID2D1SolidColorBrush>& brush) noexcept
  {
    if (layout) {
      dc->DrawTextLayout(origin, layout.Get(), brush.Get(), D2D1_DRAW_TEXT_OPTIONS_NONE);
    }
  }

  input input_;

  ComPtr<ID2D1Bitmap> outline_;
  ComPtr<ID2D1BitmapRenderTarget> outline_dc_;
  ComPtr<ID2D1SolidColorBrush> outline_brush_;
  ComPtr<ID2D1Effect> outline_dilate_;

  ComPtr<IDWriteFactory6> factory_;
  ComPtr<IDWriteFontCollection2> fonts_;

  struct brushes {
    ComPtr<ID2D1SolidColorBrush> red;
    ComPtr<ID2D1SolidColorBrush> orange;
    ComPtr<ID2D1SolidColorBrush> yellow;
    ComPtr<ID2D1SolidColorBrush> green;
    ComPtr<ID2D1SolidColorBrush> blue;
    ComPtr<ID2D1SolidColorBrush> black;
    ComPtr<ID2D1SolidColorBrush> white;
    ComPtr<ID2D1SolidColorBrush> gray;
    ComPtr<ID2D1SolidColorBrush> info;
    ComPtr<ID2D1LinearGradientBrush> status;
    ComPtr<ID2D1LinearGradientBrush> report;
  } brushes_;

  struct formats {
    ComPtr<IDWriteTextFormat> label;
    ComPtr<IDWriteTextFormat> debug;
    ComPtr<IDWriteTextFormat> status;
    ComPtr<IDWriteTextFormat> report;
  } formats_;

  clock::duration duration_{};
  std::wstring info_;

  struct label {
    float x{ 0.0f };
    float y{ 0.0f };
    ComPtr<IDWriteTextLayout> layout;
  };

  struct scene {
    ComPtr<IDWriteTextLayout> status;
    ComPtr<IDWriteTextLayout> report;
    std::vector<label> labels;
    clock::duration duration{};
  };

  std::array<scene, 3> scenes_{};
  scene* scene_work_{ &scenes_[0] };
  scene* scene_draw_{ &scenes_[1] };
  std::atomic<scene*> scene_done_{ &scenes_[2] };
  std::atomic_bool scene_done_updated_{ false };

  clock::time_point update_time_point_{ clock::now() };

  text string_;
  text status_;
  text report_;

  deus::device device_;

  std::atomic_bool stop_{ false };
  boost::asio::io_context context_{ 1 };
  timer timer_{ context_ };
  std::jthread thread_;
};

}  // namespace epos