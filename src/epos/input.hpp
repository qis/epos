#pragma once
#include <epos/clock.hpp>
#include <epos/enum.hpp>
#include <epos/runtime.hpp>
#include <epos/timer.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/udp.hpp>
#include <dinput.h>
#include <dinputd.h>
#include <array>
#include <atomic>
#include <chrono>
#include <thread>
#include <type_traits>
#include <cstdint>

namespace epos {

// clang-format off

EPOS_ENUM_DEFINE_CLASS(
  (button,  std::uint8_t),
  (left,    0),
  (right,   1),
  (middle,  2),
  (down,    3),
  (up,      4));

template <class E>
concept Button = std::is_same_v<std::remove_cvref_t<E>, button>;

EPOS_ENUM_DEFINE_CLASS(
  (key,     std::uint8_t),
  (escape,  DIK_ESCAPE),
  (tab,     DIK_TAB),
  (q,       DIK_Q),
  (w,       DIK_W),
  (e,       DIK_E),
  (r,       DIK_R),
  (enter,   DIK_RETURN),
  (control, DIK_LCONTROL),
  (s,       DIK_S),
  (shift,   DIK_LSHIFT),
  (c,       DIK_C),
  (b,       DIK_B),
  (alt,     DIK_LMENU),
  (space,   DIK_SPACE),
  (f6,      DIK_F6),
  (f7,      DIK_F7),
  (f8,      DIK_F8),
  (f9,      DIK_F9),
  (f10,     DIK_F10),
  (f11,     DIK_F11),
  (f12,     DIK_F12),
  (pause,   DIK_PAUSE),
  (win,     DIK_LWIN));

template <class E>
concept Key = std::is_same_v<std::remove_cvref_t<E>, key>;

template <class E>
concept Input = Button<E> || Key<E>;

// clang-format on

class input {
public:
  static constexpr std::chrono::seconds maximum_mask_duration{ 10 };

  struct state {
    /// Duration since the last @ref input::get call.
    clock::duration duration{};

    /// Relative horizontal mouse movement during @ref duration.
    std::int32_t mx{ 0 };

    /// Relative vertical mouse movement during @ref duration.
    std::int32_t my{ 0 };

    /// State and changes of buttons during @ref duration.
    std::array<std::uint8_t, enum_size_v<button>> buttons;

    /// State and changes of keys during @ref duration.
    std::array<std::uint8_t, enum_size_v<key>> keys;

    /// Checks if the current button or key state is up.
    constexpr bool up(Input auto input) const noexcept
    {
      return (get(input) & 0x01) == 0x00;
    }

    /// Checks if the current button or key state is down.
    constexpr bool down(Input auto input) const noexcept
    {
      return !up(input);
    }

    /// Checks if the button or key was pressed during @ref duration.
    constexpr bool pressed(Input auto input) const noexcept
    {
      return (get(input) & 0x02) != 0x00;
    }

    /// Checks if the button or key was released during @ref duration.
    constexpr bool released(Input auto input) const noexcept
    {
      return (get(input) & 0x04) != 0x00;
    }

    /// Returns button state during @ref duration.
    constexpr std::uint8_t get(button input) const noexcept
    {
      return buttons[enum_index(input)];
    }

    /// Returns key state during @ref duration.
    constexpr std::uint8_t get(key input) const noexcept
    {
      return keys[enum_index(input)];
    }
  };

  input(HINSTANCE instance, HWND hwnd);

  input(input&& other) = delete;
  input(const input& other) = delete;
  input& operator=(input&& other) = delete;
  input& operator=(const input& other) = delete;

  ~input();

  const state& update() noexcept;

  void mask(button button, std::chrono::milliseconds duration = {}) noexcept;
  void move(std::int16_t x, std::int16_t y) noexcept;

private:
  ComPtr<IDirectInput8> input_;
  ComPtr<IDirectInputDevice8> keybd_;
  ComPtr<IDirectInputDevice8> mouse_;

  state state_;
  DIMOUSESTATE2 mouse_state_{};
  std::vector<std::uint8_t> keybd_state_;
  clock::time_point update_{ clock::now() };

  boost::asio::io_context context_;
  boost::asio::ip::udp::socket socket_{ context_ };
  boost::asio::ip::udp::endpoint endpoint_;
  std::array<std::uint8_t, 4> data_{};
};

}  // namespace epos
