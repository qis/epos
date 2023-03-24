#pragma once
#include <boost/describe/enum.hpp>
#include <boost/static_string.hpp>
#include <qis/signature.hpp>
#include <windows.h>
#include <optional>
#include <cstddef>

#include <DirectXMath.h>

#define EPOS_OVERWATCH_UNKNOWN 0

namespace epos::overwatch::game {

constexpr std::intptr_t vm{ 0x4172EA8 };
constexpr std::intptr_t vm_offset{ 0x7E0 };

constexpr std::intptr_t entity_region_size{ 0x180000 };
constexpr std::intptr_t entity_signature_size{ 31 };
constexpr std::intptr_t entities = 255;

using namespace DirectX;

struct target {
  XMVECTOR top{};
  XMVECTOR mid{};
  float ratio{ 1.0f };
  bool tank{ false };
};

#pragma pack(push, 1)

BOOST_DEFINE_FIXED_ENUM_CLASS(
  hero,
  BYTE,
  none,
  doomfist,
  dva,
  dva_demech,
  junker_queen,
  orisa,
  ramattra,
  reinhardt,
  roadhog,
  sigma,
  winston,
  wrecking_ball,
  zarya,
  ashe,
  bastion,
  cassidy,
  echo,
  genji,
  hanzo,
  junkrat,
  mei,
  pharah,
  reaper,
  sojourn,
  soldier_76,
  sombra,
  symmetra,
  torbjorn,
  tracer,
  widowmaker,
  ana,
  baptiste,
  brigitte,
  kiriko,
  lucio,
  mercy,
  moira,
  zenyatta,
  bot);

boost::static_wstring<16> hero_name(game::hero hero);

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

  constexpr operator bool() const noexcept
  {
    return live == 0x14;
  }

  constexpr float width() const noexcept
  {
    return p1.x - p0.x;
  }

  constexpr float height() const noexcept
  {
    return p1.y - p0.y;
  }

  game::target target() const noexcept;
};

#if EPOS_OVERWATCH_UNKNOWN
static_assert(offsetof(entity, signature) - offsetof(entity, p0) == 0x9C);
#else
static_assert(sizeof(entity) == 0x9C);
#endif

#pragma pack(pop)

extern const qis::signature entity_signature;

std::optional<XMFLOAT2> project(const XMMATRIX& vm, XMVECTOR v, int sw, int sh) noexcept;
XMVECTOR camera(const XMMATRIX& vm) noexcept;

// Arrow drop per meter.
inline const XMVECTOR arrow_drop{ XMVectorSet(0.0f, 0.019f, 0.0f, 0.0f) };

// How many seconds it takes an arrow to travel a meter.
constexpr float arrow_speed{ 0.01f };

}  // namespace epos::overwatch::game
