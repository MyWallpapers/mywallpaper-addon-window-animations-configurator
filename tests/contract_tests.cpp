#include "animation.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace wa;
namespace {
void Check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
Json Read(const std::filesystem::path &path) {
  std::ifstream f(path);
  Json j;
  f >> j;
  return j;
}
void ConfigContract(const std::filesystem::path &root) {
  auto manifest = Read(root / "manifest.json");
  Check(manifest["runtime"] == "native-v1", "native surface");
  Check(manifest["settings"].size() <= 100, "setting definition limit");
  Json defaults = Json::object();
  for (const auto &s : manifest["settings"])
    if (s["type"] != "section")
      defaults[s["id"].get<std::string>()] = s["default"];
  auto parsed = ParseSettings(
      {{"deviceSettings", defaults}, {"layerSettings", Json::object()}});
  Check(bool(parsed), "manifest defaults must parse");
  Check(parsed.config.profile == "fluid" && parsed.config.respectReducedMotion,
        "default profile and comfort");
  for (const auto &s : manifest["settings"]) {
    auto key = s["id"].get<std::string>();
    auto type = s["type"].get<std::string>();
    if (type == "range") {
      for (auto endpoint : {s["min"], s["max"]}) {
        auto a = defaults;
        a[key] = endpoint;
        Check(bool(ParseSettings(a)), "range endpoint rejected");
      }
      auto a = defaults;
      a[key] = s["max"].get<double>() + 1;
      Check(!ParseSettings(a), "range overflow accepted");
    }
    if (type == "select")
      for (const auto &option : s["options"]) {
        auto a = defaults;
        a[key] = option["value"];
        Check(bool(ParseSettings(a)), "select option rejected");
      }
    if (type == "boolean") {
      auto a = defaults;
      a[key] = "true";
      Check(!ParseSettings(a), "boolean string accepted");
    }
    if (s.contains("showIf")) {
      auto id = s["showIf"]["setting"].get<std::string>();
      Check(defaults.contains(id), "showIf missing reference");
    }
  }
  for (const char *name :
       {"fluid", "discreet", "cinema", "bounce", "genie", "wobbly"}) {
    auto preset =
        ParseSettings(Read(root / "presets" / (std::string(name) + ".json")));
    Check(bool(preset), "preset rejected");
    for (int i = 0; i < 4; ++i) {
      auto m = ResolveMotion(preset.config, static_cast<Event>(i), "app.exe",
                             "normal");
      Check(m.duration >= 40 && m.mode != Mode::none,
            "preset empty transition");
    }
  }
  Check(!ParseSettings({{"startOpacity", 101}}), "opacity overflow");
  Check(!ParseSettings({{"color1", "#ABC"}}), "invalid color");
  Check(!ParseSettings({{"frameRate", 45}}), "unsupported frame rate");
  Check(!ParseSettings({{"applications", "C:\\tools\\app.exe"}}),
        "paths accepted as image names");
  Check(ApplicationList(" App.EXE;app.exe\nsecond.exe ").size() == 2,
        "application normalization");
  auto c = ParseSettings({{"profile", "custom"},
                          {"minimizeEnabled", false},
                          {"openMode", "spring"}})
               .config;
  Check(ResolveMotion(c, Event::minimize, "app.exe", "normal").mode ==
            Mode::none,
        "disabled event");
  Check(ResolveMotion(c, Event::open, "app.exe", "normal").mode == Mode::spring,
        "custom mode");
  c.rules[0] = {true, "app.exe", "all", "genie", 100};
  c.rules[1] = {true, "app.exe", "dialog", "off", 100};
  Check(ResolveMotion(c, Event::open, "app.exe", "normal").mode == Mode::genie,
        "app profile");
  Check(ResolveMotion(c, Event::open, "app.exe", "dialog").mode == Mode::none,
        "last matching rule wins");
  Check(Color("#10203080") == 0x80102030, "alpha-last color");
  c = ParseSettings({{"speed", 200}}).config;
  Check(ResolveMotion(c, Event::open, "app.exe", "normal").duration == 140,
        "speed scales duration");
  c = ParseSettings(
          {{"profile", "custom"}, {"startSize", 10}, {"intensity", 200}})
          .config;
  Check(ResolveMotion(c, Event::open, "app.exe", "normal").scale >= .05,
        "maximum intensity must not invert a small starting size");
}
void MotionContract() {
  Rect r{-900, 140, 280, 180};
  Point anchor{-800, 800};
  for (auto mode : {Mode::fade, Mode::slide, Mode::zoom, Mode::spring,
                    Mode::genie, Mode::wobbly}) {
    Motion m;
    m.mode = mode;
    for (const char *origin :
         {"center", "left", "right", "top", "bottom", "cursor", "taskbar"}) {
      m.origin = origin;
      for (auto uv : {Point{0, 0}, Point{1, 1}, Point{.3, .7}}) {
        auto p = Warp(m, r, anchor, uv.x, uv.y, 1, true);
        Check(std::abs(p.x - (r.x + uv.x * r.w)) < 1e-8 &&
                  std::abs(p.y - (r.y + uv.y * r.h)) < 1e-8,
              "enter must end exactly at source bounds");
        auto start = Warp(m, r, anchor, uv.x, uv.y, 0, false);
        Check(std::abs(start.x - p.x) < 1e-8 && std::abs(start.y - p.y) < 1e-8,
              "leave must begin exactly at source bounds");
        for (int i = 0; i <= 100; ++i) {
          auto q = Warp(m, r, anchor, uv.x, uv.y, i / 100.0, true);
          Check(std::isfinite(q.x) && std::isfinite(q.y), "nonfinite warp");
        }
      }
    }
  }
  Motion m;
  m.mode = Mode::genie;
  auto a = Warp(m, r, anchor, 0, 0, .7, false),
       b = Warp(m, r, anchor, 1, 0, .7, false);
  auto c = Warp(m, r, anchor, 0, 1, .7, false),
       d = Warp(m, r, anchor, 1, 1, .7, false);
  Check(std::abs(d.x - c.x) < std::abs(b.x - a.x),
        "Genie must neck toward its target");
  m.mode = Mode::spring;
  m.scale = .05;
  m.bounce = 1.6;
  m.damping = 1;
  for (bool entering : {false, true})
    for (int i = 0; i <= 100; ++i) {
      auto left = Warp(m, r, anchor, 0, .5, i / 100.0, entering);
      auto right = Warp(m, r, anchor, 1, .5, i / 100.0, entering);
      Check(right.x - left.x >= r.w * .04 - 1e-8,
            "extreme spring must keep a positive scale");
    }
  for (auto curve : {Curve::linear, Curve::smooth, Curve::out, Curve::inout}) {
    double prior = 0;
    for (int i = 0; i <= 100; ++i) {
      double value = Ease(curve, i / 100.0);
      Check(value >= prior - 1e-10 && value >= 0 && value <= 1,
            "easing must remain monotonic and bounded");
      prior = value;
    }
  }
}
void RendererContract() {
  Image image{96, 72, std::vector<uint32_t>(96 * 72, 0xff90b4d0)};
  Rect r{20, 30, 96, 72};
  Point anchor{68, 210};
  Config config;
  config.shadow = 0;
  config.radius = 0;
  for (auto mode : {Mode::fade, Mode::slide, Mode::zoom, Mode::spring,
                    Mode::genie, Mode::wobbly}) {
    Motion m;
    m.mode = mode;
    for (bool entering : {false, true})
      for (double t : {0.0, .25, .5, .75, 1.0}) {
        auto frame = Render(image, r, anchor, m, config, t, entering);
        Check(frame.valid, "render failed");
        bool pixel = false;
        for (auto p : frame.image.pixels) {
          uint32_t a = p >> 24;
          Check((p & 255) <= a && ((p >> 8) & 255) <= a &&
                    ((p >> 16) & 255) <= a,
                "ULW pixels must be premultiplied");
          pixel |= a != 0;
        }
        bool collapsed = mode == Mode::genie &&
                         ((entering && t == 0) || (!entering && t == 1));
        Check(pixel || collapsed, "blank effect frame");
      }
  }
  Motion m;
  m.mode = Mode::fade;
  auto a = Render(image, r, anchor, m, config, .2, true),
       b = Render(image, r, anchor, m, config, .8, true);
  uint64_t sumA = 0, sumB = 0;
  for (auto p : a.image.pixels)
    sumA += p >> 24;
  for (auto p : b.image.pixels)
    sumB += p >> 24;
  Check(sumB > sumA, "fade must become more visible");
  Check(!Render({}, r, anchor, m, config, .5, true).valid,
        "empty source accepted");
  Image translucent{8, 8, std::vector<uint32_t>(64, 0x80402010)};
  auto semi = Render(translucent, {0, 0, 8, 8}, {4, 20}, m, config, 1, true);
  for (auto p : semi.image.pixels)
    if ((p >> 24) != 0)
      Check(p == 0x80402010, "premultiplied source was multiplied twice");
  config.borderStyle = "gradient";
  config.borderWidth = 8;
  config.color1 = "#FF0000FF";
  config.color2 = "#0000FFFF";
  auto gradient = Render(image, r, anchor, m, config, 1, true);
  Check(gradient.valid, "gradient capture");
}
} // namespace
int main(int argc, char **argv) {
  try {
    Check(argc == 2, "repo root required");
    ConfigContract(argv[1]);
    MotionContract();
    RendererContract();
    std::cout << "Settings, six presets, rules, easing, geometry and "
                 "premultiplied rendering passed.\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
