#include "animation.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
namespace wa {
double Ease(Curve curve, double t) {
  t = std::clamp(t, 0.0, 1.0);
  switch (curve) {
  case Curve::linear:
    return t;
  case Curve::smooth:
    return t * t * (3 - 2 * t);
  case Curve::inout:
    return t < .5 ? 4 * t * t * t : 1 - std::pow(-2 * t + 2, 3) / 2;
  case Curve::out:
    return 1 - std::pow(1 - t, 3);
  }
  return t;
}
Point Warp(const Motion &m, const Rect &r, Point anchor, double u, double v,
           double t, bool entering) {
  double e = Ease(m.curve, t), q = entering ? e : 1 - e;
  Point p{r.x + u * r.w, r.y + v * r.h};
  if (m.mode == Mode::none || m.mode == Mode::fade)
    return p;
  double gone = 1 - q;
  if (m.mode == Mode::genie) {
    bool horizontal = m.origin == "left" || m.origin == "right";
    bool invert = m.origin == "top" || m.origin == "left";
    double a = horizontal ? v : u, b = horizontal ? u : v;
    if (invert)
      b = 1 - b;
    double delayed =
        std::clamp((gone - .35 * (1 - b)) / (1 - .35 * (1 - b)), 0.0, 1.0);
    double span = (horizontal ? r.h : r.w) * (1 - .985 * delayed);
    double cross =
        (horizontal ? r.y + r.h / 2 : r.x + r.w / 2) * (1 - delayed) +
        (horizontal ? anchor.y : anchor.x) * delayed;
    double flow = std::pow(gone, 1.45);
    if (horizontal)
      return {p.x * (1 - flow) + anchor.x * flow, cross + (a - .5) * span};
    return {cross + (a - .5) * span, p.y * (1 - flow) + anchor.y * flow};
  }
  double k = m.scale + (1 - m.scale) * q;
  if (m.mode == Mode::spring)
    k += m.bounce * std::exp(-m.damping * t) *
         std::sin(2 * std::numbers::pi * 2.5 * t) *
         std::sin(std::numbers::pi * t);
  k = std::max(0.04, k);
  if (m.mode == Mode::slide)
    k = 1;
  double cx = r.x + r.w / 2, cy = r.y + r.h / 2;
  p = {cx + (p.x - cx) * k, cy + (p.y - cy) * k};
  if (m.mode == Mode::slide || m.mode == Mode::spring) {
    double dx = 0, dy = m.distance;
    if (m.origin == "top")
      dy = -dy;
    else if (m.origin == "left") {
      dx = -dy;
      dy = 0;
    } else if (m.origin == "right") {
      dx = dy;
      dy = 0;
    } else if (m.origin == "center") {
      dx = dy = 0;
    } else if (m.origin == "taskbar" || m.origin == "cursor") {
      double len = std::hypot(anchor.x - cx, anchor.y - cy);
      if (len > 1) {
        dx = m.distance * (anchor.x - cx) / len;
        dy = m.distance * (anchor.y - cy) / len;
      }
    }
    p.x += dx * gone;
    p.y += dy * gone;
  }
  if (m.mode == Mode::zoom && (m.origin == "taskbar" || m.origin == "cursor")) {
    p.x += (anchor.x - cx) * gone;
    p.y += (anchor.y - cy) * gone;
  }
  if (m.mode == Mode::wobbly) {
    double wave = m.wobble * std::sin(std::numbers::pi * t) *
                  std::exp(-m.damping * t * .2);
    p.x += wave * std::sin(std::numbers::pi * v) *
           std::sin(2 * std::numbers::pi * (m.frequency * t + u * .3));
    p.y += wave * .65 * std::sin(std::numbers::pi * u) *
           std::sin(2 * std::numbers::pi * (m.frequency * t + v * .4));
  }
  return p;
}
double FrameOpacity(const Motion &m, double t, bool entering) {
  double q = entering ? Ease(m.curve, t) : 1 - Ease(m.curve, t);
  return std::clamp(m.opacity + (1 - m.opacity) * q, 0.0, 1.0);
}
} // namespace wa
