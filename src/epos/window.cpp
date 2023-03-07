#include "window.hpp"
#include <epos/error.hpp>
#include <epos/overlay/overwatch.hpp>
#include <commctrl.h>

namespace epos {
namespace {

constexpr UINT WM_MENU = WM_APP + 1;
constexpr UINT ID_MENU_EXIT = 40001;

}  // namespace

window::window(HINSTANCE instance) : instance_(instance)
{
  // Register window class.
  WNDCLASSEX wc = {};
  wc.cbSize = sizeof(wc);
  wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = window::proc;
  wc.hInstance = instance_;
  LoadIconMetric(instance_, MAKEINTRESOURCEW(101), LIM_LARGE, &wc.hIcon);
  LoadIconMetric(instance_, MAKEINTRESOURCEW(101), LIM_SMALL, &wc.hIconSm);
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOWFRAME);
  wc.lpszClassName = "epos::overlay";
  atom_ = RegisterClassEx(&wc);
  if (!atom_) {
    throw std::system_error(error(GetLastError()), "RegisterClassEx");
  }

  // Initialize notification icon.
  icon_.cbSize = sizeof(icon_);
  icon_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
  icon_.uCallbackMessage = WM_MENU;
  icon_.uVersion = NOTIFYICON_VERSION_4;
  LoadIconMetric(instance_, MAKEINTRESOURCEW(101), LIM_SMALL, &icon_.hIcon);
  std::strncpy(icon_.szTip, "EPOS", 5);
}

window::~window() noexcept(false)
{
  // Unregister window class.
  const auto wa = reinterpret_cast<LPCSTR>(static_cast<LONG_PTR>(atom_));
  UnregisterClass(wa, instance_);
  if (exception_ && !std::uncaught_exceptions()) {
    std::rethrow_exception(exception_);
  }
}

void window::create()
{
  // Create window.
  constexpr auto x = CW_USEDEFAULT;
  constexpr auto y = CW_USEDEFAULT;
  constexpr auto w = CW_USEDEFAULT;
  constexpr auto h = CW_USEDEFAULT;
  const auto wa = reinterpret_cast<LPCSTR>(static_cast<LONG_PTR>(atom_));
  DWORD ws = WS_POPUP;
  DWORD ex = WS_EX_TOOLWINDOW;
  ex |= WS_EX_TOPMOST;
  ex |= WS_EX_NOACTIVATE;
  ex |= WS_EX_NOREDIRECTIONBITMAP;
  ex |= WS_EX_TRANSPARENT;
  ex |= WS_EX_LAYERED;
  if (!CreateWindowEx(ex, wa, "EPOS", ws, x, y, w, h, nullptr, nullptr, instance_, this)) {
    throw std::system_error(error(GetLastError()), "CreateWindowEx");
  }
}

void window::on_create()
{
  // Create menu.
  menu_ = CreatePopupMenu();
  if (!menu_) {
    throw std::system_error(error(GetLastError()), "CreatePopupMenu");
  }
  InsertMenu(menu_, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, ID_MENU_EXIT, "E&xit");

  // Add notification icon.
  icon_.hWnd = hwnd_;
  if (!Shell_NotifyIcon(NIM_ADD, &icon_)) {
    throw std::runtime_error("Could not create notification icon.");
  }
  if (!Shell_NotifyIcon(NIM_SETVERSION, &icon_)) {
    throw std::runtime_error("Could not set notification icon version.");
  }

  // Resize window.
  RECT rc = { 0, 0, 100, 100 };
  if (const auto monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONULL)) {
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfo(monitor, &mi)) {
      rc = mi.rcMonitor;
    }
  }
  const auto cx = rc.right - rc.left;
  const auto cy = rc.bottom - rc.top;
  SetWindowPos(hwnd_, nullptr, rc.left, rc.top, cx, cy, SWP_NOACTIVATE);

  // Create overlay.
  overlay_ = std::make_unique<overwatch>(instance_, hwnd_, cx, cy);
  overlay_->start();

  // Show window.
  ShowWindow(hwnd_, SW_SHOW);
  SetLayeredWindowAttributes(hwnd_, 0, 0xFF, LWA_ALPHA);
}

void window::on_destroy() noexcept
{
  // Hide window.
  ShowWindow(hwnd_, SW_HIDE);

  // Destroy overlay and view.
  overlay_.reset();

  // Delete notification icon.
  Shell_NotifyIcon(NIM_DELETE, &icon_);

  // Destroy menu.
  DestroyMenu(menu_);
}

void window::on_close() noexcept
{
  // Destroy overlay.
  overlay_.reset();
}

void window::on_paint() noexcept
{
  // Update overlay.
  if (overlay_) {
    overlay_->update();
  }
}

void window::on_menu() noexcept
{
  // Show menu.
  POINT cursor{};
  if (GetCursorPos(&cursor)) {
    SetForegroundWindow(hwnd_);
    constexpr UINT flags = TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_BOTTOMALIGN;
    TrackPopupMenu(menu_, flags, cursor.x, cursor.y, 0, hwnd_, nullptr);
  }
}

void window::on_command(UINT command) noexcept
{
  // Handle menu commands.
  switch (command) {
  case ID_MENU_EXIT:
    PostMessage(hwnd_, WM_CLOSE, 0, 0);
    break;
  }
}

LRESULT window::proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) noexcept
{
  constexpr auto get_window = [](HWND hwnd) noexcept {
    return reinterpret_cast<epos::window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
  };
  switch (msg) {
  case WM_PAINT:
    if (const auto window = get_window(hwnd)) {
      window->on_paint();
    }
    ValidateRect(hwnd, nullptr);
    return 0;
  case WM_ERASEBKGND:
    return 1;
  case WM_CREATE: {
    const auto params = reinterpret_cast<LPCREATESTRUCT>(lparam);
    const auto window = reinterpret_cast<epos::window*>(params->lpCreateParams);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
    try {
      window->hwnd_ = hwnd;
      window->on_create();
    }
    catch (...) {
      window->exception_ = std::current_exception();
      PostMessage(hwnd, WM_CLOSE, 0, 0);
    }
    return 0;
  }
  case WM_DESTROY:
    if (const auto window = get_window(hwnd)) {
      window->on_destroy();
      window->hwnd_ = nullptr;
    }
    SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
    PostQuitMessage(EXIT_SUCCESS);
    return 0;
  case WM_CLOSE:
    if (const auto window = get_window(hwnd)) {
      window->on_close();
    }
    DestroyWindow(hwnd);
    return 0;
  case WM_COMMAND:
    if (const auto window = get_window(hwnd)) {
      window->on_command(wparam);
    }
    return 0;
  case WM_MENU:
    if (LOWORD(lparam) == WM_RBUTTONDOWN) {
      if (const auto window = get_window(hwnd)) {
        window->on_menu();
      }
    }
    return 0;
  }
  return DefWindowProc(hwnd, msg, wparam, lparam);
}

}  // namespace epos