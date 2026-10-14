#pragma once
#include <array>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace wa {
using Json = nlohmann::json;
enum class Event { open, close, minimize, restore };
enum class Mode { none, fade, slide, zoom, spring, genie, wobbly };
enum class Curve { linear, smooth, out, inout };
struct Point {
  double x{}, y{};
};
struct Rect {
  double x{}, y{}, w{}, h{};
};
struct Motion {
  Mode mode{Mode::zoom};
  int duration{280};
  Curve curve{Curve::out};
  double distance{54}, scale{0.86}, opacity{0.08}, bounce{0.14}, damping{7};
  double wobble{18}, frequency{3};
  std::string origin{"bottom"};
};
struct Rule {
  bool enabled{false};
  std::string application, kind{"all"}, profile{"fluid"};
  double speed{100};
};
struct Config {
  bool enabled{true}, respectReducedMotion{true}, skipFullscreen{true};
  bool previewVisible{false}, allowTranslucent{false}, hideEntering{true};
  std::string profile{"fluid"}, windowTypes{"normal"};
  std::vector<std::string> applications{"notepad.exe"}, excluded;
  double speed{100}, intensity{100};
  std::array<bool, 4> transitions{true, true, true, true};
  std::array<Motion, 4> motions;
  std::array<Rule, 3> rules;
  int fps{60}, cacheInterval{400}, cacheMaxAge{2000}, maxConcurrent{3},
      maxSnapshots{12};
  int mesh{20};
  double radius{12}, shadow{24}, shadowOpacity{0.22}, borderWidth{0};
  std::string borderStyle{"none"}, color1{"#64BFFAFF"}, color2{"#DF83FFFF"};
};
struct ParseResult {
  Config config;
  std::string error;
  explicit operator bool() const { return error.empty(); }
};
std::vector<std::string> ApplicationList(const std::string &text);
bool MatchesKind(const std::string &filter, const std::string &kind);
ParseResult ParseSettings(const Json &snapshot);
Motion ResolveMotion(const Config &, Event, const std::string &app,
                     const std::string &kind);
double Ease(Curve, double t);
Point Warp(const Motion &, const Rect &, Point anchor, double u, double v,
           double t, bool entering);
double FrameOpacity(const Motion &, double t, bool entering);
struct Image {
  int width{}, height{};
  std::vector<uint32_t> pixels;
};
struct Frame {
  Image image;
  int x{}, y{};
  bool valid{};
};
Frame Render(const Image &, const Rect &, Point, const Motion &, const Config &,
             double t, bool entering);
uint32_t Color(const std::string &);
} // namespace wa
