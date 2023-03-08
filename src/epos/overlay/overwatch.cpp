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
  HR(dc_->CreateCompatibleRenderTarget(&outline_dc_));
  HR(outline_dc_->CreateSolidColorBrush(D2D1::ColorF(0x000000, 1.0f), &outline_brush_));
  HR(outline_dc_->GetBitmap(&outline_));

  HR(dc_->CreateEffect(CLSID_D2D1Morphology, &outline_dilate_));
  HR(outline_dilate_->SetValue(D2D1_MORPHOLOGY_PROP_MODE, D2D1_MORPHOLOGY_MODE_DILATE));
  HR(outline_dilate_->SetValue(D2D1_MORPHOLOGY_PROP_HEIGHT, 3));
  HR(outline_dilate_->SetValue(D2D1_MORPHOLOGY_PROP_WIDTH, 3));
  outline_dilate_->SetInput(0, outline_.Get());

  HR(dc_->CreateEffect(CLSID_D2D1ColorMatrix, &outline_color_));
  constexpr D2D1_MATRIX_5X4_F matrix{ .m{
    { 0.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f },
  } };
  HR(outline_color_->SetValue(D2D1_COLORMATRIX_PROP_COLOR_MATRIX, matrix));
  outline_color_->SetInputEffect(0, outline_dilate_.Get());

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
  const auto create_brush = [this](auto& dc, UINT32 color, FLOAT alpha, ID2D1SolidColorBrush** brush) {
    HR(dc->CreateSolidColorBrush(D2D1::ColorF(color, alpha), brush));
  };

  create_brush(dc_, 0xF44336, 1.0f, &brushes_.red);     // A500 Red
  create_brush(dc_, 0xFFA726, 1.0f, &brushes_.orange);  // A400 Orange
  create_brush(dc_, 0xFFF59D, 1.0f, &brushes_.yellow);  // A200 Yellow
  create_brush(dc_, 0x8BC34A, 1.0f, &brushes_.green);   // A500 Light Green
  create_brush(dc_, 0x29B6F6, 1.0f, &brushes_.blue);    // A400 Light Blue
  create_brush(dc_, 0x000000, 1.0f, &brushes_.black);   // Black
  create_brush(dc_, 0xFFFFFF, 1.0f, &brushes_.white);   // White
  create_brush(dc_, 0xF0F0F0, 0.6f, &brushes_.gray);    // Gray

  // Create DirectWrite fonts.
  const auto create_font = [this](LPCWSTR name, FLOAT size, BOOL bold, IDWriteTextFormat** format) {
    constexpr auto style = DWRITE_FONT_STYLE_NORMAL;
    constexpr auto stretch = DWRITE_FONT_STRETCH_NORMAL;
    const auto weight = bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL;
    HR(factory_->CreateTextFormat(name, fonts_.Get(), weight, style, stretch, size, L"en-US", format));
  };

  create_font(L"Roboto", 14.0f, FALSE, &formats_.label);
  create_font(L"Roboto Mono", 10.0f, FALSE, &formats_.debug);
  create_font(L"Roboto Mono", 12.0f, FALSE, &formats_.status);
  create_font(L"Roboto Mono", 12.0f, FALSE, &formats_.report);
  formats_.label->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
  formats_.status->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);

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
    outline_dc_->BeginDraw();
    outline_dc_->Clear();
    for (auto& label : scene_draw_->labels) {
      draw(outline_dc_, { label.x, label.y }, label.layout, brushes_.black);
    }
    outline_dc_->EndDraw();
    dc_->DrawImage(outline_color_.Get());
    for (auto& label : scene_draw_->labels) {
      draw(dc_, { label.x, label.y }, label.layout, brushes_.white);
    }
  }

  // Draw status.
  if (scene_draw_->status) {
    constexpr D2D1_POINT_2F origin{ region::text::status.left, region::text::status.top };
    draw(dc_, origin, scene_draw_->status, brushes_.white);
  }

  // Draw report.
  if (scene_draw_->report) {
    constexpr D2D1_POINT_2F origin{ region::text::report.left, region::text::report.top };
    draw(dc_, origin, scene_draw_->report, brushes_.white);
  }

  // Draw duration text.
  duration_text_.clear();
  const auto draw_ms = duration_cast<milliseconds>(duration_).count();
  const auto scan_ms = duration_cast<milliseconds>(scene_draw_->duration).count();
  std::format_to(std::back_inserter(duration_text_), L"{:.03f} ms scan\n{:.03f} ms draw", scan_ms, draw_ms);
  draw(dc_, duration_text_, region::text::duration, formats_.report, brushes_.gray);

  // Update draw duration.
  duration_ = clock::now() - tp0;
}

boost::asio::awaitable<void> overwatch::run() noexcept
{
  using namespace std::chrono_literals;
  timer timer{ co_await boost::asio::this_coro::executor };

  DIMOUSESTATE2 mouse_state{};
  D2D1_POINT_2F mouse_position{};

  POINT point{};
  if (GetCursorPos(&point)) {
    mouse_->GetDeviceState(sizeof(mouse_state), &mouse_state);
    mouse_position.x = point.x;
    mouse_position.y = point.y;
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

    // Update mouse position.
    if (SUCCEEDED(mouse_->GetDeviceState(sizeof(mouse_state), &mouse_state))) {
      mouse_position.x += mouse_state.lX / 4.0f;
      mouse_position.y += mouse_state.lY / 4.0f;
    } else {
      mouse_->Acquire();
    }

    // Add labels.
    string_.clear();
    string_.format(brushes_.blue, L"{:.01f}", mouse_position.x);
    string_.append(L':');
    string_.format(brushes_.green, L"{:.01f}", mouse_position.y);
    auto& mouse = scene_work_->labels.emplace_back(mouse_position.x + 16, mouse_position.y + 16);
    string_.create(factory_, formats_.debug, 256, 32, &mouse.layout);

    // Write status.
    status_.format(L"{}", counter++);

    // Write report.
    report_.format(L"{}", counter++);

    // Create status.
    if (status_) {
      constexpr auto cx = region::text::status.right - region::text::status.left;
      constexpr auto cy = region::text::status.bottom - region::text::status.top;
      status_.create(factory_, formats_.status, cx, cy, &scene_work_->status);
    }

    // Create report.
    if (report_) {
      constexpr auto cx = region::text::report.right - region::text::report.left;
      constexpr auto cy = region::text::report.bottom - region::text::report.top;
      report_.create(factory_, formats_.report, cx, cy, &scene_work_->report);
    }

    // Update scan duration.
    scene_work_->duration = clock::now() - tp0;

    // Swap work and done scenes.
    scene_work_ = scene_done_.exchange(scene_work_);

    // Indicate that the done scene was updated.
    scene_done_updated_.store(true, std::memory_order_release);

    // Signal overlay to call render() on another thread.
    update();

    // Clear scene.
    scene_work_->labels.clear();
    scene_work_->status.Reset();
    scene_work_->report.Reset();
    status_.clear();
    report_.clear();

    // Limit frame rate.
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  co_return;
}

}  // namespace epos