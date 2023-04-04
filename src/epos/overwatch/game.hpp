#pragma once
#include <epos/clock.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/container/static_vector.hpp>
#include <boost/describe/enum.hpp>
#include <boost/static_string.hpp>
#include <qis/signature.hpp>
#include <windows.h>
#include <optional>
#include <span>
#include <cstddef>
#include <cstdint>

#include <DirectXMath.h>

#define EPOS_OVERWATCH_UNKNOWN 0

namespace epos::overwatch::game {

constexpr std::intptr_t vm{ 0x4172EA8 };
constexpr std::intptr_t vm_offset{ 0x7E0 };

constexpr std::intptr_t entity_region_size{ 0x180000 };
constexpr std::intptr_t entity_signature_size{ 31 };
constexpr std::intptr_t entities = 1024;

using namespace DirectX;

enum class team : BYTE {
  one = 0x08,
  two = 0x10,
};

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

struct target {
  /// Collision box top point.
  XMVECTOR top;

  /// Collision box mid point.
  XMVECTOR mid;

  /// Movement in meters per second.
  XMVECTOR movement;

  /// Collusion box width to height ratio.
  float ratio;

  /// Entity team.
  team team;

  /// Entity can be targeted.
  bool live;

  /// Entity is a likely tank.
  bool tank;
};

#pragma pack(push, 1)

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
  game::target target() const noexcept;
};

#pragma pack(pop)

#if EPOS_OVERWATCH_UNKNOWN
constexpr auto entity_signature_offset = offsetof(entity, signature);
static_assert(entity_signature_offset - offsetof(entity, p0) == 0x9C);
#else
constexpr auto entity_signature_offset = sizeof(entity);
static_assert(entity_signature_offset == 0x9C);
#endif

extern const qis::signature entity_signature;

XMVECTOR camera(const XMMATRIX& vm) noexcept;
XMMATRIX translate(XMMATRIX vm, XMVECTOR v) noexcept;
std::optional<XMFLOAT2> project(const XMMATRIX& vm, XMVECTOR v, int sw, int sh) noexcept;

struct scene {
  /// Scene time point.
  clock::time_point tp;

  /// Horizontal mouse movement since last @ref vm change.
  std::int32_t mx;

  /// Vertical mouse movement since last @ref vm change.
  std::int32_t my;

  /// View matrix.
  XMMATRIX vm;

  /// Camera position.
  XMVECTOR camera;

  /// Camera movement in meters per second.
  XMVECTOR movement;

  /// Scene targets.
  boost::container::static_vector<target, entities> targets;
};

class record {
public:
  const scene& update(
    clock::time_point tp,
    std::int32_t mx,
    std::int32_t my,
    const XMMATRIX& vm,
    std::span<const entity> entities) noexcept;

  void clear() noexcept
  {
    scenes_.clear();
  }

private:
  scene scene_;
  boost::circular_buffer<scene> scenes_{ 1024 };
};

}  // namespace epos::overwatch::game
