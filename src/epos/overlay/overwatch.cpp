#include "overwatch.hpp"
#include <epos/error.hpp>
#include <epos/fonts.hpp>
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

namespace epos {
namespace {

using token = decltype(boost::asio::as_tuple(boost::asio::use_awaitable));
using timer = decltype(token::as_default_on(boost::asio::steady_timer({})));

}  // namespace

overwatch::overwatch(HINSTANCE instance, HWND hwnd, long cx, long cy) :
  overlay(instance, hwnd, cx, cy)
{
  // Verify window size.
  if (cx != dw || cy != dh) {
    throw std::runtime_error("Invalid display size.");
  }

  // Create DirectInput objects.
  HR(DirectInput8Create(instance, DIRECTINPUT_VERSION, IID_IDirectInput8, &input_, nullptr));

  HR(input_->CreateDevice(GUID_SysKeyboard, &keybd_, nullptr));
  HR(keybd_->SetDataFormat(&c_dfDIKeyboard));
  HR(keybd_->SetCooperativeLevel(hwnd, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE));
  HR(keybd_->Acquire());

  HR(input_->CreateDevice(GUID_SysMouse, &mouse_, nullptr));
  HR(mouse_->SetDataFormat(&c_dfDIMouse2));
  HR(mouse_->SetCooperativeLevel(hwnd, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE));

  DIPROPDWORD dipdw;
  dipdw.diph.dwSize = sizeof(DIPROPDWORD);
  dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
  dipdw.diph.dwObj = 0;
  dipdw.diph.dwHow = DIPH_DEVICE;
  dipdw.dwData = 0;  // buffer size
  HR(mouse_->SetProperty(DIPROP_BUFFERSIZE, &dipdw.diph));
  HR(mouse_->Acquire());

  // Create DirectWrite objects.
  HR(dc_->CreateCompatibleRenderTarget(&oueline_dc_));
  HR(oueline_dc_->CreateSolidColorBrush(D2D1::ColorF(0x000000, 1.0f), &oueline_brush_));
  HR(oueline_dc_->GetBitmap(&oueline_));

  HR(dc_->CreateEffect(CLSID_D2D1Morphology, &oueline_dilate_));
  HR(oueline_dilate_->SetValue(D2D1_MORPHOLOGY_PROP_MODE, D2D1_MORPHOLOGY_MODE_DILATE));
  HR(oueline_dilate_->SetValue(D2D1_MORPHOLOGY_PROP_HEIGHT, 3));
  HR(oueline_dilate_->SetValue(D2D1_MORPHOLOGY_PROP_WIDTH, 3));
  oueline_dilate_->SetInput(0, oueline_.Get());

  HR(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(factory_), &factory_));

  ComPtr<IDWriteInMemoryFontFileLoader> loader;
  HR(factory_->CreateInMemoryFontFileLoader(&loader));
  HR(factory_->RegisterFontFileLoader(loader.Get()));

  ComPtr<IDWriteFontSetBuilder2> builder;
  HR(factory_->CreateFontSetBuilder(&builder));

  for (const DWORD id : { EPOS_FONT_ROBOTO, EPOS_FONT_ROBOTO_BLACK, EPOS_FONT_ROBOTO_MONO }) {
    ComPtr<IDWriteFontFile> file;
    ComPtr<IFontResource> font;
    HR(CreateFontResource(instance, MAKEINTRESOURCE(id), RT_RCDATA, &font));
    HR(loader->CreateInMemoryFontFileReference(
      factory_.Get(),
      font->Data(),
      font->Size(),
      font.Get(),
      &file));
    HR(builder->AddFontFile(file.Get()));
  }

  ComPtr<IDWriteFontSet> set;
  HR(builder->CreateFontSet(&set));
  HR(factory_->CreateFontCollectionFromFontSet(set.Get(), DWRITE_FONT_FAMILY_MODEL_TYPOGRAPHIC, &fonts_));

  // Create DirectWrite brushes.
  const auto create_brush = [this](auto& dc, UINT32 color, FLOAT alpha, brush brush) {
    auto& object = brush_[static_cast<unsigned>(brush)];
    HR(dc->CreateSolidColorBrush(D2D1::ColorF(color, alpha), &object));
  };

  create_brush(dc_, 0xF44336, 1.0f, brush::red);     // A500 Red
  create_brush(dc_, 0xFFA726, 1.0f, brush::orange);  // A400 Orange
  create_brush(dc_, 0xFFF59D, 1.0f, brush::yellow);  // A200 Yellow
  create_brush(dc_, 0x8BC34A, 1.0f, brush::green);   // A500 Light Green
  create_brush(dc_, 0x29B6F6, 1.0f, brush::blue);    // A400 Light Blue
  create_brush(dc_, 0x000000, 1.0f, brush::black);   // Black
  create_brush(dc_, 0xFFFFFF, 1.0f, brush::white);   // White
  create_brush(dc_, 0xF0F0F0, 0.6f, brush::gray);    // Gray

  // Create DirectWrite fonts.
  const auto create_font = [this](LPCWSTR name, FLOAT size, BOOL bold, format format) {
    constexpr auto style = DWRITE_FONT_STYLE_NORMAL;
    constexpr auto stretch = DWRITE_FONT_STRETCH_NORMAL;
    const auto weight = bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL;
    auto& object = format_[static_cast<unsigned>(format)];
    HR(factory_->CreateTextFormat(name, fonts_.Get(), weight, style, stretch, size, L"en-US", &object));
  };

