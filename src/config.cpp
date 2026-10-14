#include "animation.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace wa {
namespace {
std::string Lower(std::string s) {
  for (char &c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
void Require(bool value, const std::string &key) {
  if (!value)
    throw std::invalid_argument("Valeur invalide : " + key);
}
std::string String(const Json &j, const std::string &k, std::string value) {
  if (j.contains(k)) {
    Require(j[k].is_string(), k);
    value = j[k].get<std::string>();
    Require(value.size() <= 4096, k);
  }
  return value;
}
bool Bool(const Json &j, const std::string &k, bool value) {
  if (j.contains(k)) {
    Require(j[k].is_boolean(), k);
    value = j[k].get<bool>();
  }
  return value;
}
double Number(const Json &j, const std::string &k, double value, double lo,
              double hi) {
  if (j.contains(k)) {
    Require(j[k].is_number(), k);
    value = j[k].get<double>();
  }
  Require(std::isfinite(value) && value >= lo && value <= hi, k);
  return value;
}
int Integer(const Json &j, const std::string &k, int value, int lo, int hi) {
  double n = Number(j, k, value, lo, hi);
  Require(std::floor(n) == n, k);
  return static_cast<int>(n);
}
std::string Choice(const Json &j, const std::string &k, std::string value,
                   std::initializer_list<const char *> choices) {
  value = String(j, k, value);
  bool found = false;
  for (auto c : choices)
    if (value == c)
      found = true;
  Require(found, k);
  return value;
}
std::string Profile(const Json &j, const std::string &key, std::string def,
                    bool rule = false) {
  if (rule)
    return Choice(
        j, key, def,
        {"fluid", "discreet", "cinema", "bounce", "genie", "wobbly", "off"});
  return Choice(
      j, key, def,
      {"fluid", "discreet", "cinema", "bounce", "genie", "wobbly", "custom"});
}
Mode ReadMode(const Json &j, const std::string &key, Mode def) {
  if (!j.contains(key))
    return def;
  auto v =
      Choice(j, key, "zoom",
             {"none", "fade", "slide", "zoom", "spring", "genie", "wobbly"});
  if (v == "none")
    return Mode::none;
  if (v == "fade")
    return Mode::fade;
  if (v == "slide")
    return Mode::slide;
  if (v == "spring")
    return Mode::spring;
  if (v == "genie")
    return Mode::genie;
  if (v == "wobbly")
    return Mode::wobbly;
  return Mode::zoom;
}
Curve ReadCurve(const Json &j) {
  auto c = Choice(j, "curve", "out", {"linear", "smooth", "out", "inout"});
  if (c == "linear")
    return Curve::linear;
  if (c == "smooth")
    return Curve::smooth;
  if (c == "inout")
    return Curve::inout;
  return Curve::out;
}
Motion Preset(const std::string &profile, Event event) {
  Motion m;
  bool dock = event == Event::minimize || event == Event::restore;
  m.duration = dock ? 360 : 280;
  m.mode = dock ? Mode::slide : Mode::zoom;
  if (profile == "discreet") {
    m.mode = Mode::fade;
    m.duration = 150;
    m.opacity = 0.35;
  } else if (profile == "cinema") {
    m.mode = dock ? Mode::genie : Mode::zoom;
    m.duration = dock ? 650 : 460;
    m.scale = 0.68;
    m.curve = Curve::inout;
  } else if (profile == "bounce") {
    m.mode = Mode::spring;
    m.duration = 520;
    m.bounce = 0.26;
    m.scale = 0.65;
    m.damping = 6;
  } else if (profile == "genie") {
    m.mode = Mode::genie;
    m.duration = 560;
    m.opacity = 0.16;
    m.curve = Curve::smooth;
  } else if (profile == "wobbly") {
    m.mode = Mode::wobbly;
    m.duration = 680;
    m.wobble = 34;
    m.frequency = 3;
  } else if (profile == "off")
    m.mode = Mode::none;
  return m;
}
void ValidateColor(const std::string &s, const char *key) {
  Require((s.size() == 7 || s.size() == 9) && s[0] == '#', key);
  for (size_t i = 1; i < s.size(); ++i)
    Require(std::isxdigit(static_cast<unsigned char>(s[i])) != 0, key);
}
} // namespace
std::vector<std::string> ApplicationList(const std::string &text) {
  std::string normalized = text;
  for (char &c : normalized)
    if (c == ',' || c == ';' || c == '\r')
      c = '\n';
  std::istringstream lines(normalized);
  std::vector<std::string> result;
  std::string line;
  while (std::getline(lines, line)) {
    size_t a = line.find_first_not_of(" \t"), b = line.find_last_not_of(" \t");
    if (a == std::string::npos)
      continue;
    line = Lower(line.substr(a, b - a + 1));
    Require(line.size() > 4 && line.size() <= 260 && line.ends_with(".exe") &&
                line.find_first_of("/\\:*?\"<>|") == std::string::npos,
            "applications (.exe uniquement)");
    if (std::find(result.begin(), result.end(), line) == result.end())
      result.push_back(line);
    Require(result.size() <= 32, "32 applications maximum");
  }
  return result;
}
bool MatchesKind(const std::string &filter, const std::string &kind) {
  return filter == "all" || filter == kind;
}
ParseResult ParseSettings(const Json &snapshot) {
  Config c;
  try {
    Require(snapshot.is_object(), "snapshot");
    const auto &s = snapshot.contains("deviceSettings")
                        ? snapshot.at("deviceSettings")
                        : snapshot;
    Require(s.is_object(), "deviceSettings");
    c.enabled = Bool(s, "enabled", true);
    c.respectReducedMotion = Bool(s, "respectReducedMotion", true);
    c.skipFullscreen = Bool(s, "skipFullscreen", true);
    c.previewVisible = Bool(s, "previewVisible", false);
    c.allowTranslucent = Bool(s, "allowTranslucent", false);
    c.hideEntering = Bool(s, "hideEntering", true);
    c.profile = Profile(s, "profile", "fluid");
    c.windowTypes =
        Choice(s, "windowTypes", "normal", {"normal", "dialog", "all"});
    c.applications = ApplicationList(String(s, "applications", "notepad.exe"));
    c.excluded = ApplicationList(String(s, "excludedApplications", ""));
    c.speed = Number(s, "speed", 100, 25, 250);
    c.intensity = Number(s, "intensity", 100, 0, 200);
    Motion custom;
    custom.curve = ReadCurve(s);
    custom.origin = Choice(
        s, "origin", "bottom",
        {"center", "bottom", "top", "left", "right", "cursor", "taskbar"});
    custom.distance = Number(s, "slideDistance", 54, 0, 400);
    custom.scale = Number(s, "startSize", 86, 10, 100) / 100;
    custom.opacity = Number(s, "startOpacity", 8, 0, 100) / 100;
    custom.bounce = Number(s, "bounce", 14, 0, 60) / 100;
    custom.damping = Number(s, "damping", 7, 2, 16);
    custom.wobble = Number(s, "wobbleAmount", 18, 0, 120);
    custom.frequency = Number(s, "wobbleFrequency", 3, 1, 8);
    const char *names[] = {"open", "close", "minimize", "restore"};
    for (int i = 0; i < 4; ++i) {
      std::string name = names[i];
      c.transitions[i] = Bool(s, name + "Enabled", true);
      auto m = custom;
      m.mode = ReadMode(s, name + "Mode", i >= 2 ? Mode::slide : Mode::zoom);
      m.duration = Integer(s, name + "Duration", i >= 2 ? 360 : 280, 80, 2000);
      c.motions[i] = m;
    }
    c.fps = Integer(s, "frameRate", 60, 30, 60);
    Require(c.fps == 30 || c.fps == 60, "frameRate");
    c.cacheInterval = Integer(s, "cacheInterval", 400, 100, 2000);
    c.cacheMaxAge = Integer(s, "cacheMaxAge", 2000, 250, 5000);
    c.maxConcurrent = Integer(s, "maxConcurrent", 3, 1, 4);
    c.maxSnapshots = Integer(s, "maxSnapshots", 12, 1, 24);
    c.mesh = Integer(s, "meshQuality", 20, 8, 40);
    c.radius = Number(s, "cornerRadius", 12, 0, 48);
    c.shadow = Number(s, "shadowSize", 24, 0, 48);
    c.shadowOpacity = Number(s, "shadowOpacity", 22, 0, 60) / 100;
    c.borderWidth = Number(s, "borderWidth", 2, 0, 8);
    c.borderStyle =
        Choice(s, "borderStyle", "none", {"none", "solid", "gradient"});
    c.color1 = String(s, "color1", "#64BFFAFF");
    c.color2 = String(s, "color2", "#DF83FFFF");
    ValidateColor(c.color1, "color1");
    ValidateColor(c.color2, "color2");
    for (int i = 0; i < 3; ++i) {
      auto &r = c.rules[i];
      std::string p = "rule" + std::to_string(i + 1);
      r.enabled = Bool(s, p + "Enabled", false);
      r.application = String(s, p + "Application", "");
      r.kind = Choice(s, p + "WindowType", "all", {"normal", "dialog", "all"});
      r.profile = Profile(s, p + "Profile", "fluid", true);
      r.speed = Number(s, p + "Speed", 100, 25, 250);
      if (!r.application.empty()) {
        auto apps = ApplicationList(r.application);
        Require(apps.size() == 1, p + "Application");
        r.application = apps[0];
      }
      Require(!r.enabled || !r.application.empty(),
              p + "Application (nom requis)");
    }
    return {std::move(c), {}};
  } catch (const std::exception &e) {
    return {Config{}, e.what()};
  }
}
Motion ResolveMotion(const Config &c, Event event, const std::string &app,
                     const std::string &kind) {
  size_t i = static_cast<size_t>(event);
  auto profile = c.profile;
  double speed = c.speed;
  for (const auto &r : c.rules)
    if (r.enabled && r.application == app && MatchesKind(r.kind, kind)) {
      profile = r.profile;
      speed = r.speed;
    }
  Motion m = profile == "custom" ? c.motions[i] : Preset(profile, event);
  // Spatial controls remain direct for every preset. Profiles only set the
  // motion library.
  m.origin = c.motions[i].origin;
  m.duration = std::clamp(
      static_cast<int>(std::lround(m.duration * 100.0 / speed)), 40, 4000);
  double amount = c.intensity / 100;
  m.distance *= amount;
  m.scale = std::clamp(1 - (1 - m.scale) * amount, 0.05, 1.0);
  m.bounce *= amount;
  m.wobble *= amount;
  if (!c.enabled || !c.transitions[i])
    m.mode = Mode::none;
  return m;
}
uint32_t Color(const std::string &color) {
  uint32_t rgb =
      static_cast<uint32_t>(std::stoul(color.substr(1, 6), nullptr, 16));
  uint32_t a =
      color.size() == 9
          ? static_cast<uint32_t>(std::stoul(color.substr(7, 2), nullptr, 16))
          : 255;
  return (a << 24) | (rgb & 0xffffff);
}
} // namespace wa
