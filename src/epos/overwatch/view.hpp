#pragma once
#include "game.hpp"
#include <epos/clock.hpp>
#include <epos/input.hpp>
#include <epos/overlay.hpp>
#include <epos/runtime.hpp>
#include <epos/text.hpp>
#include <epos/timer.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/circular_buffer.hpp>
#include <deus.hpp>
#include <dxgi1_2.h>
#include <array>
#include <atomic>
#include <thread>

namespace epos::overwatch {

using namespace DirectX;

class view : public overlay {
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

  // Sceen center.
  static constexpr D2D1_POINT_2F sc{ sw / 2.0f, sh / 2.0f };

  // Weapon spread.
  static const D2D1_ELLIPSE spread;

  // Trigger distance in meters.
  static constexpr auto trigger = 12.0f;

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

  view(HINSTANCE instance, HWND hwnd, long cx, long cy);

  view(view&& other) = delete;
  view(const view& other) = delete;
  view& operator=(view&& other) = delete;
  view& operator=(const view& other) = delete;

  ~view() override;

  command render() noexcept override;
  void presented() noexcept override;

private:
  boost::asio::awaitable<void> update(std::chrono::steady_clock::duration wait) noexcept;
  boost::asio::awaitable<void> run() noexcept;

  static __forceinline void draw(
    auto& dc,
    std::wstring_view text,
    const D2D1_RECT_F& rect,
    const ComPtr<IDWriteTextFormat>& format,
    const ComPtr<ID2D1SolidColorBrush>& brush) noexcept
  {
    if (const auto size = static_cast<UINT32>(text.size())) {
      dc->DrawText(text.data(), size, format.Get(), rect, brush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }
  }

  static __forceinline void draw(
    auto& dc,
    const D2D1_POINT_2F& origin,
    const ComPtr<IDWriteTextLayout>& layout,
    const ComPtr<ID2D1SolidColorBrush>& brush) noexcept
  {
    if (layout) {
      dc->DrawTextLayout(origin, layout.Get(), brush.Get(), D2D1_DRAW_TEXT_OPTIONS_NONE);
    }
  }

  input input_;
  boost::circular_buffer<XMFLOAT2> mouse_{ 3 };

  ComPtr<ID2D1Bitmap> outline_;
  ComPtr<ID2D1BitmapRenderTarget> outline_dc_;
  ComPtr<ID2D1SolidColorBrush> outline_brush_;
  ComPtr<ID2D1Effect> outline_dilate_;

  ComPtr<IDWriteFactory6> factory_;
  ComPtr<IDWriteFontCollection2> fonts_;

  struct brushes {
    ComPtr<ID2D1SolidColorBrush> black;
    ComPtr<ID2D1SolidColorBrush> white;
    ComPtr<ID2D1SolidColorBrush> red;
    ComPtr<ID2D1SolidColorBrush> orange;
    ComPtr<ID2D1SolidColorBrush> yellow;
    ComPtr<ID2D1SolidColorBrush> green;
    ComPtr<ID2D1SolidColorBrush> blue;
    ComPtr<ID2D1SolidColorBrush> gray;
    ComPtr<ID2D1SolidColorBrush> info;
    ComPtr<ID2D1SolidColorBrush> spread;
    ComPtr<ID2D1SolidColorBrush> enemy;
    ComPtr<ID2D1LinearGradientBrush> status;
    ComPtr<ID2D1LinearGradientBrush> report;
  } brushes_;

  struct formats {
    ComPtr<IDWriteTextFormat> label;
    ComPtr<IDWriteTextFormat> debug;
    ComPtr<IDWriteTextFormat> status;
    ComPtr<IDWriteTextFormat> report;
  } formats_;

  struct label {
    FLOAT x{};
    FLOAT y{};
    ComPtr<IDWriteTextLayout> layout;
    ComPtr<ID2D1SolidColorBrush> brush;
  };

  std::wstring info_;

  struct scene {
    ComPtr<IDWriteTextLayout> report;
    std::size_t entities{ 0 };
    bool vm{ false };
  };

  std::array<scene, 3> scenes_{};
  scene* scene_work_{ &scenes_[0] };
  scene* scene_draw_{ &scenes_[1] };
  std::vector<label> scene_labels_{};
  std::atomic<scene*> scene_done_{ &scenes_[2] };
  std::atomic_bool scene_done_updated_{ false };

  text status_;
  text report_;
  text string_;

  label& string_label(
    FLOAT x,
    FLOAT y,
    FLOAT w,
    FLOAT h,
    const ComPtr<IDWriteTextFormat>& format,
    const ComPtr<ID2D1SolidColorBrush>& brush)
  {
    auto& label = scene_labels_.emplace_back(x, y, ComPtr<IDWriteTextLayout>{}, brush);
    string_.create(factory_, format, w, h, &label.layout);
    DWRITE_TEXT_METRICS tm{};
    if (SUCCEEDED(label.layout->GetMetrics(&tm))) {
      label.x -= tm.width / 2;
      label.y -= tm.height / 2;
    }
    return label;
  }

  clock::time_point draw_{ clock::now() };
  clock::time_point swap_{ clock::now() };
  clock::duration swap_duration_{};

  deus::device device_;
  std::vector<std::byte> memory_{ game::entity_region_size };
  std::array<game::entity, game::entities> entities_;
  XMMATRIX vm_{};

  struct snapshot {
    XMVECTOR target{};
    clock::time_point time_point;
  };

  std::array<boost::circular_buffer<snapshot>, game::entities> movement_;
  clock::time_point update_movement_{ clock::now() };

  std::atomic<game::team> team_{ game::team::one };
  clock::time_point lockout_{ clock::now() };

  std::atomic_bool stop_{ false };
  boost::asio::io_context context_{ 1 };
  timer timer_{ context_ };
  std::jthread thread_;
};

}  // namespace epos::overwatch