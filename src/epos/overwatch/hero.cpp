#include "view.hpp"
#include <cmath>

namespace epos::overwatch {

#if 1

void view::hero(clock::time_point tp0, const epos::input::state& state, const XMFLOAT2& mouse) noexcept
{
  // Weapon spread.
  static const D2D1_ELLIPSE spread{ D2D1::Ellipse(D2D1::Point2F(sx + sc.x, sy + sc.y), 80.0f, 80.0f) };

  // Trigger distance in meters.
  static constexpr auto trigger = 16.0f;

  // Handle input.
  if (state.pressed(key::e) || state.pressed(key::shift)) {
    lockout_ = tp0 + 1280ms;
  }

  if (state.down(button::left)) {
    lockout_ = tp0 + 64ms;
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
    auto target = e.mid();
    if (update_movement || movement_[i].empty()) {
      movement_[i].push_back({ target, tp0 });
    }

    // Calculate target offset for next frame.
    auto offset = XMVectorSet(0.0f, e.height() * 0.15, 0.0f, 0.0f);
    if (movement_[i].size() > 1) {
      const auto& snapshot = movement_[i].front();
      const auto time_scale = 8.0f / duration_cast<milliseconds>(tp0 - snapshot.time_point).count();
      offset += (target - snapshot.target) * time_scale;
    }

    // Calculate distance to camera in meters.
    auto m = XMVectorGetX(XMVector3Length(camera - target));

    // Project target points.
    const auto top = game::project(vm_, e.top() + offset, sw, sh);
    if (!top) {
      continue;
    }
    const auto mid = game::project(vm_, target + offset, sw, sh);
    if (!mid) {
      continue;
    }

    // Draw target.
    const auto x0 = mid->x - mouse.x;
    const auto y0 = mid->y - mouse.y;
    const auto r1 = mid->y - top->y;
    const auto r0 = (e.width() / e.height()) * r1 * 0.6f;
    const auto e0 = D2D1::Ellipse(D2D1::Point2F(sx + x0, sy + y0), r0, r1);
    if (m < trigger) {
      dc_->DrawEllipse(e0, brushes_.black.Get(), 2.0f);
      dc_->DrawEllipse(e0, brushes_.white.Get(), 1.6f);
      dc_->DrawEllipse(e0, brushes_.enemy[1].Get(), 1.5f);
    } else if (m < trigger * 1.2f) {
      dc_->DrawEllipse(e0, brushes_.enemy[2].Get());
    } else if (m < trigger * 1.4f) {
      dc_->DrawEllipse(e0, brushes_.enemy[3].Get());
    } else if (m < trigger * 1.6f) {
      dc_->DrawEllipse(e0, brushes_.enemy[4].Get());
    } else if (m < trigger * 1.8f) {
      dc_->DrawEllipse(e0, brushes_.enemy[5].Get());
    } else if (m < trigger * 2.0f) {
      dc_->DrawEllipse(e0, brushes_.enemy[6].Get());
    } else if (m < trigger * 2.2f) {
      dc_->DrawEllipse(e0, brushes_.enemy[7].Get());
    } else {
      dc_->DrawEllipse(e0, brushes_.enemy[8].Get());
    }

    // Create target label.
#ifndef NDEBUG
    string_.reset(L"{:.0f}", m);
    string_label(sx + x0, sy + y0, 32, 32, formats_.label, brushes_.white);
#endif

    // Check if fire conditions are met.
    if (fire || m >= trigger || state.down(key::control)) {
      continue;
    }

    // Press fire button if crosshair is inside ellipse or ellipse is inside spread.
    const auto ex = std::pow((sc.x - mid->x), 2.0f) / std::pow(r0, 2.0f);
    const auto ey = std::pow((sc.y - mid->y), 2.0f) / std::pow(r1, 2.0f);
    const auto rs = spread.radiusX;
    if (ex + ey < 1.0f || (ey < 1.0f && x0 - r0 > sc.x - rs && x0 + r0 < sc.x + rs)) {
      if (tp0 > lockout_ && (m < 2.0f || state.down(button::right))) {
        input_.mask(button::up, 16ms);
        lockout_ = tp0 + 128ms;
      }
      fire = true;
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

#elif 0

void view::hero(clock::time_point tp0, const epos::input::state& state, const XMFLOAT2& mouse) noexcept
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
    lockout_ = tp0 + 512ms;
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
    auto target = e.mid();
    if (update_movement || movement_[i].empty()) {
      movement_[i].push_back({ target, tp0 });
    }

    // Calculate distance to camera in meters.
    auto m = XMVectorGetX(XMVector3Length(camera - target));

    // Calculate whipshot travel time in milliseconds.
    auto s = m / speed;

    // Calculate target position offset on whipshot impact.
    if (movement_[i].size() > 1) {
      const auto& snapshot = movement_[i].front();
      const auto scale = s / duration_cast<seconds>(tp0 - snapshot.time_point).count();
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
    if (fire || m >= range || state.up(key::shift) || tp0 < lockout_) {
      continue;
    }

    // Press whipshot button if crosshair is inside hit ellipse.
    const auto ex = std::pow((sc.x - x1), 2.0f) / std::pow(r1 * 1.5f, 2.0f);
    const auto ey = std::pow((sc.y - y1), 2.0f) / std::pow(r1 * 2.5f, 2.0f);
    if (ex + ey < 1.0f) {
      input_.mask(button::up, 16ms);
      lockout_ = tp0 + 128ms;
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