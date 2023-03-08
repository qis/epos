#pragma once
#include <epos/com.hpp>
#include <d2d1_1.h>
#include <dcomp.h>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace epos {

class overlay {
public:
  overlay(HINSTANCE instance, HWND hwnd, long cx, long cy);

  overlay(overlay&& other) = delete;
  overlay(const overlay& other) = delete;
  overlay& operator=(overlay&& other) = delete;
  overlay& operator=(const overlay& other) = delete;

  virtual ~overlay();

  void start() noexcept;
  void stop() noexcept;

  void update() noexcept
  {
    auto expected = command::none;
    if (command_.compare_exchange_strong(expected, command::update)) {
      cv_.notify_one();
    }
  }

protected:
  ComPtr<ID2D1DeviceContext> dc_;

  virtual void render() noexcept = 0;

private:
  void run() noexcept;

  ComPtr<IDXGISwapChain1> sc_;
  ComPtr<ID2D1Bitmap1> buffer_;

  ComPtr<IDCompositionDevice> composition_device_;
  ComPtr<IDCompositionTarget> composition_target_;
  ComPtr<IDCompositionVisual> composition_visual_;

  enum class command {
    none = 0,
    update,
    stop,
  };

  std::atomic<command> command_{ command::none };
  std::condition_variable cv_;
  std::mutex mutex_;
  std::jthread thread_;
};

}  // namespace epos