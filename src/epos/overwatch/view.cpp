#include "view.hpp"
#include <epos/error.hpp>
#include <epos/fonts.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/mean.hpp>

namespace epos::overwatch {

view::view(HINSTANCE instance, HWND hwnd, long cx, long cy) :
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

  // Create brushes (uses material design colors).
  const auto create_brush = [this](auto& dc, UINT32 color, FLOAT alpha, ID2D1SolidColorBrush** brush) {
    HR(dc->CreateSolidColorBrush(D2D1::ColorF(color, alpha), brush));
  };

  create_brush(dc_, 0x000000, 1.0f, &brushes_.black);   // Full Black
  create_brush(dc_, 0xFFFFFF, 1.0f, &brushes_.white);   // Full White
  create_brush(dc_, 0xE57373, 1.0f, &brushes_.red);     //  300 Red
  create_brush(dc_, 0xFFB74D, 1.0f, &brushes_.orange);  //  300 Orange
  create_brush(dc_, 0xFFF176, 1.0f, &brushes_.yellow);  //  300 Yellow
  create_brush(dc_, 0xAED581, 1.0f, &brushes_.green);   //  300 Light Green
  create_brush(dc_, 0x4FC3F7, 1.0f, &brushes_.blue);    //  300 Light Blue
  create_brush(dc_, 0xBDBDBD, 1.0f, &brushes_.gray);    //  400 Gray
  create_brush(dc_, 0xF5F5F5, 0.6f, &brushes_.info);    //  100 Gray
  create_brush(dc_, 0x000000, 0.5f, &brushes_.frame);   // Full Black
  create_brush(dc_, 0xFAFAFA, 0.4f, &brushes_.spread);  //   50 Gray

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

  create_brush(dc_, 0xD50000, 1.0f, &brushes_.target[0]);  // A700 Red
  create_brush(dc_, 0xD50000, 0.1f, &brushes_.target[1]);  // A700 Red
  create_brush(dc_, 0xFF5252, 0.5f, &brushes_.target[2]);  // A200 Red
  create_brush(dc_, 0xFF8A80, 0.8f, &brushes_.target[3]);  // A100 Red
  create_brush(dc_, 0xFF9E80, 0.7f, &brushes_.target[4]);  // A100 Deep Orange
  create_brush(dc_, 0xFFD180, 0.6f, &brushes_.target[5]);  // A100 Orange
  create_brush(dc_, 0xFFE57F, 0.5f, &brushes_.target[6]);  // A100 Amber
  create_brush(dc_, 0xFFFF8D, 0.4f, &brushes_.target[7]);  // A100 Yellow
  create_brush(dc_, 0xEFEBE9, 0.3f, &brushes_.target[8]);  //   50 Brown

  create_brush(dc_, 0xD500F9, 1.0f, &brushes_.tank[0]);  // A400 Purple
  create_brush(dc_, 0xD500F9, 0.1f, &brushes_.tank[1]);  // A400 Purple
  create_brush(dc_, 0xE040FB, 0.5f, &brushes_.tank[2]);  // A200 Purple
  create_brush(dc_, 0xEA80FC, 0.8f, &brushes_.tank[3]);  // A100 Purple
  create_brush(dc_, 0xB388FF, 0.7f, &brushes_.tank[4]);  // A100 Deep Purple
  create_brush(dc_, 0x8C9EFF, 0.6f, &brushes_.tank[5]);  // A100 Indigo
  create_brush(dc_, 0x82B1FF, 0.5f, &brushes_.tank[6]);  // A100 Blue
  create_brush(dc_, 0x80D8FF, 0.4f, &brushes_.tank[7]);  // A100 Light Blue
  create_brush(dc_, 0xECEFF1, 0.3f, &brushes_.tank[8]);  //   50 Blue Gray

  // Create formats.
  const auto create_format = [this](LPCWSTR name, FLOAT size, BOOL bold, IDWriteTextFormat** format) {
    constexpr auto style = DWRITE_FONT_STYLE_NORMAL;
    constexpr auto stretch = DWRITE_FONT_STRETCH_NORMAL;
    const auto weight = bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL;
    HR(factory_->CreateTextFormat(name, fonts_.Get(), weight, style, stretch, size, L"en-US", format));
  };

