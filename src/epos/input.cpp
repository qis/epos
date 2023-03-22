#include "input.hpp"
#include <epos/error.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_future.hpp>
#include <algorithm>

namespace epos {
namespace {

constexpr std::size_t keybd_state_size = 256;
constexpr std::size_t mouse_state_size = sizeof(DIMOUSESTATE2);
constexpr const char* hid_address = "192.168.178.7";
constexpr const char* hid_service = "777";

}  // namespace

input::input(HINSTANCE instance, HWND hwnd)
{
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

  // Initialize DirectInput state buffers.
  keybd_state_.resize(keybd_state_size * 2 + mouse_state_size * 2, std::uint8_t(0));
  std::memset(&mouse_state_, 0, mouse_state_size);

  // Create socket and endpoint.
  boost::system::error_code ec;
  socket_.open(boost::asio::ip::udp::v4(), ec);
  if (!ec) {
    boost::asio::ip::udp::resolver resolver{ context_ };
    const auto endpoints = resolver.resolve(boost::asio::ip::udp::v4(), hid_address, hid_service, ec);
    if (!ec && !endpoints.empty()) {
      endpoint_ = *endpoints.begin();
    }
  }

  // Start context thread.
  boost::asio::co_spawn(context_, run(), boost::asio::detached);
  thread_ = std::jthread([this]() noexcept {
    boost::system::error_code ec;
    context_.run(ec);
  });
}

input::~input()
{
  context_.stop();
  if (thread_.joinable()) {
    thread_.join();
  }
  if (mouse_) {
    mouse_->Unacquire();
  }
  if (keybd_) {
    keybd_->Unacquire();
  }
}

boost::asio::awaitable<input::state> input::get() noexcept
{
  return boost::asio::co_spawn(context_, reset(), boost::asio::use_awaitable);
}

input::state input::get_sync() noexcept
{
  return boost::asio::co_spawn(context_, reset(), boost::asio::use_future).get();
}

void input::mask(
  button button,
  std::chrono::milliseconds duration,
  std::chrono::steady_clock::duration delay) noexcept
{
  if (delay > std::chrono::steady_clock::duration(0)) {
    hid_timer_.expires_from_now(delay);
    hid_timer_.async_wait([this, button, duration](boost::system::error_code ec) noexcept {
      mask(button, duration);
    });
    return;
  }

  switch (button) {
  case button::left:
    hid_data_[0] = 0x01 << 0;
    break;
  case button::right:
    hid_data_[0] = 0x01 << 1;
    break;
  case button::middle:
    hid_data_[0] = 0x01 << 2;
    break;
  case button::down:
    hid_data_[0] = 0x01 << 3;
    break;
  case button::up:
    hid_data_[0] = 0x01 << 4;
    break;
  default:
    hid_data_[0] = 0;
    break;
  }

  duration = std::clamp(duration, 0ms, std::chrono::milliseconds(maximum_mask_duration));
  const auto ms = static_cast<std::uint16_t>(duration.count());
  hid_data_[1] = static_cast<std::uint8_t>((ms >> 8) & 0xFF);
  hid_data_[2] = static_cast<std::uint8_t>((ms >> 0) & 0xFF);

  while (true) {
    boost::system::error_code ec;
    socket_.send_to(boost::asio::buffer(hid_data_.data(), 3), endpoint_, {}, ec);
    if (ec != boost::system::errc::resource_unavailable_try_again) {
      break;
    }
    std::this_thread::sleep_for(1us);
  }
}

void input::move(std::int16_t x, std::int16_t y) noexcept
{
  if (!x && !y) {
    return;
  }

  hid_data_[0] = static_cast<std::uint8_t>(static_cast<uint16_t>(x) & 0xFF);
  hid_data_[1] = static_cast<std::uint8_t>(static_cast<uint16_t>(x) >> 8 & 0xFF);
  hid_data_[2] = static_cast<std::uint8_t>(static_cast<uint16_t>(y) & 0xFF);
  hid_data_[3] = static_cast<std::uint8_t>(static_cast<uint16_t>(y) >> 8 & 0xFF);

  while (true) {
    boost::system::error_code ec;
    socket_.send_to(boost::asio::buffer(hid_data_.data(), 4), endpoint_, {}, ec);
    if (ec != boost::system::errc::resource_unavailable_try_again) {
      break;
    }
    std::this_thread::sleep_for(1us);
  }
}

void input::update() noexcept
{
  // Get keyboard state.
  if (FAILED(keybd_->GetDeviceState(keybd_state_size, keybd_state_.data()))) {
    keybd_->Acquire();
    return;
  }

  // Update keys.
  for (std::size_t i = 0; i < state_.keys.size(); i++) {
    if (keybd_state_[static_cast<std::uint8_t>(enum_entry<key>(i))] & 0x80) {
      if (state_.keys[i] & 0x01) {
        state_.keys[i] |= 0x01;  // down
      } else {
        state_.keys[i] |= 0x03;  // down and pressed
      }
    } else {
      if (state_.keys[i] & 0x01) {
        state_.keys[i] |= 0x04;  // released
      }
      state_.keys[i] &= ~0x01;  // up
    }
  }

  // Get mouse state.
  if (FAILED(mouse_->GetDeviceState(mouse_state_size, &mouse_state_))) {
    mouse_->Acquire();
    return;
  }

  // Update time point.
  state_.update = clock::now();

  // Update buttons.
  for (std::size_t i = 0; i < state_.buttons.size(); i++) {
    if (mouse_state_.rgbButtons[i]) {
      if (state_.buttons[i] & 0x01) {
        state_.buttons[i] |= 0x01;  // down
      } else {
        state_.buttons[i] |= 0x03;  // down and pressed
      }
    } else {
      if (state_.buttons[i] & 0x01) {
        state_.buttons[i] |= 0x04;  // released
      }
      state_.buttons[i] &= ~0x01;  // up
    }
  }

  // Update mouse movement.
  state_.mx += mouse_state_.lX;
  state_.my += mouse_state_.lY;
}

boost::asio::awaitable<input::state> input::reset() noexcept
{
  // Update state.
  update();

  // Copy state.
  auto state = state_;

  // Set duration.
  state.duration = state.update - state_update_;

  // Reset keys.
  for (std::size_t i = 0; i < state_.keys.size(); i++) {
    state_.keys[i] &= 0x01;
  }

  // Reset buttons.
  for (std::size_t i = 0; i < state_.buttons.size(); i++) {
    state_.buttons[i] &= 0x01;
  }

  // Reset mouse movement.
  state_.mx = 0;
  state_.my = 0;

  // Reset time point.
  state_update_ = state.update;

  // Return state copy.
  co_return state;
}

boost::asio::awaitable<void> input::run() noexcept
{
  const auto executor = co_await boost::asio::this_coro::executor;
  SetThreadDescription(GetCurrentThread(), L"input");
  timer update_timer{ executor };
  while (true) {
    update();
    update_timer.expires_from_now(1ms);
    if (const auto [ec] = co_await update_timer.async_wait(); ec) {
      co_return;
    }
  }
  co_return;
}

}  // namespace epos