  create_font(L"Roboto", 14.0f, FALSE, format::label);
  create_font(L"Roboto Mono", 10.0f, FALSE, format::debug);
  create_font(L"Roboto Mono", 12.0f, FALSE, format::status);
  create_font(L"Roboto Mono", 12.0f, FALSE, format::report);
  get(format::label)->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
  get(format::status)->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);

  // Create device.
  if (const auto rv = device_.create(); !rv) {
    throw std::system_error(rv.error(), "create");
  }

  // Start thread.
  boost::asio::co_spawn(context_, run(), boost::asio::detached);
  thread_ = std::jthread([this]() noexcept {
    boost::system::error_code ec;
    context_.run(ec);
  });
}

overwatch::~overwatch()
{
  // Stop thread.
  stop_.store(true, std::memory_order_release);
  context_.stop();
}

void overwatch::render() noexcept
{
  // Measure draw duration.
  const auto tp0 = clock::now();

  // Swap draw and done scenes if the done scene was updated.
  auto scene_done_updated_expected = true;
  if (scene_done_updated_.compare_exchange_weak(scene_done_updated_expected, false)) {
    scene_draw_ = scene_done_.exchange(scene_draw_);
  }

  // Clear scene.
  dc_->Clear();
  dc_->SetTransform(D2D1::IdentityMatrix());

  // Draw labels.
  if (!scene_draw_->labels.empty()) {
    // Draw text to oueline bitmap.
    //oueline_dc_->BeginDraw();
    //oueline_dc_->Clear();
    //for (const auto& label : scene_draw_->labels) {
    //  draw(oueline_dc_, label.text, label.rect, get(label.format), oueline_brush_.Get());
    //}
    //oueline_dc_->EndDraw();

    // Draw oueline bitmap.
    //dc_->DrawImage(oueline_dilate_.Get());

    // Draw labels.
    for (const auto& label : scene_draw_->labels) {
      //draw(dc_, label.text, label.rect, get(label.format), get(label.brush));
      const auto data = label.text.data();
      const auto size = static_cast<UINT32>(label.text.size());
      const auto cx = label.rect.right - label.rect.left;
      const auto cy = label.rect.bottom - label.rect.top;
      ComPtr<IDWriteTextLayout> layout;
      if (SUCCEEDED(factory_->CreateTextLayout(data, size, get(label.format), cx, cy, &layout))) {
        layout->SetDrawingEffect(get(brush::red), {1, 3});
        dc_->DrawTextLayout({ label.rect.left, label.rect.top }, layout.Get(), get(label.brush), D2D1_DRAW_TEXT_OPTIONS_NONE);
      }
    }
  }

  // Draw duration text.
  duration_text_.clear();
  const auto draw_ms = std::chrono::duration_cast<milliseconds>(duration_).count();
  const auto scan_ms = std::chrono::duration_cast<milliseconds>(scene_draw_->duration).count();
  std::format_to(std::back_inserter(duration_text_), L"{:.03f} ms scan\n{:.03f} ms draw", scan_ms, draw_ms);
  draw(dc_, duration_text_, region::text::duration, get(format::report), get(brush::gray));

  // Draw status and report.
  //draw(dc_, scene_draw_->status, region::text::status, get(format::status), get(brush::blue));
  //draw(dc_, scene_draw_->report, region::text::report, get(format::report), get(brush::green));

  // Update draw duration.
  duration_ = clock::now() - tp0;
}

boost::asio::awaitable<void> overwatch::run() noexcept
{
  using namespace std::chrono_literals;
  timer timer{ co_await boost::asio::this_coro::executor };

  POINT point{};
  if (GetCursorPos(&point)) {
    mouse_->GetDeviceState(sizeof(mouse_state_), &mouse_state_);
    cursor_position_.x = point.x;
    cursor_position_.y = point.y;
  }

  size_t counter = 0;
  while (!stop_.load(std::memory_order_relaxed)) {
    // Measure scan duration.
    const auto tp0 = clock::now();

    // TODO: Scan memory.
    //if (duration < 7ms) {
    //  timer.expires_from_now(7ms - duration);
    //  if (const auto [ec] = co_await timer.async_wait(); ec) {
    //    co_return;
    //  }
    //}

    // Write status.
    scene_work_->status.clear();
    scene_work_->status.emplace_back(std::format(L"{}", counter++), brush::red);

    // Write labels.
    scene_work_->labels.clear();
    if (SUCCEEDED(mouse_->GetDeviceState(sizeof(mouse_state_), &mouse_state_))) {
      cursor_position_.x += mouse_state_.lX / 4.0f;
      cursor_position_.y += mouse_state_.lY / 4.0f;
    } else {
      mouse_->Acquire();
    }
    scene_work_->labels.emplace_back(
      std::format(L"{:.01f}:{:.01f}", cursor_position_.x, cursor_position_.y),
      D2D1::RectF(cursor_position_.x + 16, cursor_position_.y + 16, dw, dh),
      format::debug);

    // Write report.
    scene_work_->report.clear();
    scene_work_->report.emplace_back(std::format(L"{}", counter++), brush::blue);

    // Update duration.
    scene_work_->duration = clock::now() - tp0;

    // Swap work and done scenes.
    scene_work_ = scene_done_.exchange(scene_work_);

    // Indicate that the done scene was updated.
    scene_done_updated_.store(true, std::memory_order_release);

    // Signal overlay to call render() on another thread.
    update();

    // Limit frame rate.
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  co_return;
}

}  // namespace epos