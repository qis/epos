#include "overwatch.hpp"
#include <epos/error.hpp>
#include <epos/fonts.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

namespace epos {

overwatch::overwatch(HINSTANCE instance, HWND hwnd, long cx, long cy) :
  overlay(instance, hwnd, cx, cy), input_(instance, hwnd)
{
  // Verify window size.
  if (cx != dw || cy != dh) {
    throw std::runtime_error("Invalid display size.");
  }

  // Create DirectWrite objects.
  constexpr auto options = D2D1_COMPATIBLE_RENDER_TARGET_OPTIONS_NONE;
  const auto format = D2D1::PixelFormat(DXGI_FORMAT_A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED);
  HR(dc_->CreateCompatibleRenderTarget(nullptr, nullptr, &format, options, &outline_dc_));
  outline_dc_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

  HR(outline_dc_->CreateSolidColorBrush(D2D1::ColorF(0x000000, 1.0f), &outline_brush_));
  HR(outline_dc_->GetBitmap(&outline_));

  HR(dc_->CreateEffect(CLSID_D2D1Morphology, &outline_dilate_));
  HR(outline_dilate_->SetValue(D2D1_MORPHOLOGY_PROP_MODE, D2D1_MORPHOLOGY_MODE_DILATE));
  HR(outline_dilate_->SetValue(D2D1_MORPHOLOGY_PROP_HEIGHT, 3));
  HR(outline_dilate_->SetValue(D2D1_MORPHOLOGY_PROP_WIDTH, 3));
  outline_dilate_->SetInput(0, outline_.Get());

  HR(DWriteCreateFactory(DWRITE_FACTORY_TYPE_ISOLATED, __uuidof(factory_), &factory_));

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

  // Create DirectWrite brushes (uses material design colors).
  const auto create_brush = [this](auto& dc, UINT32 color, FLOAT alpha, ID2D1SolidColorBrush** brush) {
    HR(dc->CreateSolidColorBrush(D2D1::ColorF(color, alpha), brush));
  };

  create_brush(dc_, 0xE57373, 1.0f, &brushes_.red);     // A300 Red
  create_brush(dc_, 0xFFB74D, 1.0f, &brushes_.orange);  // A300 Orange
  create_brush(dc_, 0xFFF176, 1.0f, &brushes_.yellow);  // A300 Yellow
  create_brush(dc_, 0xAED581, 1.0f, &brushes_.green);   // A300 Light Green
  create_brush(dc_, 0x4FC3F7, 1.0f, &brushes_.blue);    // A300 Light Blue
  create_brush(dc_, 0x000000, 1.0f, &brushes_.black);   // Black
  create_brush(dc_, 0xFFFFFF, 1.0f, &brushes_.white);   // White
  create_brush(dc_, 0xA0A0A0, 1.0f, &brushes_.gray);    // Gray
  create_brush(dc_, 0xF0F0F0, 0.6f, &brushes_.info);    // Info

  D2D1_GRADIENT_STOP gradient[2];
  gradient[0].color = D2D1::ColorF(D2D1::ColorF::Black, 0.8f);
  gradient[0].position = 0.0f;
  gradient[1].color = D2D1::ColorF(D2D1::ColorF::Black, 0.0f);
  gradient[1].position = 1.0f;
  ComPtr<ID2D1GradientStopCollection> shade;
  HR(dc_->CreateGradientStopCollection(gradient, 2, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_MIRROR, &shade));

  HR(dc_->CreateLinearGradientBrush(
    D2D1::LinearGradientBrushProperties(
      D2D1::Point2F(region::status.left, region::status.top + 320),
      D2D1::Point2F(region::status.right, region::status.bottom - 64)),
    shade.Get(),
    &brushes_.status));

  HR(dc_->CreateLinearGradientBrush(
    D2D1::LinearGradientBrushProperties(
      D2D1::Point2F(region::report.right, region::report.top + 320),
      D2D1::Point2F(region::report.left, region::report.bottom - 64)),
    shade.Get(),
    &brushes_.report));

  // Create DirectWrite fonts.
  const auto create_font = [this](LPCWSTR name, FLOAT size, BOOL bold, IDWriteTextFormat** format) {
    constexpr auto style = DWRITE_FONT_STYLE_NORMAL;
    constexpr auto stretch = DWRITE_FONT_STRETCH_NORMAL;
    const auto weight = bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL;
    HR(factory_->CreateTextFormat(name, fonts_.Get(), weight, style, stretch, size, L"en-US", format));
  };

  create_font(L"Roboto", 14.0f, FALSE, &formats_.label);
  create_font(L"Roboto Mono", 11.0f, FALSE, &formats_.debug);
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

  // Handle scene updated notification.
  auto scene_done_updated_expected = true;
  if (scene_done_updated_.compare_exchange_weak(scene_done_updated_expected, false)) {
    // Swap draw and done scenes.
    scene_draw_ = scene_done_.exchange(scene_draw_);

    // Draw outlines.
    if (!scene_draw_->labels.empty()) {
      outline_dc_->BeginDraw();
      outline_dc_->Clear();
      for (auto& label : scene_draw_->labels) {
        draw(outline_dc_, { label.x, label.y }, label.layout, brushes_.black);
      }
      outline_dc_->EndDraw();
    }
  }

  // Clear scene.
  dc_->Clear();
  dc_->SetTransform(D2D1::IdentityMatrix());

  // Draw status.
  if (scene_draw_->status) {
    dc_->FillRectangle(region::status, brushes_.status.Get());
    constexpr D2D1_POINT_2F origin{ region::text::status.left, region::text::status.top };
    draw(dc_, origin, scene_draw_->status, brushes_.gray);
  }

  // Draw report.
  if (scene_draw_->report) {
    dc_->FillRectangle(region::report, brushes_.report.Get());
    constexpr D2D1_POINT_2F origin{ region::text::report.left, region::text::report.top };
    draw(dc_, origin, scene_draw_->report, brushes_.gray);
  }

  // Draw info.
  info_.clear();
  const auto draw_ms = duration_cast<milliseconds>(duration_).count();
  const auto scan_ms = duration_cast<milliseconds>(scene_draw_->duration).count();
  std::format_to(std::back_inserter(info_), L"{:.03f} ms scan\n{:.03f} ms draw", scan_ms, draw_ms);
  draw(dc_, info_, region::text::duration, formats_.status, brushes_.info);

  // Draw circles, rectangles, polygons and labels.
  // TODO

  if (!scene_draw_->labels.empty()) {
    dc_->DrawImage(outline_dilate_.Get(), D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
    for (auto& label : scene_draw_->labels) {
      draw(dc_, { label.x, label.y }, label.layout, brushes_.white);
    }
  }

  // Update draw duration.
  duration_ = clock::now() - tp0;
}

boost::asio::awaitable<void> overwatch::run() noexcept
{
  const auto executor = co_await boost::asio::this_coro::executor;
  //timer timer{ executor };

  DIMOUSESTATE2 mouse_state{};
  D2D1_POINT_2F mouse_position{};

  POINT point{};
  if (GetCursorPos(&point)) {
    mouse_position.x = point.x;
    mouse_position.y = point.y;
    co_await input_.get();
  }

  std::vector<BYTE> data;
  for (unsigned i = 0; i <= 300; i++) {
    data.push_back(static_cast<BYTE>(i));
  }
  std::vector<text::style> data_styles;
  data_styles.emplace_back(1, 3, brushes_.red.Get());
  data_styles.emplace_back(47, 4, brushes_.green.Get());
  data_styles.emplace_back(250, 7, brushes_.blue.Get());

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
    const auto state = co_await input_.get();
    mouse_position.x += state.mx / 4.0f;
    mouse_position.y += state.my / 4.0f;

    // Add circles, rectangles, polygons and labels.
    // TODO

    string_.clear();
    string_.format(brushes_.blue, L"{:.01f}", mouse_position.x);
    string_.append(L':');
    string_.format(brushes_.green, L"{:.01f}", mouse_position.y);
    auto& mouse = scene_work_->labels.emplace_back(mouse_position.x + 16, mouse_position.y + 16);
    string_.create(factory_, formats_.debug, 256, 32, &mouse.layout);

    // Write status.
    status_.format(brushes_.white, L"{}", counter++);
    status_.append(state.down(button::left) ? brushes_.red : brushes_.green, L"\nLMB");
    status_.append(state.down(button::right) ? brushes_.red : brushes_.green, L"\nRMB");
    status_.visualize(state.buttons.data(), state.buttons.size());

    // Write report.
    report_.append(brushes_.white, L"Searching ...");
    report_.visualize(data.data(), data.size(), data_styles);

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
    scene_work_->status.Reset();
    scene_work_->report.Reset();
    scene_work_->labels.clear();
    status_.clear();
    report_.clear();

    // Limit frame rate.
    std::this_thread::sleep_for(1ms);
  }
  co_return;
}

}  // namespace epos