  create_format(L"Roboto", 14.0f, FALSE, &formats_.label);
  create_format(L"Roboto Mono", 11.0f, FALSE, &formats_.debug);
  create_format(L"Roboto Mono", 12.0f, FALSE, &formats_.status);
  create_format(L"Roboto Mono", 12.0f, FALSE, &formats_.report);
  formats_.status->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);

  // Initialize movement array.
  for (std::size_t i = 0; i < game::entities; i++) {
    movement_[i] = boost::circular_buffer<snapshot>(128);
  }

  // Create device.
  if (const auto rv = device_.create(); !rv) {
    throw std::system_error(rv.error(), "create");
  }

  // Start thread.
  boost::asio::co_spawn(context_, run(), boost::asio::detached);
  thread_ = std::jthread([this]() noexcept {
    SetThreadDescription(GetCurrentThread(), L"view");
    boost::system::error_code ec;
    context_.run(ec);
  });
}

view::~view()
{
  // Stop thread.
  stop_.store(true, std::memory_order_release);
  context_.stop();
}

overlay::command view::render() noexcept
{
  // Measure draw duration.
  const auto tp0 = clock::now();

  // Handle scene updated notification.
  auto scene_done_updated_expected = true;
  if (scene_done_updated_.compare_exchange_weak(scene_done_updated_expected, false)) {
    scene_draw_ = scene_done_.exchange(scene_draw_);
    for (std::size_t i = 0; i < scene_draw_->entities; i++) {
      movement_[i].clear();
    }
  }

  // Clear scene.
  dc_->Clear();
  dc_->SetTransform(D2D1::IdentityMatrix());

  // Draw report.
  if (scene_draw_->report) {
    dc_->FillRectangle(region::report, brushes_.report.Get());
    constexpr D2D1_POINT_2F origin{ region::text::report.left, region::text::report.top };
    draw(dc_, origin, scene_draw_->report, brushes_.gray);
  }

  // Clear labels.
  scene_labels_.clear();

  // Get input state.
  const auto state = input_.get_sync();
  mouse_.push_back({ state.mx / 4.0f, state.my / 4.0f });
  XMFLOAT2 mouse{};
  for (const auto& e : mouse_) {
    mouse.x += e.x;
    mouse.y += e.y;
  }
  mouse.x /= mouse_.size();
  mouse.y /= mouse_.size();

  if (state.pressed(key::pause)) {
    team_ = team_ == game::team::one ? game::team::two : game::team::one;
  }

  if (state.pressed(key::f10)) {
    hero_ = game::hero::reaper;
  } else if (state.pressed(key::f11)) {
    hero_ = game::hero::symmetra;
  } else if (state.pressed(key::f12)) {
    hero_ = game::hero::widowmaker;
  }

  // Update memory.
  if (!scene_draw_->vm || !device_.update()) {
    if (hero_ != game::hero::none) {
      info_.clear();
      info_.append(L"\n\n");
      info_.append(game::hero_name(hero_).data());
      draw(dc_, info_, region::text::duration, formats_.status, brushes_.info);
    }
    return command::none;
  }

  // Update movement.
  if (tp0 > update_movement_) {
    for (std::size_t i = 0; i < scene_draw_->entities; i++) {
      movement_[i].push_front({ XMLoadFloat3(&entities_[i].p0), tp0 });
    }
    update_movement_ = tp0 + 1ms;
  }

  // Handle hero.
  switch (hero_) {
  case game::hero::reaper:
    reaper(tp0, state, mouse);
    break;
  case game::hero::symmetra:
    symmetra(tp0, state, mouse);
    break;
  case game::hero::widowmaker:
    widowmaker(tp0, state, mouse);
    break;
  }

  // Draw status.
  if (status_) {
    ComPtr<IDWriteTextLayout> status;
    constexpr auto cx = region::text::status.right - region::text::status.left;
    constexpr auto cy = region::text::status.bottom - region::text::status.top;
    status_.create(factory_, formats_.status, cx, cy, &status);
    dc_->FillRectangle(region::status, brushes_.status.Get());
    constexpr D2D1_POINT_2F origin{ region::text::status.left, region::text::status.top };
    draw(dc_, origin, status, brushes_.gray);
    status_.reset();
  }

  // Draw labels.
  if (!scene_labels_.empty()) {
    outline_dc_->BeginDraw();
    outline_dc_->Clear();
    for (const auto& label : scene_labels_) {
      if (label.layout && brushes_.black) {
        draw(outline_dc_, { label.x, label.y }, label.layout, brushes_.black);
      }
    }
    outline_dc_->EndDraw();
    dc_->DrawImage(outline_dilate_.Get(), D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
    for (const auto& label : scene_labels_) {
      if (label.layout && brushes_.black) {
        draw(dc_, { label.x, label.y }, label.layout, label.brush);
      }
    }
  }

  // Draw info.
  info_.clear();
  if (hero_ == game::hero::none) {
    info_.push_back(L'\n');
  }
  const auto draw_ms = duration_cast<milliseconds>(tp0 - draw_).count();
  const auto swap_ms = duration_cast<milliseconds>(swap_duration_).count();
  std::format_to(std::back_inserter(info_), L"{:.03f}\n{:.03f}\n", draw_ms, swap_ms);
  if (hero_ != game::hero::none) {
    info_.append(game::hero_name(hero_).data());
  }
  draw(dc_, info_, region::text::duration, formats_.status, brushes_.info);

  // Update draw duration.
  draw_ = tp0;
  return command::update;
}

void view::presented() noexcept
{
  const auto now = clock::now();
  swap_duration_ = now - swap_;
  swap_ = now;
}

boost::asio::awaitable<void> view::update(std::chrono::steady_clock::duration wait) noexcept
{
  // Create report.
  if (report_) {
    constexpr auto cx = region::text::report.right - region::text::report.left;
    constexpr auto cy = region::text::report.bottom - region::text::report.top;
    report_.create(factory_, formats_.report, cx, cy, &scene_work_->report);
  }

  // Swap work and done scenes.
  scene_work_ = scene_done_.exchange(scene_work_);

  // Indicate that the done scene was updated.
  scene_done_updated_.store(true, std::memory_order_release);

  // Signal overlay to call render() on another thread.
  overlay::update();

  // Reset scene.
  scene_work_->report.Reset();
  scene_work_->entities = 0;
  scene_work_->vm = false;
  report_.reset();

  // Limit frame rate.
  timer_.expires_from_now(wait);
  co_await timer_.async_wait();
  co_return;
}

boost::asio::awaitable<void> view::run() noexcept
{
  bool first = true;
  while (!stop_.load(std::memory_order_relaxed)) {
    // Close device.
    if (first) {
      first = false;
    } else {
      scene_work_->entities = 0;
      scene_work_->vm = false;
      co_await update(5s);
      device_.close();
    }

    // Find window.
    const auto hwnd = FindWindow("TankWindowClass", "Overwatch");
    if (!hwnd) {
      continue;
    }

    // Find process.
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (!pid) {
      continue;
    }

    // Open process.
    report_.write(L"Process: ");
    if (const auto rv = device_.open(pid); !rv) {
      report_.write(brushes_.red, L"ERROR\n");
      report_.write(brushes_.white, rv.error().message());
      continue;
    }
    report_.write(L"{}\n", pid);

    // Get modules.
    report_.write(L"Modules: ");
    const auto modules = device_.modules();
    if (!modules) {
      report_.write(brushes_.red, L"ERROR\n");
      report_.write(brushes_.white, modules.error().message());
      continue;
    }
    report_.write(L"{}\n", modules->size());

    // Get game module.
    const auto game = std::find_if(modules->begin(), modules->end(), [](const deus::module& module) {
      return module.name() == L"Overwatch.exe";
    });
    if (game == modules->end()) {
      report_.write(brushes_.red, L"Could not find game module.");
      continue;
    }

    // Get view matrix.
    report_.write(L"VMatrix: ");
    std::uintptr_t vm_base = 0;
    if (const auto rv = device_.read(game->base + game::vm, vm_base); !rv) {
      report_.write(brushes_.red, L"ERROR\n");
      report_.write(brushes_.white, rv.error().message());
      continue;
    }
    if (!vm_base) {
      report_.write(brushes_.red, L"0\n");
    } else {
      report_.write(L"{:X}\n", vm_base);
    }
    const deus::copy vm{ vm_base + game::vm_offset, reinterpret_cast<UINT_PTR>(&vm_), sizeof(vm_) };

    // Compares signature offset location to watched copy.
    constexpr auto compare = [](std::uintptr_t offset, const deus::copy& copy) noexcept {
      return offset == copy.src;
    };

    // Watch memory.
    game::entity entity;
    std::vector<deus::copy> watch;
    std::vector<std::uintptr_t> offsets;
    while (!stop_.load(std::memory_order_relaxed)) {
      // Check process.
      DWORD cmp = 0;
      GetWindowThreadProcessId(hwnd, &cmp);
      if (cmp != pid) {
        break;
      }

      // Get regions.
      report_.write(L"Regions: ");
      const auto region_size = game::entity_region_size;
      const auto regions = device_.regions(MEM_PRIVATE, PAGE_READWRITE, PAGE_READWRITE, region_size);
      if (!regions) {
        report_.write(brushes_.red, L"ERROR\n");
        report_.write(brushes_.white, regions.error().message());
        break;
      }
      report_.write(L"{}\n", regions->size());
      if (regions->empty()) {
        break;
      }

      // Get offsets.
      offsets.clear();
      std::error_code ec;
      for (const auto& region : *regions) {
        const auto read = device_.read(region.base_address, memory_.data(), region_size);
        if (!read) {
          ec = read.error();
          break;
        }
        for (std::size_t i = 0; i < *read; i++) {
          const auto pos = qis::scan(memory_.data() + i, *read - i, game::entity_signature);
          if (pos == qis::npos) {
            break;
          }
          const auto signature = region.base_address + i + pos;
#if EPOS_OVERWATCH_UNKNOWN
          constexpr auto copy_size = offsetof(game::entity, signature);
#else
          constexpr auto copy_size = sizeof(game::entity);
#endif
          if (signature >= region.base_address + copy_size) {
            offsets.push_back(signature - copy_size);
          }
          i += pos;
        }
      }
      report_.write(L"Offsets: ");
      if (ec) {
        report_.write(brushes_.red, L"ERROR\n");
        report_.write(brushes_.white, ec.message());
        break;
      }
      report_.write(L"{}\n", offsets.size());
      if (offsets.empty()) {
        break;
      }

      // Sort offsets.
      std::sort(offsets.begin(), offsets.end());

      // Limit number of entities.
      const auto entities = std::min(offsets.size(), entities_.size());

      // Reset report.
      report_.reset();

      // Check if watched memory changed.
      auto changed = offsets.size() + 1 != watch.size();
      if (!changed && std::equal(offsets.begin(), offsets.end(), watch.begin(), compare)) {
        scene_work_->entities = entities;
        scene_work_->vm = true;
        co_await update(1s);
        continue;
      }

      // Report success.
      watch.clear();
      for (std::size_t i = 0; i < entities; i++) {
        watch.emplace_back(offsets[i], reinterpret_cast<UINT_PTR>(&entities_[i]), sizeof(entities_[i]));
      }
      watch.push_back(vm);
      if (const auto rv = device_.watch(watch); !rv) {
        report_.write(brushes_.red, L"Could not start watching memory\n");
        report_.write(brushes_.white, rv.error().message());
        break;
      }
      scene_work_->entities = entities;
      scene_work_->vm = true;
      co_await update(1s);
    }
  }
  co_return;
}

}  // namespace epos::overwatch