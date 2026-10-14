#include "animation.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace wa {
namespace {
struct Vertex {
  Point p;
  double u, v;
};
double Cross(Point a, Point b, Point c) {
  return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}
uint32_t Pixel(const Image &src, double u, double v, const Config &c,
               double opacity) {
  int x = std::clamp(static_cast<int>(u * (src.width - 1)), 0, src.width - 1),
      y = std::clamp(static_cast<int>(v * (src.height - 1)), 0, src.height - 1);
  double radius = std::min({c.radius, src.width / 2.0, src.height / 2.0});
  double ax =
      std::max(radius - x, static_cast<double>(x) - (src.width - 1 - radius));
  double ay =
      std::max(radius - y, static_cast<double>(y) - (src.height - 1 - radius));
  double coverage = 1;
  if (ax > 0 && ay > 0)
    coverage = std::clamp(radius + .5 - std::hypot(ax, ay), 0.0, 1.0);
  uint32_t p = src.pixels[static_cast<size_t>(y) * src.width + x];
  bool border =
      c.borderStyle != "none" &&
      std::min({x, y, src.width - 1 - x, src.height - 1 - y}) < c.borderWidth;
  if (border) {
    uint32_t a = Color(c.color1),
             b = c.borderStyle == "gradient" ? Color(c.color2) : a;
    p = 0;
    for (int shift : {0, 8, 16, 24})
      p |= static_cast<uint32_t>(((a >> shift) & 255) * (1 - v) +
                                 ((b >> shift) & 255) * v)
           << shift;
  }
  double factor = opacity * coverage;
  uint32_t result = 0;
  // Source pixels are premultiplied; newly authored border colors are straight.
  double alpha = ((p >> 24) & 255) / 255.0;
  for (int shift : {0, 8, 16})
    result |= static_cast<uint32_t>(((p >> shift) & 255) * factor *
                                    (border ? alpha : 1))
              << shift;
  result |= static_cast<uint32_t>(255 * factor * alpha) << 24;
  return result;
}
void Triangle(Frame &out, const Image &src, Vertex a, Vertex b, Vertex c,
              const Config &config, double opacity) {
  double area = Cross(a.p, b.p, c.p);
  if (std::abs(area) < .0001)
    return;
  int left = std::max(
          0, static_cast<int>(std::floor(std::min({a.p.x, b.p.x, c.p.x})))),
      right = std::min(
          out.image.width - 1,
          static_cast<int>(std::ceil(std::max({a.p.x, b.p.x, c.p.x}))));
  int top = std::max(
          0, static_cast<int>(std::floor(std::min({a.p.y, b.p.y, c.p.y})))),
      bottom = std::min(
          out.image.height - 1,
          static_cast<int>(std::ceil(std::max({a.p.y, b.p.y, c.p.y}))));
  for (int y = top; y <= bottom; ++y)
    for (int x = left; x <= right; ++x) {
      Point p{x + .5, y + .5};
      double w1 = Cross(b.p, c.p, p) / area, w2 = Cross(c.p, a.p, p) / area,
             w3 = 1 - w1 - w2;
      if (w1 < -.000001 || w2 < -.000001 || w3 < -.000001)
        continue;
      out.image.pixels[static_cast<size_t>(y) * out.image.width + x] =
          Pixel(src, a.u * w1 + b.u * w2 + c.u * w3,
                a.v * w1 + b.v * w2 + c.v * w3, config, opacity);
    }
}
} // namespace
Frame Render(const Image &src, const Rect &r, Point anchor, const Motion &m,
             const Config &c, double t, bool entering) {
  Frame out;
  if (src.width < 1 || src.height < 1 ||
      src.pixels.size() != static_cast<size_t>(src.width) * src.height)
    return out;
  int n = (m.mode == Mode::genie || m.mode == Mode::wobbly) ? c.mesh : 1;
  std::vector<Vertex> vertices;
  vertices.reserve(static_cast<size_t>(n + 1) * (n + 1));
  double left = std::numeric_limits<double>::max(), right = -left, top = left,
         bottom = -left;
  for (int y = 0; y <= n; ++y)
    for (int x = 0; x <= n; ++x) {
      double u = static_cast<double>(x) / n, v = static_cast<double>(y) / n;
      auto p = Warp(m, r, anchor, u, v, t, entering);
      vertices.push_back({p, u, v});
      left = std::min(left, p.x);
      right = std::max(right, p.x);
      top = std::min(top, p.y);
      bottom = std::max(bottom, p.y);
    }
  int pad = static_cast<int>(std::ceil(c.shadow)) + 2;
  out.x = static_cast<int>(std::floor(left)) - pad;
  out.y = static_cast<int>(std::floor(top)) - pad;
  out.image.width = static_cast<int>(std::ceil(right)) - out.x + pad;
  out.image.height = static_cast<int>(std::ceil(bottom)) - out.y + pad;
  if (out.image.width <= 0 || out.image.height <= 0 ||
      static_cast<int64_t>(out.image.width) * out.image.height > 8'000'000)
    return out;
  out.image.pixels.assign(
      static_cast<size_t>(out.image.width) * out.image.height, 0);
  for (auto &a : vertices) {
    a.p.x -= out.x;
    a.p.y -= out.y;
  }
  double opacity = FrameOpacity(m, t, entering);
  for (int y = 0; y < n; ++y)
    for (int x = 0; x < n; ++x) {
      auto a = vertices[static_cast<size_t>(y) * (n + 1) + x],
           b = vertices[static_cast<size_t>(y) * (n + 1) + x + 1];
      auto d = vertices[static_cast<size_t>(y + 1) * (n + 1) + x],
           e = vertices[static_cast<size_t>(y + 1) * (n + 1) + x + 1];
      Triangle(out, src, a, b, e, c, opacity);
      Triangle(out, src, a, e, d, c, opacity);
    }
  if (c.shadow > 0 && c.shadowOpacity > 0) {
    // A bounded separable box blur of the rendered alpha, offset downwards.
    int w = out.image.width, h = out.image.height,
        rad = std::max(1, static_cast<int>(c.shadow / 2));
    std::vector<double> horizontal(out.image.pixels.size());
    for (int y = 0; y < h; ++y) {
      double sum = 0;
      for (int x = -rad; x < w; ++x) {
        int add = x + rad, sub = x - rad - 1;
        if (add >= 0 && add < w)
          sum +=
              (out.image.pixels[static_cast<size_t>(y) * w + add] >> 24) & 255;
        if (sub >= 0 && sub < w)
          sum -=
              (out.image.pixels[static_cast<size_t>(y) * w + sub] >> 24) & 255;
        if (x >= 0)
          horizontal[static_cast<size_t>(y) * w + x] = sum / (2 * rad + 1);
      }
    }
    for (int x = 0; x < w; ++x) {
      double sum = 0;
      for (int y = -rad; y < h; ++y) {
        int add = y + rad, sub = y - rad - 1;
        if (add >= 0 && add < h)
          sum += horizontal[static_cast<size_t>(add) * w + x];
        if (sub >= 0 && sub < h)
          sum -= horizontal[static_cast<size_t>(sub) * w + x];
        if (y >= 0) {
          int dy = std::min(h - 1, y + static_cast<int>(c.shadow / 4));
          auto &p = out.image.pixels[static_cast<size_t>(dy) * w + x];
          uint32_t alpha = p >> 24;
          uint32_t shade = static_cast<uint32_t>(
              std::clamp(sum / (2 * rad + 1) * c.shadowOpacity, 0.0, 255.0));
          p = (p & 0xffffff) | ((alpha + shade * (255 - alpha) / 255) << 24);
        }
      }
    }
  }
  out.valid = true;
  return out;
}
} // namespace wa
