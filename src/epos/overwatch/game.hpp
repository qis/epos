#pragma once
#include <epos/clock.hpp>
#include <DirectXMath.h>
#include <qis/signature.hpp>
#include <d3d11.h>
#include <d3d9types.h>
#include <optional>
#include <cassert>
#include <cstddef>

#define EPOS_OVERWATCH_UNKNOWN 0

namespace epos::overwatch::game {

constexpr std::intptr_t vm{ 0x4172EA8 };
constexpr std::intptr_t vm_offset{ 0x7E0 };

constexpr std::intptr_t entity_region_size{ 0x180000 };
constexpr std::intptr_t entity_signature_size{ 31 };
constexpr std::intptr_t entities = 255;

using namespace DirectX;

#pragma pack(push, 1)

enum class team : BYTE {
  one = 0x08,
  two = 0x10,
};

struct entity {
  XMFLOAT3 p0{};
  std::array<std::byte, 4> unknown0{};
  XMFLOAT3 p1{};
  std::array<std::byte, 124> unknown1{};
  team team{};
  std::array<std::byte, 2> unknown2{};
  BYTE live{};
#if EPOS_OVERWATCH_UNKNOWN
  std::array<std::byte, entity_signature_size> signature{};
  std::array<std::uint32_t, 8> unknown3{};
#endif

  XMVECTOR top() const noexcept
  {
    return XMVectorSet((p0.x + p1.x) / 2.0f, p1.y, (p0.z + p1.z) / 2.0f, 0.0f);
  }

  XMVECTOR mid() const noexcept
  {
    return XMVectorSet((p0.x + p1.x) / 2.0f, (p0.y + p1.y) / 2.0f, (p0.z + p1.z) / 2.0f, 0.0f);
  }

  XMVECTOR bottom() const noexcept
  {
    return XMVectorSet((p0.x + p1.x) / 2.0f, p0.y, (p0.z + p1.z) / 2.0f, 0.0f);
  }

  XMVECTOR head() const noexcept
  {
    XMFLOAT3 point{};
    XMStoreFloat3(&point, top());
    const auto eh = height();
    if (eh < 1.10f) {
      // 1.05 | 1.00 -> 0.40
      // 0.25 - 0.45 TORBJÖRN
      point.y -= eh * 0.40f;
    } else if (eh < 1.40f) {
      // 1.30 | 1.00 -> 0.15
      // 0.00 - 0.20 BRIGITTE
      point.y -= eh * 0.15f;
    } else if (eh < 1.49f) {
      // 1.47 | 1.40 -> 0.30
      // 0.15 - 0.45 WRECKING BALL
      point.y -= eh * 0.30f;
    } else if (eh < 1.55f && width() > 1.30f) {
      // 1.50 | 1.40 -> 0.25
      // 0.15 - 0.35 D.VA
      point.y -= eh * 0.25f;
    } else if (eh < 1.55f) {
      // 1.50 | 1.00 -> 0.50
      // 0.00 - 0.15 JUNKER QUEEN
      // 0.00 - 0.20 ZARYA
      // 0.10 - 0.30 ASHE
      // 0.10 - 0.30 CASSIDY
      // 0.10 - 0.30 D.VA (DEMECH)
      // 0.00 - 0.10 ECHO
      // 0.35 - 0.50 GENJI
      // 0.35 - 0.40 HANZO
      // 0.35 - 0.50 JUNKRAT
      // 0.25 - 0.40 MEI
      // 0.20 - 0.30 PHARAH
      // 0.10 - 0.30 REAPER
      // 0.30 - 0.50 SOJOURN
      // 0.10 - 0.25 SOLDIER: 76
      // 0.25 - 0.50 SOMBRA
      // 0.25 - 0.40 SYMMETRA
      // 0.25 - 0.50 TRACER
      // 0.10 - 0.30 WIDOWMAKER
      // 0.30 - 0.50 ANA
      // 0.20 - 0.35 BAPTISTE
      // 0.20 - 0.40 KIRIKO
      // 0.30 - 0.50 LÚCIO
      // 0.15 - 0.35 MERCY
      // 0.00 - 0.20 MOIRA
      // 0.10 - 0.30 ZENYATTA
      // 0.00 - 0.15 (BOT)
      point.y -= eh * 0.50f;
    } else if (eh < 1.70f) {
      // 1.60 | 1.00 -> 0.10
      // 0.00 - 0.15 RAMATTRA
      point.y -= eh * 0.10f;
    } else if (eh < 1.90f) {
      // 1.80 | 1.40 -> 0.25
      // 0.25 - 0.35 DOOMFIST
      // 0.00 - 0.25 ORISA
      // 0.00 - 0.30 REINHARDT
      // 0.10 - 0.30 ROADHOG
      // 0.10 - 0.25 BASTION
      point.y -= eh * 0.25f;
    } else {
      // 2.00 | 1.40 -> 0.15 - 0.35
      // 0.00 - 0.15 SIGMA
      // 0.35 - 0.35 WINSTON
      point.y -= eh * 0.25f;
    }
    return XMVectorSet(point.x, point.y, point.z, 0.0f);
  }

