#include "game.hpp"
#include <epos/runtime.hpp>
#include <boost/describe/enumerators.hpp>
#include <boost/mp11/algorithm.hpp>
#include <cassert>

namespace epos::overwatch::game {

boost::static_wstring<16> hero_name(game::hero hero)
{
  boost::static_wstring<16> name{ L"unknown" };
  boost::mp11::mp_for_each<boost::describe::describe_enumerators<game::hero>>([&](auto description) {
    if (hero == description.value) {
      name.resize(std::min(std::strlen(description.name), name.capacity()));
      std::copy(description.name, description.name + name.size(), name.data());
    }
  });
  return name;
}

game::target entity::target() const noexcept
{
  const auto w = p1.x - p0.x;
  const auto h = p1.y - p0.y;

  bool tank = false;
  auto ratio = 0.30f;
  XMFLOAT3 top{ (p0.x + p1.x) / 2.0f, p1.y, (p0.z + p1.z) / 2.0f };
  XMFLOAT3 mid{ (p0.x + p1.x) / 2.0f, (p0.y + p1.y) / 2.0f, (p0.z + p1.z) / 2.0f };
  if (h < 1.10f) {
    // 1.05 | 1.00
    // 0.25 - 0.45 TORBJÖRN
    top.y -= h * 0.38f;
    mid.y -= h * 0.40f;
    ratio = 0.60f;
  } else if (h < 1.40f) {
    // 1.30 | 1.00
    // 0.00 - 0.20 BRIGITTE
    top.y -= h * 0.06f;
    ratio = 0.40f;
  } else if (h < 1.49f) {
    // 1.47 | 1.40
    // 0.15 - 0.45 WRECKING BALL
    top.y -= h * 0.25f;
    mid.y -= h * 0.39f;
    ratio = 0.35f;
    tank = true;
  } else if (h < 1.55f && w > 1.30f) {
    // 1.50 | 1.40
    // 0.15 - 0.35 D.VA
    top.y -= h * 0.16f;
    mid.y += h * 0.05f;
    ratio = 1.20f;
    tank = true;
  } else if (h < 1.55f) {
    // 1.50 | 1.00
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
    // 0.00 - 0.15 BOT
    mid.y -= h * 0.10f;
    mid.y += h * 0.20f;
  } else if (h < 1.70f) {
    // 1.60 | 1.00
    // 0.00 - 0.15 RAMATTRA
    top.y += h * 0.10f;
    mid.y += h * 0.10f;
    tank = true;
  } else if (h < 1.90f) {
    // 1.80 | 1.40
    // 0.25 - 0.35 DOOMFIST
    // 0.00 - 0.25 ORISA
    // 0.00 - 0.30 REINHARDT
    // 0.10 - 0.30 ROADHOG
    // 0.10 - 0.25 BASTION
    mid.y += h * 0.10f;
    ratio = 0.60f;
    tank = true;
  } else {
    // 2.00 | 1.40
    // 0.00 - 0.15 SIGMA
    // 0.35 - 0.35 WINSTON
    ratio = 0.50f;
    tank = true;
  }
  return { XMLoadFloat3(&top), XMLoadFloat3(&mid), {}, w / h * ratio, team, live == 0x14, tank };
}

const qis::signature entity_signature = []() noexcept {
  qis::signature signature{
    "?? ?? 00 00 ?? ?? 00 00 ?? 00 00 00 ?? 00 00 00 00 00 FF FF 02 00 01 00 00 00 00 00 ?? ?? ??"
  };
  assert(signature.size() == static_cast<std::size_t>(entity_signature_size));
  return signature;
}();

XMVECTOR camera(const XMMATRIX& vm) noexcept
{
  const auto vi = XMMatrixInverse(nullptr, vm);
  const auto vw = XMVectorGetW(vi.r[2]);
  if (vw < 0.0001f) {
    return XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
  }
  const auto x = XMVectorGetX(vi.r[2]) / vw;
  const auto y = XMVectorGetY(vi.r[2]) / vw;
  const auto z = XMVectorGetZ(vi.r[2]) / vw;
  return XMVectorSet(x, y, z, 0.0f);
}

XMMATRIX translate(XMMATRIX vm, XMVECTOR v) noexcept
{
  vm = XMMatrixInverse(nullptr, vm);
  vm.r[2] += v * XMVectorGetW(vm.r[2]);
  return XMMatrixInverse(nullptr, vm);
}

std::optional<XMFLOAT2> project(const XMMATRIX& vm, XMVECTOR v, int sw, int sh) noexcept
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

// Checks if the view matrix changed.
__forceinline bool changed(const XMMATRIX& a, const XMMATRIX& b) noexcept
{
  return std::memcmp(&a, &b, sizeof(a)) != 0;
}

// Checks if the position vector changed.
__forceinline bool changed(const XMVECTOR& a, const XMVECTOR& b) noexcept
{
  return std::memcmp(&a, &b, sizeof(a)) != 0;
}

const scene& record::update(
  clock::time_point tp,
  std::int32_t mx,
  std::int32_t my,
  const XMMATRIX& vm,
  std::span<const entity> entities) noexcept
{
  // Reset current scene.
  scene_.tp = tp;
  scene_.mx = mx;
  scene_.my = my;
  scene_.vm = vm;
  scene_.camera = camera(vm);
  scene_.movement = {};
  scene_.targets.clear();
  for (const auto& e : entities) {
    scene_.targets.push_back(e.target());
  }

  // First scene after scene_ with a different view matrix.
  const scene* s0 = nullptr;

  // First scene after s0 with a different view matrix.
  const scene* s1 = nullptr;

  // First scene for each target after scene_ with a different mid position.
  std::array<const scene*, game::entities> t0{};

  // First scene for each target after t0 with a different mid position.
  std::array<const scene*, game::entities> t1{};

  // How long to look into the past for movement changes.
  const auto limit = tp - 64ms;

  // How long to look into the past for movement accumulation.
  const auto total = tp - 16ms;

  // Accumulated camera movement.
  XMVECTOR total_movement{};

  // Accumulated target movement.
  std::array<XMVECTOR, game::entities> total_targets_movement{};

  // Accumulated movement count.
  std::size_t total_count = 1;

  // Set s0, s1, t0 and t1.
  for (const auto& e : scenes_) {
    // Stop inspecting history if the time limit was reached.
    if (e.tp < limit) {
      break;
    }

    // Stop accumulating movement if the time limit was reached.
    const auto accumulate_movement = e.tp > total;

    // Check if the view matrix changed.
    if (!s0) {
      if (changed(vm, e.vm)) {
        s0 = &e;
      } else {
        mx += e.mx;
        my += e.my;
      }
    } else if (!s1) {
      if (changed(s0->vm, e.vm)) {
        s1 = &e;
      }
    } else {
      if (changed(s1->vm, e.vm)) {
        s1 = &e;
      }
    }

    if (accumulate_movement) {
      total_movement += e.movement;
      total_count++;
    }

    // Check if the target positions changed.
    for (std::size_t i = 0; i < entities.size(); i++) {
      const auto& targets = e.targets[i];
      if (!t0[i]) {
        if (changed(scene_.targets[i].mid, targets.mid)) {
          t0[i] = &e;
        }
      } else if (!t1[i]) {
        if (changed(t0[i]->targets[i].mid, targets.mid)) {
          t1[i] = &e;
        }
      } else {
        if (changed(t1[i]->targets[i].mid, targets.mid)) {
          t1[i] = &e;
        }
      }
      if (accumulate_movement) {
        total_targets_movement[i] += targets.movement;
      }
    }
  }

  // Set camera movement.
  if (s0 && s1) {
    if (const auto duration = s0->tp - s1->tp; duration > 1ms) {
      scene_.movement = (s0->camera - s1->camera) / duration_cast<seconds>(duration).count();
    }
  }

  // Set target movement.
  for (std::size_t i = 0; i < entities.size(); i++) {
    if (t0[i] && t1[i]) {
      if (const auto duration = t0[i]->tp - t1[i]->tp; duration > 1ms) {
        const auto movement = t0[i]->targets[i].mid - t1[i]->targets[i].mid;
        scene_.targets[i].movement = movement / duration_cast<seconds>(duration).count();
      }
    }
  }

  // Record current scene with original mouse and movement values.
  scenes_.push_front(scene_);

  // Update scene mx and my values to mouse movement since
  // last view matrix change instead of last scene update.
  scene_.mx = mx;
  scene_.my = my;

  // Smooth out camera movement.
  scene_.movement = (scene_.movement + total_movement) / total_count;

  // Smooth out target movement.
  for (std::size_t i = 0; i < entities.size(); i++) {
    auto& movement = scene_.targets[i].movement;
    movement = (movement + total_targets_movement[i]) / total_count;
  }

  // Return constant reference to current scene with updated mx and my values.
  return scene_;
}

}  // namespace epos::overwatch::game