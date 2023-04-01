#include "view.hpp"
#include <cmath>

namespace epos::overwatch {
namespace {

constexpr float offset(std::int32_t m, float sensitivity) noexcept
{
  return -(m * sensitivity / 11.7647f);
}

constexpr float offset(std::int32_t m, float sensitivity, float relative) noexcept
{
  return -(m * sensitivity * relative / 4.3478f);
}

}  // namespace

void view::reaper(clock::time_point tp, const epos::input::state& state, const game::scene& scene) noexcept
{
  // Prediction frames per second.
  static constexpr auto fps = 60.0f;

  // Mouse sensitivity in percent.
  static constexpr auto sensitivity = 5.0f;

  // Fire lockout duration.
  static constexpr auto fire_lockout = 128ms;

  // Power lockout duration.
  static constexpr auto power_lockout = 1280ms;

  // Melee lockout duration.
  static constexpr auto melee_lockout = 2000ms;

  // Trigger distance in meters.
  static constexpr auto trigger = 16.0f;

  // Weapon spread ellipse.
  static const D2D1_ELLIPSE spread{ D2D1::Ellipse(D2D1::Point2F(sx + sc.x, sy + sc.y), 80.0f, 80.0f) };

  // Handle input.
  const auto ox = offset(scene.mx, sensitivity);
  const auto oy = offset(scene.my, sensitivity);

  if (state.pressed(key::e) || state.pressed(key::shift)) {
    melee_lockout_ = tp + melee_lockout;
    fire_lockout_ = tp + power_lockout;
  }

  if (state.down(button::left)) {
    fire_lockout_ = tp + fire_lockout;
  }

  if (state.pressed(key::c)) {
    melee_lockout_ = tp + melee_lockout;
  }

  // Inject scheduled melee attack.
  if (melee_ && tp > *melee_) {
    input_.mask(button::down, 16ms);
    melee_ = std::nullopt;
  }

  // Predict camera movement for next frame.
  const auto camera = scene.camera + scene.movement / fps;

  // Translate view matrix based on predicted camera movement for next frame.
  const auto vm = game::translate(scene.vm, scene.movement / fps);

  // Draw targets.
  auto fire = false;
  auto melee = false;
  for (const auto& target : scene.targets) {
    // Filter out friendly and dead targets.
    if (!target.live || target.team != team_) {
      continue;
    }

    // Get distance to camera in meters.
    auto m = XMVectorGetX(XMVector3Length(camera - target.mid));
    if (m < 0.9f) {
      continue;
    }

    // Project target points.
    const auto top = game::project(vm, target.top + target.movement / fps, sw, sh);
    if (!top) {
      continue;
    }
    const auto mid = game::project(vm, target.mid + target.movement / fps, sw, sh);
    if (!mid) {
      continue;
    }

    // Get target brushes.
    const auto& target_brushes = target.tank ? brushes_.tank : brushes_.target;

    // Draw target.
    const auto x0 = mid->x + ox;
    const auto y0 = mid->y + oy;
    const auto r1 = mid->y - top->y;
    const auto r0 = r1 * std::min(target.ratio * 2.0f, 1.0f);
    const auto e0 = D2D1::Ellipse(D2D1::Point2F(sx + x0, sy + y0), r0, r1);
    const auto e1 = D2D1::Ellipse(D2D1::Point2F(sx + x0, sy + y0), r0 + 1.5f, r1 + 1.5f);

    if (m < trigger) {
      dc_->FillEllipse(e0, target_brushes[1].Get());
      dc_->DrawEllipse(e0, target_brushes[2].Get(), 2.0f);
      dc_->DrawEllipse(e1, brushes_.frame.Get());
    } else if (m < trigger * 1.2f) {
      dc_->DrawEllipse(e0, target_brushes[3].Get());
    } else if (m < trigger * 1.4f) {
      dc_->DrawEllipse(e0, target_brushes[4].Get());
    } else if (m < trigger * 1.6f) {
      dc_->DrawEllipse(e0, target_brushes[5].Get());
    } else if (m < trigger * 1.8f) {
      dc_->DrawEllipse(e0, target_brushes[6].Get());
    } else if (m < trigger * 2.0f) {
      dc_->DrawEllipse(e0, target_brushes[7].Get());
    } else {
      dc_->DrawEllipse(e0, target_brushes[8].Get());
    }

    // Create target label.
#ifndef NDEBUG
    string_.reset(L"{:.0f}", m);
    string_label(sx + x0, sy + y0, 32, 32, formats_.label, target_brushes[0]);
#endif

    // Check if the target is in trigger range.
    if (m > trigger) {
      continue;
    }

    // Check if crosshair is inside ellipse or ellipse is inside spread.
    const auto o0 = sc.x - mid->x;
    const auto o1 = sc.y - mid->y;
    const auto r2 = std::pow(r0, 2.0f);
    const auto r3 = std::pow(r1, 2.0f);
    const auto rs = spread.radiusX;
    for (auto multiplier = 1.0f; multiplier < 1.5f; multiplier += 0.1f) {
      const auto ex = std::pow(o0 + scene.mx * multiplier, 2.0f) / r2;
      const auto ey = std::pow(o1 + scene.my * multiplier, 2.0f) / r3;
      if (ex + ey < 1.0f || (ey < 1.0f && x0 - r0 > sc.x - rs && x0 + r0 < sc.x + rs)) {
        if (m < 1.3f || state.down(button::right)) {
          fire = true;
        }
        if (!target.tank && (m < 1.3f || (m < 2.5f && state.down(button::right)))) {
          melee = true;
        }
        break;
      }
    }
  }

  // Inject fire and schedule melee attack.
  if (fire || melee) {
    if (fire && tp > fire_lockout_) {
      input_.mask(button::up, 16ms);
      fire_lockout_ = tp + fire_lockout;
    }
    if (melee && tp > melee_lockout_) {
      melee_ = tp + 32ms;
      melee_lockout_ = tp + melee_lockout;
    }
  }

  // Draw crosshair.
  const auto x = sx + sc.x;
  const auto y = sy + sc.y;
  const auto r = 3.0f;
  const auto e = D2D1::Ellipse(D2D1::Point2F(x, y), r, r);
  dc_->FillEllipse(e, state.down(button::right) ? brushes_.blue.Get() : brushes_.white.Get());
  dc_->DrawEllipse(e, brushes_.black.Get());

  // Draw weapon spread.
  dc_->DrawEllipse(spread, brushes_.spread.Get());
}

void view::widowmaker(clock::time_point tp, const epos::input::state& state, const game::scene& scene) noexcept
{
  // Prediction frames per second.
  static constexpr auto fps = 60.0f;

  // Mouse sensitivity in percent.
  static constexpr auto sensitivity = 5.0f;

  // Relative aim sensitivity factor while zoomed.
  static constexpr auto relative = 0.2f;

  // Fire lockout duration.
  static constexpr auto fire_lockout = 1100ms;

  // Melee lockout duration.
  static constexpr auto melee_lockout = 2000ms;

  // Handle input.
  const auto zoom = state.down(button::right);
  const auto trigger = state.down(button::left);
  const auto ox = zoom ? offset(scene.mx, sensitivity) : offset(scene.mx, sensitivity, relative);
  const auto oy = zoom ? offset(scene.my, sensitivity) : offset(scene.my, sensitivity, relative);

  if (state.pressed(button::right)) {
    fire_lockout_ = tp + fire_lockout;
  }

  if (state.up(button::right) && state.down(button::left) && tp > fire_lockout_) {
    input_.mask(button::up, 128ms);
    fire_lockout_ = tp + 32ms;
  }

  // Inject scheduled melee attack.
  if (melee_ && tp > *melee_) {
    input_.mask(button::down, 16ms);
    melee_ = std::nullopt;
  }

  // Predict camera movement for next frame.
  const auto camera = scene.camera + scene.movement / fps;

  // Translate view matrix based on predicted camera movement for next frame.
  const auto vm = game::translate(scene.vm, scene.movement / fps);

  // Draw targets.
  auto fire = false;
  auto melee = false;
  for (const auto& target : scene.targets) {
    // Filter out friendly and dead targets.
    if (!target.live || target.team != team_) {
      continue;
    }

    // Get distance to camera in meters.
    auto m = XMVectorGetX(XMVector3Length(camera - target.mid));
    if (m < 0.9f) {
      continue;
    }

    // Project target points.
    const auto top = game::project(vm, target.top + target.movement / fps, sw, sh);
    if (!top) {
      continue;
    }
    const auto mid = game::project(vm, target.mid + target.movement / fps, sw, sh);
    if (!mid) {
      continue;
    }

    // Get target brushes.
    const auto& target_brushes = target.tank ? brushes_.tank : brushes_.target;

    // Draw target.
    const auto x0 = mid->x + ox;
    const auto y0 = mid->y + oy;
    const auto r1 = mid->y - top->y;
    const auto r0 = r1 * target.ratio;
    const auto e0 = D2D1::Ellipse(D2D1::Point2F(sx + x0, sy + y0), r0, r1);
    const auto e1 = D2D1::Ellipse(D2D1::Point2F(sx + x0, sy + y0), r0 + 1.5f, r1 + 1.5f);

    dc_->FillEllipse(e0, target_brushes[1].Get());
    dc_->DrawEllipse(e0, target_brushes[2].Get(), 2.0f);
    dc_->DrawEllipse(e1, brushes_.frame.Get());

    // Create target label.
#ifndef NDEBUG
    string_.reset(L"{:.0f}", m);
    string_label(sx + x0, sy + y0, 32, 32, formats_.label, target_brushes[0]);
#endif

    // Check if crosshair is inside ellipse.
    const auto o0 = sc.x - mid->x;
    const auto o1 = sc.y - mid->y;
    const auto r2 = std::pow(r0, 2.0f);
    const auto r3 = std::pow(r1, 2.0f);
    for (auto multiplier = 1.0f; multiplier < 1.2f; multiplier += 0.1f) {
      const auto ex = std::pow(o0 + scene.mx * multiplier, 2.0f) / r2;
      const auto ey = std::pow(o1 + scene.my * multiplier, 2.0f) / r3;
      if (ex + ey < 1.0f) {
        if (zoom) {
          fire = true;
        }
        if (m < 2.5f) {
          melee = true;
        }
        break;
      }
    }
  }

  // Inject fire and schedule melee attack.
  if (fire || melee) {
    if ((melee || (fire && zoom && trigger)) && tp > fire_lockout_) {
      input_.mask(button::up, 16ms);
      fire_lockout_ = tp + fire_lockout;
    }
    if (melee && tp > melee_lockout_) {
      melee_ = tp + 32ms;
      melee_lockout_ = tp + melee_lockout;
    }
  }

  // Draw crosshair.
  const auto x = sx + sc.x;
  const auto y = sy + sc.y;
  const auto r = 3.0f;
  const auto e = D2D1::Ellipse(D2D1::Point2F(x, y), r, r);
  dc_->FillEllipse(e, brushes_.white.Get());
  dc_->DrawEllipse(e, brushes_.black.Get());
}

}  // namespace epos::overwatch