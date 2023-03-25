#include "view.hpp"
#include <cmath>

namespace epos::overwatch {
namespace {

static const auto movement_scale = XMVectorSet(1.0f, 0.6f, 1.0f, 1.0f);

}  // namespace

void view::reaper(clock::time_point tp, const epos::input::state& state, const XMFLOAT2& mouse) noexcept
{
  // Lockout duration on scope in and primary fire.
  static constexpr auto lockout = 128ms;

  // Weapon spread.
  static const D2D1_ELLIPSE spread{ D2D1::Ellipse(D2D1::Point2F(sx + sc.x, sy + sc.y), 80.0f, 80.0f) };

  // Trigger distance in meters.
  static constexpr auto trigger = 16.0f;

  // Handle input.
  if (state.pressed(key::e) || state.pressed(key::shift)) {
    primary_ = tp + 1280ms;
    melee_ = tp + 1500ms;
  }

  if (state.down(button::left)) {
    primary_ = tp + lockout;
  }

  if (state.pressed(key::c)) {
    melee_ = tp + 1500ms;
  }

  // Get camera position.
  const auto camera = game::camera(vm_);

  // Get update movement time point.
  const auto update_movement = tp > update_movement_;
  update_movement_ = tp + 1ms;

  // Draw entities.
  for (std::size_t i = 0; i < scene_draw_->entities; i++) {
    const auto& e = entities_[i];
    if (!e || e.team == team_) {
      movement_[i].clear();
      continue;
    }

    // Get target.
    auto target = e.target();
    if (update_movement || movement_[i].empty()) {
      movement_[i].push_back({ target.mid, tp });
    }

    // Calculate distance to camera in meters.
    auto m = XMVectorGetX(XMVector3Length(camera - target.mid));
    if (m < 0.9f) {
      continue;
    }

    // Calculate movement vector for next frame.
    const auto mv = [&]() noexcept {
      if (movement_[i].size() > 1) {
        const auto& snapshot = movement_[i].front();
        const auto time_scale = 8.0f / duration_cast<milliseconds>(tp - snapshot.time_point).count();
        return (target.mid - snapshot.target) * movement_scale * time_scale;
      }
      return XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
    }();

    // Project target points.
    const auto top = game::project(vm_, target.top + mv, sw, sh);
    if (!top) {
      continue;
    }
    const auto mid = game::project(vm_, target.mid + mv, sw, sh);
    if (!mid) {
      continue;
    }

    // Draw target.
    const auto x0 = mid->x - mouse.x;
    const auto y0 = mid->y - mouse.y;
    const auto r1 = mid->y - top->y;
    const auto r0 = r1 * std::min(target.ratio * 2.0f, 1.0f);
    const auto e0 = D2D1::Ellipse(D2D1::Point2F(sx + x0, sy + y0), r0, r1);
    const auto e1 = D2D1::Ellipse(D2D1::Point2F(sx + x0, sy + y0), r0 + 1.5f, r1 + 1.5f);

    const auto& target_brushes = target.tank ? brushes_.tank : brushes_.target;
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

    // Check if primary fire or melee is allowed.
    if (m > trigger || state.down(key::control)) {
      continue;
    }

    // Press primary fire button if crosshair is inside ellipse or ellipse is inside spread.
    const auto ex = std::pow((sc.x - mid->x), 2.0f) / std::pow(r0, 2.0f);
    const auto ey = std::pow((sc.y - mid->y), 2.0f) / std::pow(r1, 2.0f);
    const auto rs = spread.radiusX;
    if (ex + ey < 1.0f || (ey < 1.0f && x0 - r0 > sc.x - rs && x0 + r0 < sc.x + rs)) {
      // Inject primary fire.
      if (tp > primary_ && (m < 1.3f || state.down(button::right))) {
        input_.mask(button::up, 16ms);
        primary_ = tp + lockout;
      }
      // Inject melee.
      if (tp > melee_ && (m < 1.3f || (state.down(button::right) && m < 2.5f))) {
        input_.mask(button::down, 16ms, 32ms);
        melee_ = tp + 1500ms;
      }
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

void view::widowmaker(clock::time_point tp0, const epos::input::state& state, const XMFLOAT2& mouse) noexcept
{
  // Lockout duration on scope in and primary fire.
  static constexpr auto lockout = 1100ms;

  // Handle input.
  if (state.pressed(button::right)) {
    primary_ = tp0 + lockout;
  }
  if (state.up(button::right) && state.down(button::left) && tp0 > primary_) {
    input_.mask(button::up, 64ms);
    primary_ = tp0 + 32ms;
  }

  // Get camera position.
  const auto camera = game::camera(vm_);

  // Get update movement time point.
  const auto update_movement = tp0 > update_movement_;
  update_movement_ = tp0 + 1ms;

  // Draw entities.
  auto fire = false;
  for (std::size_t i = 0; i < scene_draw_->entities; i++) {
    const auto& e = entities_[i];
    if (!e || e.team == team_) {
      movement_[i].clear();
      continue;
    }

    // Get target.
    auto target = e.target();
    if (update_movement || movement_[i].empty()) {
      movement_[i].push_back({ target.mid, tp0 });
    }

    // Calculate distance to camera in meters.
    auto m = XMVectorGetX(XMVector3Length(camera - target.mid));
    if (m < 0.9f) {
      continue;
    }

    // Calculate movement vector for next frame.
    const auto mv = [&]() noexcept {
      if (movement_[i].size() > 1) {
        const auto& snapshot = movement_[i].front();
        const auto time_scale = 8.0f / duration_cast<milliseconds>(tp0 - snapshot.time_point).count();
        return (target.mid - snapshot.target) * movement_scale * time_scale;
      }
      return XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
    }();

    // Project target points.
    const auto top = game::project(vm_, target.top + mv, sw, sh);
    if (!top) {
      continue;
    }
    const auto mid = game::project(vm_, target.mid + mv, sw, sh);
    if (!mid) {
      continue;
    }

    // Draw target.
    const auto x0 = mid->x - mouse.x;
    const auto y0 = mid->y - mouse.y;
    const auto r1 = mid->y - top->y;
    const auto r0 = target.ratio * r1;
    const auto e0 = D2D1::Ellipse(D2D1::Point2F(sx + x0, sy + y0), r0, r1);
    const auto e1 = D2D1::Ellipse(D2D1::Point2F(sx + x0, sy + y0), r0 + 1.5f, r1 + 1.5f);

    const auto& target_brushes = target.tank ? brushes_.tank : brushes_.target;
    dc_->FillEllipse(e0, target_brushes[1].Get());
    dc_->DrawEllipse(e0, target_brushes[2].Get(), 2.0f);
    dc_->DrawEllipse(e1, brushes_.frame.Get());

    // Create target label.
#ifndef NDEBUG
    string_.reset(L"{:.0f}", m);
    string_label(sx + x0, sy + y0, 32, 32, formats_.label, target_brushes[0]);
#endif

    // Check if primary fire conditions are met.
    if (fire || tp0 < primary_ || state.up(button::right) || state.up(button::left)) {
      continue;
    }

    // Press primary fire button if crosshair is inside ellipse or crosses the ellipse (lazy).
    const auto o0 = sc.x - mid->x;
    const auto o1 = sc.y - mid->y;
    const auto r2 = std::pow(r0, 2.0f);
    const auto r3 = std::pow(r1, 2.0f);
    for (auto multiplier = 1.0f; multiplier < 6.1f; multiplier += 1.0f) {
      const auto ex = std::pow(o0 + mouse.x * multiplier * 2.0f, 2.0f) / r2;
      const auto ey = std::pow(o0 + mouse.y * multiplier, 2.0f) / r3;
      if (ex + ey < 1.0f) {
        input_.mask(button::up, 16ms);
        primary_ = tp0 + lockout;
        fire = true;
        break;
      }
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

#if 0

void view::hero(clock::time_point tp, const epos::input::state& state, const XMFLOAT2& mouse) noexcept
{
  // Whipshot speed in meters per second.
  static constexpr auto speed = 48.0f;

  // Whipshot range in meters.
  const auto range = [&]() noexcept {
    if (state.down(button::right)) {
      return 22.96f;
    }
    return 20.96f;
  }();

  // Handle input.
  if (state.pressed(key::shift)) {
    lockout_ = tp + 512ms;
  }

  // Get camera position.
  const auto camera = game::camera(vm_);

  // Get update movement time point.
  const auto update_movement = tp > update_movement_;
  update_movement_ = tp + 1ms;

  // Draw entities.
  auto fire = false;
  for (std::size_t i = 0; i < scene_draw_->entities; i++) {
    const auto& e = entities_[i];
    if (!e || e.team == team_) {
      movement_[i].clear();
      continue;
    }

    // Get target.
    auto target = e.mid();
    if (update_movement || movement_[i].empty()) {
      movement_[i].push_back({ target, tp });
    }

    // Calculate distance to camera in meters.
    auto m = XMVectorGetX(XMVector3Length(camera - target));

    // Calculate whipshot travel time in milliseconds.
    auto s = m / speed;

    // Calculate target position offset on whipshot impact.
    if (movement_[i].size() > 1) {
      const auto& snapshot = movement_[i].front();
      const auto scale = s / duration_cast<seconds>(tp - snapshot.time_point).count();
      const auto multiplier = XMVectorSet(scale, scale / 3.0f, scale, 1.0f);
      target += (target - snapshot.target) * multiplier;
      m = XMVectorGetX(XMVector3Length(camera - target));
      s = m / speed;
    }

    // Project target points.
    const auto top = game::project(vm_, e.top(), sw, sh);
    if (!top) {
      continue;
    }
    const auto mid = game::project(vm_, e.mid(), sw, sh);
    if (!mid) {
      continue;
    }
    const auto hit = game::project(vm_, target, sw, sh);
    if (!hit) {
      continue;
    }

    // Create mid ellipse.
    const auto x0 = mid->x - mouse.x;
    const auto y0 = mid->y - mouse.y;
    const auto r0 = 2.0f;
    const auto e0 = D2D1::Ellipse(D2D1::Point2F(sx + x0, sy + y0), r0, r0);

    // Draw inner mid ellipse if not in range.
    if (m > range) {
      if (m < range + 2.5f) {
        dc_->DrawEllipse(e0, brushes_.enemy[2].Get());
      } else if (m < range + 5.0f) {
        dc_->DrawEllipse(e0, brushes_.enemy[3].Get());
      } else if (m < range + 7.5f) {
        dc_->DrawEllipse(e0, brushes_.enemy[4].Get());
      } else if (m < range + 10.0f) {
        dc_->DrawEllipse(e0, brushes_.enemy[5].Get());
      } else if (m < range + 12.5f) {
        dc_->DrawEllipse(e0, brushes_.enemy[6].Get());
      } else if (m < range + 15.0f) {
        dc_->DrawEllipse(e0, brushes_.enemy[7].Get());
      } else {
        dc_->DrawEllipse(e0, brushes_.enemy[8].Get());
      }
      continue;
    }

    // Create hit ellipse.
    const auto x1 = hit->x - mouse.x;
    const auto y1 = hit->y - mouse.y;
    const auto r1 = (mid->y - top->y) * 0.4f;
    const auto e1 = D2D1::Ellipse(D2D1::Point2F(sx + x1, sy + y1), r1, r1);

    // Create mid outline ellipse.
    const auto r2 = r0 + 1.0f;
    const auto e2 = D2D1::Ellipse(D2D1::Point2F(sx + x0, sy + y0), r2, r2);

    // Create hit outline ellipse.
    const auto r3 = r1 + 1.0f;
    const auto e3 = D2D1::Ellipse(D2D1::Point2F(sx + x1, sy + y1), r3, r3);

    // Calculate distance between hit and mid points.
    const auto d0 = std::sqrt(std::pow(x1 - x0, 2.0f) + std::pow(y1 - y0, 2.0f));

    // Create hit to mid connection points.
    const auto p0 = D2D1::Point2F(sx + x0, sy + y0);
    const auto p1 = D2D1::Point2F(sx + x1, sy + y1);
    const auto t0 = r1 / std::max(d0, 1.0f);
    const auto x2 = (1.0f - t0) * x1 + t0 * x0;
    const auto y2 = (1.0f - t0) * y1 + t0 * y0;
    const auto p2 = D2D1::Point2F(sx + x2, sy + y2);

    // Draw hit background and outline.
    dc_->FillEllipse(e3, brushes_.enemy[0].Get());
    dc_->DrawEllipse(e3, brushes_.black.Get(), 2.0f);

    // Draw mid outline and line from mid to hit outline.
    if (d0 > r1) {
      dc_->FillEllipse(e2, brushes_.black.Get());
      dc_->DrawLine(p0, p2, brushes_.black.Get(), 3.0f);
    }

    // Draw hit.
    dc_->DrawEllipse(e3, brushes_.enemy[1].Get());

    // Draw mid and fill line from mid to hit outline.
    if (d0 > r1) {
      dc_->FillEllipse(e0, brushes_.enemy[1].Get());
      dc_->DrawLine(p0, p2, brushes_.enemy[1].Get());
    }

    // Create target label.
#ifndef NDEBUG
    string_.reset(L"{:.0f}", m);
    string_label(sx + x1, sy + y1, 64, 64, formats_.debug, brushes_.white);
#endif

    // Check if fire conditions are met.
    if (fire || m >= range || state.up(key::shift) || tp < lockout_) {
      continue;
    }

    // Press whipshot button if crosshair is inside hit ellipse.
    const auto ex = std::pow((sc.x - x1), 2.0f) / std::pow(r1 * 1.5f, 2.0f);
    const auto ey = std::pow((sc.y - y1), 2.0f) / std::pow(r1 * 2.5f, 2.0f);
    if (ex + ey < 1.0f) {
      input_.mask(button::up, 16ms);
      lockout_ = tp + 128ms;
      fire = true;
    }
  }

  // Draw crosshair.
  const auto x = sx + sc.x;
  const auto y = sy + sc.y;
  const auto r = 3.0f;
  const auto e = D2D1::Ellipse(D2D1::Point2F(x, y), r, r);
  dc_->FillEllipse(e, state.down(key::shift) ? brushes_.blue.Get() : brushes_.white.Get());
  dc_->DrawEllipse(e, brushes_.black.Get());
}

#endif

}  // namespace epos::overwatch