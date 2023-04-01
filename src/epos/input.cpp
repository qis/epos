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
}

input::~input()
{
  context_.stop();
  if (mouse_) {
    mouse_->Unacquire();
  }
  if (keybd_) {
    keybd_->Unacquire();
  }
}

const input::state& input::update() noexcept
{
  // Get keyboard state.
  if (FAILED(keybd_->GetDeviceState(keybd_state_size, keybd_state_.data()))) {
    keybd_->Acquire();
    return state_;
  }

  // Update keys.
  for (std::size_t i = 0; i < state_.keys.size(); i++) {
    state_.keys[i] &= 0x01;  // unset flags
    if (keybd_state_[static_cast<std::uint8_t>(enum_entry<key>(i))] & 0x80) {
      if (!(state_.keys[i] & 0x01)) {
        state_.keys[i] |= 0x03;  // set down state and pressed flag
      }
    } else {
      if (state_.keys[i] & 0x01) {
        state_.keys[i] |= 0x04;  // set released flag
      }
      state_.keys[i] &= ~0x01;  // set up state
    }
  }

  // Get mouse state.
  if (FAILED(mouse_->GetDeviceState(mouse_state_size, &mouse_state_))) {
    mouse_->Acquire();
    return state_;
  }

  // Update time point.
  const auto now = clock::now();
  state_.duration = now - update_;
  update_ = now;

  // Update buttons.
  for (std::size_t i = 0; i < state_.buttons.size(); i++) {
    state_.buttons[i] &= 0x01;  // unset flags
    if (mouse_state_.rgbButtons[i]) {
      if (!(state_.buttons[i] & 0x01)) {
        state_.buttons[i] |= 0x03;  // set down state and pressed flag
      }
    } else {
      if (state_.buttons[i] & 0x01) {
        state_.buttons[i] |= 0x04;  // set released flag
      }
      state_.buttons[i] &= ~0x01;  // set up state
    }
  }

  // Update mouse movement.
  state_.mx = mouse_state_.lX;
  state_.my = mouse_state_.lY;

  return state_;
}

void input::mask(button button, std::chrono::milliseconds duration) noexcept
{
  switch (button) {
  case button::left:
    data_[0] = 0x01 << 0;
    break;
  case button::right:
    data_[0] = 0x01 << 1;
    break;
  case button::middle:
    data_[0] = 0x01 << 2;
    break;
  case button::down:
    data_[0] = 0x01 << 3;
    break;
  case button::up:
    data_[0] = 0x01 << 4;
    break;
  default:
    data_[0] = 0;
    break;
  }

  duration = std::clamp(duration, 0ms, std::chrono::milliseconds(maximum_mask_duration));
  const auto ms = static_cast<std::uint16_t>(duration.count());
  data_[1] = static_cast<std::uint8_t>((ms >> 8) & 0xFF);
  data_[2] = static_cast<std::uint8_t>((ms >> 0) & 0xFF);

  while (true) {
    boost::system::error_code ec;
    socket_.send_to(boost::asio::buffer(data_.data(), 3), endpoint_, {}, ec);
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

  data_[0] = static_cast<std::uint8_t>(static_cast<uint16_t>(x) & 0xFF);
  data_[1] = static_cast<std::uint8_t>(static_cast<uint16_t>(x) >> 8 & 0xFF);
  data_[2] = static_cast<std::uint8_t>(static_cast<uint16_t>(y) & 0xFF);
  data_[3] = static_cast<std::uint8_t>(static_cast<uint16_t>(y) >> 8 & 0xFF);

  while (true) {
    boost::system::error_code ec;
    socket_.send_to(boost::asio::buffer(data_.data(), 4), endpoint_, {}, ec);
    if (ec != boost::system::errc::resource_unavailable_try_again) {
      break;
    }
    std::this_thread::sleep_for(1us);
  }
}

}  // namespace epos