  constexpr float height() const noexcept
  {
    return p1.y - p0.y;
  }

  constexpr float width() const noexcept
  {
    return p1.x - p0.x;
  }

  constexpr operator bool() const noexcept
  {
    return live == 0x14;
  }
};

#if EPOS_OVERWATCH_UNKNOWN
static_assert(offsetof(entity, signature) - offsetof(entity, p0) == 0x9C);
#else
static_assert(sizeof(entity) == 0x9C);
#endif

#pragma pack(pop)

inline const auto entity_signature = []() noexcept {
  qis::signature signature{
    "?? ?? 00 00 ?? ?? 00 00 ?? 00 00 00 ?? 00 00 00 00 00 FF FF 02 00 01 00 00 00 00 00 ?? ?? ??"
  };
  assert(signature.size() == static_cast<std::size_t>(entity_signature_size));
  return signature;
}();

inline std::optional<XMFLOAT2> project(const XMMATRIX& vm, XMVECTOR v, int sw, int sh) noexcept
{
  const auto x = XMVectorGetX(v);
  const auto y = XMVectorGetY(v);
  const auto z = XMVectorGetZ(v);
  // clang-format off
#ifdef _XM_NO_INTRINSICS_
  const auto vx = vm._11 * x + vm._21 * y + vm._31 * z + vm._41;
  const auto vy = vm._12 * x + vm._22 * y + vm._32 * z + vm._42;
  const auto vw = vm._14 * x + vm._24 * y + vm._34 * z + vm._44;
#else
  const auto vx = vm.r[0].m128_f32[0] * x + vm.r[1].m128_f32[0] * y + vm.r[2].m128_f32[0] * z + vm.r[3].m128_f32[0];
  const auto vy = vm.r[0].m128_f32[1] * x + vm.r[1].m128_f32[1] * y + vm.r[2].m128_f32[1] * z + vm.r[3].m128_f32[1];
  const auto vw = vm.r[0].m128_f32[3] * x + vm.r[1].m128_f32[3] * y + vm.r[2].m128_f32[3] * z + vm.r[3].m128_f32[3];
#endif
  // clang-format on
  if (vw < 0.0001f) {
    return std::nullopt;
  }
  const auto cx = sw / 2.0f;
  const auto cy = sh / 2.0f;
  const auto sx = cx + cx * vx / vw;
  const auto sy = cy - cy * vy / vw;
  if (sx < 0 || sy < -sh || sx >= sw || sy >= sh * 1.5f) {
    return std::nullopt;
  }
  return XMFLOAT2{ sx, sy };
}

inline XMVECTOR camera(const XMMATRIX& vm) noexcept
{
  const auto v = XMMatrixInverse(nullptr, vm);
  const auto w = XMVectorGetW(v.r[2]);
  if (w < 0.0001f) {
    return XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
  }
  const auto x = XMVectorGetX(v.r[2]);
  const auto y = XMVectorGetY(v.r[2]);
  const auto z = XMVectorGetZ(v.r[2]);
  return XMVectorSet(x / w, y / w, z / w, 0.0f);
}

// Arrow drop per meter.
inline const XMVECTOR arrow_drop{ XMVectorSet(0.0f, 0.019f, 0.0f, 0.0f) };

// How many seconds it takes an arrow to travel a meter.
constexpr float arrow_speed{ 0.01f };

}  // namespace epos::overwatch::game