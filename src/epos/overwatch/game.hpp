#pragma once
#include <DirectXMath.h>
#include <qis/signature.hpp>
#include <d3d11.h>
#include <d3d9types.h>
#include <optional>
#include <cstddef>

namespace epos::overwatch {
namespace settings {

inline const qis::signature signature{
  "?? ?? 00 00 ?? ?? 00 00 ?? 00 00 00 ?? 00 00 00 00 00 FF FF 02 00 01 00 00 00 00 00 ?? ?? ??"
};

constexpr std::size_t region_size = 0x180000;
constexpr std::uintptr_t view_matrix_base = 0x4172EA8;
constexpr std::uintptr_t view_matrix = 0x7E0;

}  // namespace settings

inline std::optional<DirectX::XMFLOAT2> project(
  const DirectX::XMMATRIX& vm,
  const DirectX::XMFLOAT3& v,
  int sw,
  int sh) noexcept
{
  // clang-format off
  const auto vx = vm.r[0].m128_f32[0] * v.x + vm.r[1].m128_f32[0] * v.y + vm.r[2].m128_f32[0] * v.z + vm.r[3].m128_f32[0];
  const auto vy = vm.r[0].m128_f32[1] * v.x + vm.r[1].m128_f32[1] * v.y + vm.r[2].m128_f32[1] * v.z + vm.r[3].m128_f32[1];
  const auto vw = vm.r[0].m128_f32[3] * v.x + vm.r[1].m128_f32[3] * v.y + vm.r[2].m128_f32[3] * v.z + vm.r[3].m128_f32[3];
  if (vw < 0.0001f) {
    return std::nullopt;
  }
  // clang-format on
  const auto cx = sw / 2.0f;
  const auto cy = sh / 2.0f;
  const auto sx = cx + cx * vx / vw;
  const auto sy = cy - cy * vy / vw;
  if (sx < 0 || sy < 0 || sx >= sw || sy >= sh) {
    return std::nullopt;
  }
  return DirectX::XMFLOAT2{ sx, sy };
}

}  // namespace epos::overwatch