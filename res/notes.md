# Notes
Debug entity head height.

```cpp
const auto& e = entities_[i];

const auto height = e.height();
const auto width = e.width();

D2D_POINT_2F top{};
if (const auto point = game::project(vm_, e.top(), sw, sh)) {
  const auto x = sx + point->x - mouse.x;
  const auto y = sy + point->y - mouse.y;
  const auto r = 3.0f;
  const auto e = D2D1::Ellipse(D2D1::Point2F(x, y), r, r);
  dc_->DrawLine({ x - 100, y }, { x + 100, y }, brushes_.red.Get(), 2.0f);
  dc_->FillEllipse(e, brushes_.red.Get());
  dc_->DrawEllipse(e, brushes_.black.Get());
  top = D2D1::Point2F(x, y);
} else {
  continue;
}

D2D_POINT_2F bottom{};
if (const auto point = game::project(vm_, e.bottom(), sw, sh)) {
  const auto x = sx + point->x - mouse.x;
  const auto y = sy + point->y - mouse.y;
  const auto r = 3.0f;
  const auto e = D2D1::Ellipse(D2D1::Point2F(x, y), r, r);
  dc_->DrawLine({ x - 100, y }, { x + 100, y }, brushes_.green.Get(), 2.0f);
  dc_->FillEllipse(e, brushes_.green.Get());
  dc_->DrawEllipse(e, brushes_.black.Get());
  bottom = D2D1::Point2F(x, y);
} else {
  continue;
}

D2D_POINT_2F center{};
if (const auto point = game::project(vm_, e.center(), sw, sh)) {
  const auto x = sx + point->x - mouse.x;
  const auto y = sy + point->y - mouse.y;
  const auto r = 3.0f;
  const auto e = D2D1::Ellipse(D2D1::Point2F(x, y), r, r);
  dc_->DrawLine({ x - 100, y }, { x + 100, y }, brushes_.blue.Get(), 2.0f);
  dc_->FillEllipse(e, brushes_.blue.Get());
  dc_->DrawEllipse(e, brushes_.black.Get());
  string_.reset(L"{:.02f} | {:.02f}", height, width);
  string_label(x - 64, y, 64, 32, formats_.label, brushes_.white);
  center = D2D1::Point2F(x, y);
} else {
  continue;
}

{
  const auto x = top.x;
  const auto y = top.y + (bottom.y - top.y) * 0.15;
  const auto r = 3.0f;
  const auto e = D2D1::Ellipse(D2D1::Point2F(x, y), r, r);
  dc_->FillEllipse(e, brushes_.white.Get());
  dc_->DrawEllipse(e, brushes_.black.Get());
}

{
  const auto x = top.x;
  const auto y = top.y + (bottom.y - top.y) * 0.25;
  const auto r = 3.0f;
  const auto e = D2D1::Ellipse(D2D1::Point2F(x, y), r, r);
  dc_->FillEllipse(e, brushes_.yellow.Get());
  dc_->DrawEllipse(e, brushes_.black.Get());
}

{
  const auto x = top.x;
  const auto y = top.y + (bottom.y - top.y) * 0.35;
  const auto r = 3.0f;
  const auto e = D2D1::Ellipse(D2D1::Point2F(x, y), r, r);
  dc_->FillEllipse(e, brushes_.white.Get());
  dc_->DrawEllipse(e, brushes_.black.Get());
}
```

## Widowmaker
```cpp
if (widowmaker_) {
  const auto top = game::project(vm_, e.top(), sw, sh);
  const auto mid = game::project(vm_, e.mid(), sw, sh);
  if (top && mid) {
    const auto x0 = -mouse.x + top->x - 4.0f;
    const auto x1 = -mouse.x + top->x + 4.0f;
    const auto y0 = -mouse.y + top->y;
    const auto y1 = -mouse.y + mid->y;
    if (sc.y >= y0 && sc.y <= y1 && sc.x >= x0 && sc.x <= x1) {
      fire = true;
    }
    const D2D1_RECT_F rect{ sx + x0, sy + y0, sx + x1, sy + y1 };
    dc_->DrawRectangle(rect, brushes_.enemy.Get());
  }
  continue;
}

// Inject fire.
if (fire && state.down(button::right) && tp0 > lockout_) {
  input_.mask(button::up, 16ms);
  lockout_ = tp0 + 1300ms;
}
```

## Hanzo
```cpp
// Draw entities.
for (std::size_t i = 0; i < scene_draw_->entities; i++) {
  const auto& e = entities_[i];
  if (!e) {
    movement_[i].clear();
    continue;
  }

  // Get target position.
  auto target = e.head();
  if (update_movement || movement_[i].empty()) {
    movement_[i].push_back({ target, tp0 });
  }

  // Calculate distance to camera in meters.
  auto m = XMVectorGetX(XMVector3Length(camera - target));
  if (m < 1.5f) {
    continue;
  }

  // Calculate arrow travel time in seconds.
  auto s = m * game::arrow_speed;

  // Calculate target position on arrow impact.
  if (movement_[i].size() > 1) {
    const auto& snapshot = movement_[i].front();
    const auto scale = s / duration_cast<seconds>(tp0 - snapshot.time_point).count();
    target += (target - snapshot.target) * scale;
    m = XMVectorGetX(XMVector3Length(camera - target));
    s = m * game::arrow_speed;
  }

  // Account for arrow drop.
  target += m * game::arrow_drop;

  // Project target.
  const auto point = game::project(vm_, target, sw, sh);
  if (!point) {
    continue;
  }

  // Draw target.
  const auto x0 = sx + point->x - mouse.x;
  const auto y0 = sy + point->y - mouse.y;
  const auto r0 = std::max(2.0f, std::sqrt(500.0f / m));
  const auto e0 = D2D1::Ellipse(D2D1::Point2F(x0, y0), r0, r0);
  if (const auto head = game::project(vm_, e.head(), sw, sh)) {
    const auto x1 = sx + head->x - mouse.x;
    const auto y1 = sy + head->y - mouse.y;
    dc_->DrawLine({ x0, y0 }, { x1, y1 }, brushes_.enemy.Get());
  }
  dc_->FillEllipse(e0, brushes_.enemy.Get());
  dc_->DrawEllipse(e0, brushes_.black.Get());
  //string_.reset(L"{:.1f}m ({:.03f}s)", m, s);
  //string_label(x, y + 20, 63, 32, formats_.label, brushes_.white);
}
```
