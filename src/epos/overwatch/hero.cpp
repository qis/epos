#include "view.hpp"
#include <cmath>

namespace epos::overwatch {
namespace {

static const auto melee = 2000ms;
static const auto power = 1280ms;

}  // namespace

void view::reaper(clock::time_point tp, const epos::input::state& state, const XMFLOAT2& mouse) noexcept
{
  // Lockout duration on primary fire.
  static constexpr auto fire = 128ms;

  // Weapon spread.
  static const D2D1_ELLIPSE spread{ D2D1::Ellipse(D2D1::Point2F(sx + sc.x, sy + sc.y), 80.0f, 80.0f) };

  // Trigger distance in meters.
  static constexpr auto trigger = 16.0f;

  // Handle input.
  if (state.pressed(key::e) || state.pressed(key::shift)) {
    fire_ = tp + power;
    melee_ = tp + melee;
  }

  if (state.down(button::left)) {
    fire_ = tp + fire;
  }

  if (state.pressed(key::c)) {
    melee_ = tp + melee;
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

    // Calculate distance to camera in meters.
    auto m = XMVectorGetX(XMVector3Length(camera - target.mid));
    if (m < 0.9f) {
      continue;
    }

    // Calculate movement vector for next frame.
    const auto mv = predict(i, tp, milliseconds(8.0f));

    // Project target points.
    const auto top = game::project(vm_, target.top + mv, sw, sh);
    if (!top) {
      continue;
    }
    const auto mid = game::project(vm_, target.mid + mv, sw, sh);
    if (!mid) {
      continue;
    }

    // Get target brushes.
    const auto& target_brushes = target.tank ? brushes_.tank : brushes_.target;

    // Draw target.
    const auto x0 = mid->x - mouse.x;
    const auto y0 = mid->y - mouse.y;
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
      if (tp > fire_ && (m < 1.3f || state.down(button::right))) {
        input_.mask(button::up, 16ms);
        fire_ = tp + fire;
      }
      // Inject melee.
      if (!target.tank && tp > melee_ && (m < 1.3f || (state.down(button::right) && m < 2.5f))) {
        input_.mask(button::down, 16ms, 32ms);
        melee_ = tp + melee;
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

void view::symmetra(clock::time_point tp, const epos::input::state& state, const XMFLOAT2& mouse) noexcept
{
  // Lockout duration on secondary fire.
  static constexpr auto fire = 128ms;

  // Projectile speed in meters per second.
  // The projectile travels 50m in 127 frames at 120 fps.
  // 120 FPS / 127 frames * 50m = 47.2 m/s
  static constexpr auto speed = 47.2f;

  // Get camera position.
  const auto camera = game::camera(vm_);

  // Handle input.
  if (state.pressed(button::right)) {
    fire_ = tp + fire;
    melee_ = tp + melee;
  }

  // Draw entities.
  for (std::size_t i = 0; i < scene_draw_->entities; i++) {
    const auto& e = entities_[i];
    if (!e || e.team == team_) {
      movement_[i].clear();
      continue;
    }

    // Get target.
    auto target = e.target();

    // Calculate distance to camera in meters.
    auto m = XMVectorGetX(XMVector3Length(camera - target.mid));
    if (m < 0.9f) {
      continue;
    }

    // Calculate projectile travel time in milliseconds.
    auto s = m / speed;

    // Calculate movement vector for projectile impact.
    const auto mv = predict(i, tp, seconds(s));
    m = XMVectorGetX(XMVector3Length(camera - target.mid + mv));
    s = m / speed;

    // Project target points.
    const auto top = game::project(vm_, target.top, sw, sh);
    if (!top) {
      continue;
    }
    const auto mid = game::project(vm_, target.mid, sw, sh);
    if (!mid) {
      continue;
    }
    const auto hit = game::project(vm_, target.mid + mv, sw, sh);
    if (!hit) {
      continue;
    }

    // Get target brushes.
    const auto& target_brushes = target.tank ? brushes_.tank : brushes_.target;

    // Create mid ellipse.
    const auto x0 = mid->x - mouse.x;
    const auto y0 = mid->y - mouse.y;
    const auto e0 = D2D1::Ellipse(D2D1::Point2F(sx + x0, sy + y0), 2.0f, 2.0f);

    // Create mid outline ellipse.
    const auto e1 = D2D1::Ellipse(D2D1::Point2F(sx + x0, sy + y0), 3.0f, 3.0f);

    // Create hit ellipse.
    const auto x2 = hit->x - mouse.x;
    const auto y2 = hit->y - mouse.y;
    const auto r2 = mid->y - top->y;
    const auto r1 = r2 * std::min(target.ratio * 2.0f, 1.0f);
    const auto e2 = D2D1::Ellipse(D2D1::Point2F(sx + x2, sy + y2), r1, r2);

    // Create hit outline ellipse.
    const auto e3 = D2D1::Ellipse(D2D1::Point2F(sx + x2, sy + y2), r1 + 1.0f, r2 + 1.0f);

    // Calculate distance between hit and mid points.
    const auto d0 = std::sqrt(std::pow(x2 - x0, 2.0f) + std::pow(y2 - y0, 2.0f));

    // Create hit to mid connection points.
    const auto p0 = D2D1::Point2F(sx + x0, sy + y0);
    const auto t1 = r1 / std::max(d0, 1.0f);
    const auto t2 = r2 / std::max(d0, 1.0f);
    const auto x1 = (1.0f - t1) * x2 + t1 * x0;
    const auto y1 = (1.0f - t2) * y2 + t2 * y0;
    const auto p1 = D2D1::Point2F(sx + x1, sy + y1);

    // Draw outline for connection between hit and mid.
    if (d0 > r2) {
      dc_->DrawLine(p0, p1, brushes_.black.Get(), 3.0f);
    }

    // Draw hit ellipse.
    dc_->DrawEllipse(e3, brushes_.black.Get());
    dc_->FillEllipse(e2, target_brushes[1].Get());
    dc_->DrawEllipse(e2, target_brushes[0].Get());

    // Draw mid ellipse and connection between hit and mid.
    if (d0 > r2) {
      dc_->FillEllipse(e1, brushes_.black.Get());
      dc_->FillEllipse(e0, target_brushes[0].Get());
      dc_->DrawLine(p0, p1, target_brushes[0].Get());
    }

    // Create target label.
#ifndef NDEBUG
    string_.reset(L"{:.0f}", m);
    string_label(sx + x0, sy + y0, 128, 32, formats_.label, target_brushes[0]);
#endif
  }

  // Draw crosshair.
  const auto x = sx + sc.x;
  const auto y = sy + sc.y;
  const auto r = 3.0f;
  const auto e = D2D1::Ellipse(D2D1::Point2F(x, y), r, r);
  dc_->FillEllipse(e, state.down(button::right) ? brushes_.blue.Get() : brushes_.white.Get());
  dc_->DrawEllipse(e, brushes_.black.Get());
}

void view::widowmaker(clock::time_point tp, const epos::input::state& state, const XMFLOAT2& mouse) noexcept
{
  // Lockout duration on scope in and primary fire.
  static constexpr auto fire = 1100ms;

  // Handle input.
  if (state.pressed(button::right)) {
    fire_ = tp + fire;
  }
  if (state.up(button::right) && state.down(button::left) && tp > fire_) {
    input_.mask(button::up, 64ms);
    fire_ = tp + 32ms;
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

    // Calculate distance to camera in meters.
    auto m = XMVectorGetX(XMVector3Length(camera - target.mid));
    if (m < 0.9f) {
      continue;
    }

    // Calculate movement vector for next frame.
    const auto mv = predict(i, tp, milliseconds(8.0f));

    // Project target points.
    const auto top = game::project(vm_, target.top + mv, sw, sh);
    if (!top) {
      continue;
    }
    const auto mid = game::project(vm_, target.mid + mv, sw, sh);
    if (!mid) {
      continue;
    }

    // Get target brushes.
    const auto& target_brushes = target.tank ? brushes_.tank : brushes_.target;

    // Draw target.
    const auto x0 = mid->x - mouse.x;
    const auto y0 = mid->y - mouse.y;
    const auto r1 = mid->y - top->y;
    const auto r0 = target.ratio * r1;
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

    // Check if primary fire conditions are met.
    if (tp < fire_ || state.up(button::right) || state.up(button::left)) {
      continue;
    }

    // Press primary fire button if crosshair is inside ellipse or crosses the ellipse (lazy).
    const auto o0 = sc.x - mid->x;
    const auto o1 = sc.y - mid->y;
    const auto r2 = std::pow(r0, 2.0f);
    const auto r3 = std::pow(r1, 2.0f);
    for (auto multiplier = 1.0f; multiplier < 3.1f; multiplier += 1.0f) {
      const auto ex = std::pow(o0 + mouse.x * multiplier * 2.0f, 2.0f) / r2;
      const auto ey = std::pow(o0 + mouse.y * multiplier, 2.0f) / r3;
      if (ex + ey < 1.0f) {
        input_.mask(button::up, 16ms);
        fire_ = tp + fire;
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

}  // namespace epos::overwatch