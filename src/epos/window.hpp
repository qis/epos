#pragma once
#include <epos/overlay.hpp>
#include <windows.h>
#include <shellapi.h>
#include <exception>
#include <memory>

namespace epos {

class window {
public:
  window(HINSTANCE instance);

  window(window&& other) = delete;
  window(const window& other) = delete;
  window& operator=(window&& other) = delete;
  window& operator=(const window& other) = delete;

  ~window() noexcept(false);

  void create();

private:
  void on_create();
  void on_destroy() noexcept;
  void on_close() noexcept;
  void on_paint() noexcept;
  void on_menu() noexcept;
  void on_command(UINT command) noexcept;

  static LRESULT proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) noexcept;

  HINSTANCE instance_;
  ATOM atom_{};
  HWND hwnd_{};

  NOTIFYICONDATA icon_{};
  HMENU menu_{};

  std::unique_ptr<overlay> overlay_;
  std::exception_ptr exception_;
};

}  // namespace epos