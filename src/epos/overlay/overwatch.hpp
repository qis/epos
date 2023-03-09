#pragma once
#include <epos/chrono.hpp>
#include <epos/com.hpp>
#include <epos/overlay.hpp>
#include <epos/text.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <deus.hpp>
#include <dinput.h>
#include <dinputd.h>
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
      static constexpr auto margin = 8;
      static constexpr D2D1_RECT_F status{
        region::status.left + margin,
        region::status.top + margin,
        region::status.right - margin,
        region::status.bottom - margin,
      };
      static constexpr D2D1_RECT_F screen{
        region::screen.left + margin,
        region::screen.top + margin,
        region::screen.right - margin,
        region::screen.bottom - margin,
      };
      static constexpr D2D1_RECT_F report{
        region::report.left + margin,
        region::report.top + margin,
        region::report.right - margin,
        region::report.bottom - margin,
      };
      static constexpr D2D1_RECT_F duration{
        region::report.left + margin,
        region::report.bottom - margin - 28,
        region::report.right - margin,
        region::report.bottom,
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
  boost::asio::awaitable<void> run() noexcept;

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

  ComPtr<IDirectInput8> input_;
  ComPtr<IDirectInputDevice8> keybd_;
  ComPtr<IDirectInputDevice8> mouse_;

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
  std::wstring duration_text_;

  struct label {
    float x{ 0.0f };
    float y{ 0.0f };
    ComPtr<IDWriteTextLayout> layout;
  };

  struct scene {
    std::vector<label> labels;
    ComPtr<IDWriteTextLayout> status;
    ComPtr<IDWriteTextLayout> report;
    clock::duration duration{};
  };

  std::array<scene, 3> scenes_{};
  scene* scene_work_{ &scenes_[0] };
  scene* scene_draw_{ &scenes_[1] };
  std::atomic<scene*> scene_done_{ &scenes_[2] };
  std::atomic_bool scene_done_updated_{ false };

  text string_;
  text status_;
  text report_;

  deus::device device_;

  std::atomic_bool stop_{ false };
  boost::asio::io_context context_{ 1 };
  std::jthread thread_;
};

}  // namespace epos