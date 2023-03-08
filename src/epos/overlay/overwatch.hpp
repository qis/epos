#pragma once
#include <epos/overlay.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <deus.hpp>
#include <dinput.h>
#include <dinputd.h>
#include <dwrite_3.h>
#include <dxgi1_2.h>
#include <array>
#include <atomic>
#include <chrono>
#include <string>
#include <string_view>
#include <thread>

namespace epos {

class overwatch : public overlay {
public:
  // Chrono types.
  using clock = std::chrono::high_resolution_clock;
  using milliseconds = std::chrono::duration<double, std::chrono::milliseconds::period>;

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
    static constexpr D2D1_RECT_F status{ 0, 0, sx, sh };
    static constexpr D2D1_RECT_F screen{ sx, sy, sx + sw, sy + sh };
    static constexpr D2D1_RECT_F report{ sx + sw, 0, dw, sh };

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
        region::report.bottom - margin - 58,
        region::report.right - margin,
        region::report.bottom,
      };
    };
  };

  enum class brush : unsigned {
    red = 0,
    orange,
    yellow,
    green,
    blue,
    black,
    white,
    gray,
    none,
  };

  enum class format : unsigned {
    label = 0,
    debug,
    status,
    report,
    none,
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

  __forceinline ID2D1SolidColorBrush* get(brush brush) noexcept
  {
    assert(static_cast<unsigned>(brush) < static_cast<unsigned>(brush::none));
    return brush_[static_cast<unsigned>(brush)].Get();
  }

  __forceinline IDWriteTextFormat* get(format format) noexcept
  {
    assert(static_cast<unsigned>(format) < static_cast<unsigned>(format::none));
    return format_[static_cast<unsigned>(format)].Get();
  }

  __forceinline void draw(
    auto& dc,
    std::wstring_view text,
    const D2D1_RECT_F& rect,
    IDWriteTextFormat* format,
    ID2D1Brush* brush) noexcept
  {
    if (const auto size = static_cast<UINT32>(text.size())) {
      dc->DrawText(text.data(), size, format, rect, brush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }
  }

  ComPtr<IDirectInput8> input_;
  ComPtr<IDirectInputDevice8> keybd_;
  ComPtr<IDirectInputDevice8> mouse_;

  DIMOUSESTATE2 mouse_state_{};
  D2D1_POINT_2F cursor_position_{};

  ComPtr<ID2D1Bitmap> oueline_;
  ComPtr<ID2D1BitmapRenderTarget> oueline_dc_;
  ComPtr<ID2D1SolidColorBrush> oueline_brush_;
  ComPtr<ID2D1Effect> oueline_dilate_;

  ComPtr<IDWriteFactory6> factory_;
  ComPtr<IDWriteFontCollection2> fonts_;

  std::array<ComPtr<ID2D1SolidColorBrush>, static_cast<unsigned>(brush::none)> brush_;
  std::array<ComPtr<IDWriteTextFormat>, static_cast<unsigned>(format::none)> format_;

  clock::duration duration_{};
  std::wstring duration_text_;

  struct text {
    std::wstring string;
    brush brush{ brush::white };
  };

  struct label {
    std::wstring text;
    D2D1_RECT_F rect{};
    format format{ format::label };
    brush brush{ brush::white };
  };

  struct scene {
    clock::duration duration;
    std::vector<label> labels;
    std::vector<text> status;
    std::vector<text> report;
  };

  std::array<scene, 3> scenes_{};
  scene* scene_work_{ &scenes_[0] };
  scene* scene_draw_{ &scenes_[1] };
  std::atomic<scene*> scene_done_{ &scenes_[2] };
  std::atomic_bool scene_done_updated_{ false };

  deus::device device_;

  std::atomic_bool stop_{ false };
  boost::asio::io_context context_{ 1 };
  std::jthread thread_;
};

}  // namespace epos