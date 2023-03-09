#include "overlay.hpp"
#include <epos/error.hpp>
#include <d2d1_2.h>
#include <d3d11.h>
#include <cassert>

namespace epos {

overlay::overlay(HINSTANCE instance, HWND hwnd, long cx, long cy)
{
  // Create swap chain.
  ComPtr<ID3D11Device> d3d_device;
  HR(D3D11CreateDevice(
    nullptr,
    D3D_DRIVER_TYPE_HARDWARE,
    nullptr,
    D3D11_CREATE_DEVICE_BGRA_SUPPORT,
    nullptr,
    0,
    D3D11_SDK_VERSION,
    &d3d_device,
    nullptr,
    nullptr));

  ComPtr<IDXGIDevice> dxgi_device;
  HR(d3d_device.As(&dxgi_device));

  ComPtr<IDXGIFactory2> dxgi_factory;
#ifdef NDEBUG
  constexpr UINT dxgi_factory_flags = 0;
#else
  constexpr UINT dxgi_factory_flags = DXGI_CREATE_FACTORY_DEBUG;
#endif
  HR(CreateDXGIFactory2(dxgi_factory_flags, __uuidof(dxgi_factory), &dxgi_factory));

  DXGI_SWAP_CHAIN_DESC1 description = {};
  description.Width = static_cast<decltype(description.Width)>(cx);
  description.Height = static_cast<decltype(description.Height)>(cy);
  description.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  description.SampleDesc.Count = 1;
  description.BufferCount = 2;
  description.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
  description.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
  HR(dxgi_factory->CreateSwapChainForComposition(dxgi_device.Get(), &description, nullptr, &sc_));

  // Create device context.
  ComPtr<ID2D1Factory2> d2d_factory;
  constexpr auto d2d_factory_type = D2D1_FACTORY_TYPE_SINGLE_THREADED;
#ifdef NDEBUG
  const D2D1_FACTORY_OPTIONS d2d_factory_options{ D2D1_DEBUG_LEVEL_NONE };
#else
  const D2D1_FACTORY_OPTIONS d2d_factory_options{ D2D1_DEBUG_LEVEL_INFORMATION };
#endif
  HR(D2D1CreateFactory(d2d_factory_type, d2d_factory_options, d2d_factory.GetAddressOf()));

  ComPtr<ID2D1Device1> d2d_device;
  HR(d2d_factory->CreateDevice(dxgi_device.Get(), d2d_device.GetAddressOf()));

#ifdef NDEBUG
  constexpr auto d2d_device_options = D2D1_DEVICE_CONTEXT_OPTIONS_ENABLE_MULTITHREADED_OPTIMIZATIONS;
#else
  constexpr auto d2d_device_options = D2D1_DEVICE_CONTEXT_OPTIONS_NONE;
#endif
  HR(d2d_device->CreateDeviceContext(d2d_device_options, &dc_));
  dc_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

  // Create bitmap render target for swap chain.
  ComPtr<IDXGISurface2> surface;
  HR(sc_->GetBuffer(0, __uuidof(surface), &surface));

  D2D1_BITMAP_PROPERTIES1 buffer_properties = {};
  buffer_properties.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
  buffer_properties.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
  buffer_properties.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
  HR(dc_->CreateBitmapFromDxgiSurface(surface.Get(), buffer_properties, &buffer_));
  dc_->SetTarget(buffer_.Get());

  // Enable composition.
  HR(DCompositionCreateDevice(dxgi_device.Get(), __uuidof(composition_device_), &composition_device_));
  HR(composition_device_->CreateTargetForHwnd(hwnd, true, &composition_target_));
  HR(composition_device_->CreateVisual(&composition_visual_));
  HR(composition_visual_->SetContent(sc_.Get()));
  HR(composition_target_->SetRoot(composition_visual_.Get()));
  HR(composition_device_->Commit());
}

overlay::~overlay()
{
  stop();
}

void overlay::start() noexcept
{
  stop();
  command_.store(command::none);
  thread_ = std::jthread([this]() noexcept {
    run();
  });
}

void overlay::stop() noexcept
{
  if (thread_.joinable()) {
    command_.store(command::stop, std::memory_order_release);
    cv_.notify_all();
    thread_.join();
  }
}

void overlay::run() noexcept
{
  auto cmd = command::none;
  std::unique_lock lock{ mutex_ };
  while (cmd != command::stop) {
    cv_.wait(lock, [&]() noexcept {
      cmd = command_.exchange(command::none);
      return cmd != command::none;
    });
    while (cmd == command::update) {
      dc_->BeginDraw();
      render();
      const auto result = dc_->EndDraw();
      assert(SUCCEEDED(result));
      sc_->Present(0, DXGI_PRESENT_RESTART);
      cmd = command_.exchange(command::none);
    }
  }
}

}  // namespace epos