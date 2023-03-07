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
  overlay(instance, hwnd, cx, cy), cx_(cx), cy_(cy)
{
  // Create DirectWrite objects.
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

  HR(dc_->CreateSolidColorBrush(D2D1::ColorF(0xE53935), &color_.red));
  HR(dc_->CreateSolidColorBrush(D2D1::ColorF(0x8BC34A), &color_.green));
  HR(dc_->CreateSolidColorBrush(D2D1::ColorF(0x2196F3), &color_.blue));
  HR(dc_->CreateSolidColorBrush(D2D1::ColorF(0x000000), &color_.black));
  HR(dc_->CreateSolidColorBrush(D2D1::ColorF(0xF0F0F0), &color_.white));

  const auto create = [&](LPCWSTR name, FLOAT size, BOOL bold, IDWriteTextFormat** format) {
    constexpr auto style = DWRITE_FONT_STYLE_NORMAL;
    constexpr auto stretch = DWRITE_FONT_STRETCH_NORMAL;
    const auto weight = bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL;
    HR(factory_->CreateTextFormat(name, fonts_.Get(), weight, style, stretch, 14.0f, L"en-US", format));
  };

  create(L"Roboto", 14.0f, FALSE, &format_.regular);
  create(L"Roboto", 24.0f, FALSE, &format_.numeric);
  create(L"Roboto Mono", 10.0f, FALSE, &format_.monospace);
  format_.regular->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);

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

void overwatch::draw() noexcept
{
  if (!status_.empty()) {
    const auto text = status_.data();
    const auto size = static_cast<UINT32>(status_.size());
    const auto rect = D2D1::RectF(8.0f, 29.0f, 500.0f, cy_ - 38.0f);
    const auto format = format_.regular.Get();
    const auto brush = color_.red.Get();
    dc_->DrawText(text, size, format, rect, brush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
  }
}

boost::asio::awaitable<void> overwatch::run() noexcept
{
  size_t counter = 0;
  status_.reserve(1024);
  timer timer{ co_await boost::asio::this_coro::executor };
  while (!stop_.load(std::memory_order_relaxed)) {
    status_.clear();
    std::format_to(std::back_inserter(status_), "{}", counter++);
    update();
    std::this_thread::sleep_for(std::chrono::milliseconds(8));
    //timer.expires_from_now(std::chrono::milliseconds(8));
    //if (const auto [ec] = co_await timer.async_wait(); ec) {
    //  co_return;
    //}
  }
  co_return;
}

}  // namespace epos