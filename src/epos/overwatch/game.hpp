#pragma once
#include <DirectXMath.h>
#include <qis/signature.hpp>
#include <d3d11.h>
#include <d3d9types.h>
#include <optional>
#include <cassert>
#include <cstddef>

namespace epos::overwatch::game {

constexpr std::intptr_t vm{ 0x4172EA8 };
constexpr std::intptr_t vm_offset{ 0x7E0 };

constexpr std::intptr_t entity_region_size{ 0x180000 };
constexpr std::intptr_t entity_signature_size{ 31 };
constexpr std::intptr_t entities = 255;

#pragma pack(push, 1)

enum class team : BYTE {
  one = 0x08,
  two = 0x10,
};

struct alignas(1) entity {
  DirectX::XMFLOAT3 head{};
  std::array<std::byte, 140> unknown0{};
  team team{ 0 };
  std::array<std::byte, 2> unknown1{};
  BYTE live{ 0 };

  constexpr operator bool() const noexcept
  {
    return live == 0x14;
  }
};

static_assert(sizeof(entity) == 0x9C);

#pragma pack(pop)

inline const auto entity_signature = []() noexcept {
  qis::signature signature{
    "?? ?? 00 00 ?? ?? 00 00 ?? 00 00 00 ?? 00 00 00 00 00 FF FF 02 00 01 00 00 00 00 00 ?? ?? ??"
  };
  assert(signature.size() == static_cast<std::size_t>(entity_signature_size));
  return signature;
}();

inline std::optional<DirectX::XMFLOAT2> project(
  const DirectX::XMMATRIX& vm,
  const DirectX::XMFLOAT3& v,
  int sw,
  int sh) noexcept
{
  // clang-format off
#ifdef _XM_NO_INTRINSICS_
  const auto vx = (vm._11 * v.x) + (vm._21 * v.y) + (vm._31 * v.z + vm._41);
  const auto vy = (vm._12 * v.x) + (vm._22 * v.y) + (vm._32 * v.z + vm._42);
  const auto vw = (vm._14 * v.x) + (vm._24 * v.y) + (vm._34 * v.z + vm._44);
#else
  const auto vx = vm.r[0].m128_f32[0] * v.x + vm.r[1].m128_f32[0] * v.y + vm.r[2].m128_f32[0] * v.z + vm.r[3].m128_f32[0];
  const auto vy = vm.r[0].m128_f32[1] * v.x + vm.r[1].m128_f32[1] * v.y + vm.r[2].m128_f32[1] * v.z + vm.r[3].m128_f32[1];
  const auto vw = vm.r[0].m128_f32[3] * v.x + vm.r[1].m128_f32[3] * v.y + vm.r[2].m128_f32[3] * v.z + vm.r[3].m128_f32[3];
#endif
  // clang-format on
  if (vw < 0.0001f) {
    return std::nullopt;
  }
  const auto cx = sw / 2.0f;
  const auto cy = sh / 2.0f;
  const auto sx = cx + cx * vx / vw;
  const auto sy = cy - cy * vy / vw;
  if (sx < 0 || sy < 0 || sx >= sw || sy >= sh) {
    return std::nullopt;
  }
  return DirectX::XMFLOAT2{ sx, sy };
}

}  // namespace epos::overwatch::game