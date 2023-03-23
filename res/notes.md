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
