#include "runtime.hpp"
#include "capture.hpp"
#include <algorithm>
#include <atomic>
#include <dwmapi.h>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>

namespace wa {
namespace {
constexpr UINT kEventMessage = WM_APP + 10;
HWND eventWindow{};
std::atomic<int> pendingEvents{0};
void CALLBACK EventCallback(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG object,
                            LONG child, DWORD, DWORD) {
  if (!hwnd || object != OBJID_WINDOW || child != 0 || !eventWindow)
    return;
  int count = pendingEvents.fetch_add(1);
  if (count >= 256 || !PostMessageW(eventWindow, kEventMessage, event,
                                    reinterpret_cast<LPARAM>(hwnd)))
    pendingEvents.fetch_sub(1);
}
LRESULT CALLBACK OverlayProc(HWND hwnd, UINT message, WPARAM w, LPARAM l) {
  if (message == WM_NCHITTEST)
    return HTTRANSPARENT;
  if (message == WM_MOUSEACTIVATE)
    return MA_NOACTIVATE;
  return DefWindowProcW(hwnd, message, w, l);
}
bool Reduced() {
  BOOL ui = TRUE;
  ANIMATIONINFO info{sizeof(info), TRUE};
  if (!SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &ui, 0))
    return true;
  if (!SystemParametersInfoW(SPI_GETANIMATION, sizeof(info), &info, 0))
    return true;
  return !ui || !info.iMinAnimate;
}
uint64_t Creation(HANDLE process) {
  FILETIME a{}, b{}, c{}, d{};
  if (!GetProcessTimes(process, &a, &b, &c, &d))
    return 0;
  return (static_cast<uint64_t>(a.dwHighDateTime) << 32) | a.dwLowDateTime;
}
struct Identity {
  DWORD pid{}, tid{};
  uint64_t creation{};
  std::string app, kind;
};
std::optional<Identity> Identify(HWND hwnd) {
  Identity id;
  id.tid = GetWindowThreadProcessId(hwnd, &id.pid);
  if (!id.tid)
    return {};
  HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, id.pid);
  if (!p)
    return {};
  wchar_t path[32768]{};
  DWORD size = 32768;
  bool found = QueryFullProcessImageNameW(p, 0, path, &size) != FALSE;
  id.creation = Creation(p);
  CloseHandle(p);
  if (!found || !id.creation)
    return {};
  auto name = std::filesystem::path(path).filename().wstring();
  std::transform(name.begin(), name.end(), name.begin(), ::towlower);
  int len = WideCharToMultiByte(CP_UTF8, 0, name.data(),
                                static_cast<int>(name.size()), nullptr, 0,
                                nullptr, nullptr);
  id.app.resize(len);
  WideCharToMultiByte(CP_UTF8, 0, name.data(), static_cast<int>(name.size()),
                      id.app.data(), len, nullptr, nullptr);
  wchar_t klass[256]{};
  GetClassNameW(hwnd, klass, 256);
  id.kind =
      wcscmp(klass, L"#32770") == 0 || GetWindow(hwnd, GW_OWNER) != nullptr
          ? "dialog"
          : "normal";
  return id;
}
bool Same(HWND hwnd, const Identity &old) {
  auto id = Identify(hwnd);
  return id && id->pid == old.pid && id->tid == old.tid &&
         id->creation == old.creation;
}
bool Listed(const std::vector<std::string> &list, const std::string &app) {
  return std::find(list.begin(), list.end(), app) != list.end();
}
bool Eligible(HWND hwnd, const Identity &id, const Config &c) {
  if (!c.enabled || GetAncestor(hwnd, GA_ROOT) != hwnd ||
      !MatchesKind(c.windowTypes, id.kind) || Listed(c.excluded, id.app))
    return false;
  wchar_t klass[256]{};
  GetClassNameW(hwnd, klass, 256);
  if (wcscmp(klass, L"MwAnimationPreviewControl") == 0 ||
      wcscmp(klass, L"MwWindowAnimationOverlay") == 0)
    return false;
  const std::vector<std::string> system = {"dwm.exe",
                                           "explorer.exe",
                                           "codex.exe",
                                           "consent.exe",
                                           "logonui.exe",
                                           "winlogon.exe",
                                           "shellexperiencehost.exe",
                                           "startmenuexperiencehost.exe",
                                           "searchhost.exe"};
  if (Listed(system, id.app))
    return false;
  DWORD cloaked{};
  if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked,
                                      sizeof(cloaked))) &&
      cloaked)
    return false;
  bool selected =
      Listed(c.applications, id.app) ||
      (c.previewVisible && id.app == "window-animations-preview.exe");
  for (const auto &r : c.rules)
    if (r.enabled && r.application == id.app && MatchesKind(r.kind, id.kind))
      selected = true;
  if (!selected)
    return false;
  LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
  if (ex & WS_EX_TOOLWINDOW)
    return false;
  if (ex & WS_EX_LAYERED) {
    COLORREF key{};
    BYTE alpha{255};
    DWORD flags{};
    if (GetLayeredWindowAttributes(hwnd, &key, &alpha, &flags) &&
        (flags & LWA_ALPHA) && alpha == 0)
      return false;
  }
  if (!c.allowTranslucent) {
    int backdrop = 0;
    DwmGetWindowAttribute(hwnd, static_cast<DWMWINDOWATTRIBUTE>(38), &backdrop,
                          sizeof(backdrop));
    if ((ex & WS_EX_LAYERED) || backdrop > 1)
      return false;
  }
  DWORD affinity = 0;
  if (GetWindowDisplayAffinity(hwnd, &affinity) && affinity != WDA_NONE)
    return false;
  return true;
}
Rect Bounds(HWND hwnd) {
  RECT r{};
  if (FAILED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &r,
                                   sizeof(r))))
    GetWindowRect(hwnd, &r);
  return {static_cast<double>(r.left), static_cast<double>(r.top),
          static_cast<double>(r.right - r.left),
          static_cast<double>(r.bottom - r.top)};
}
HMONITOR SourceMonitor(const Rect &r) {
  RECT bounds{static_cast<LONG>(r.x), static_cast<LONG>(r.y),
              static_cast<LONG>(r.x + r.w), static_cast<LONG>(r.y + r.h)};
  return MonitorFromRect(&bounds, MONITOR_DEFAULTTONEAREST);
}
bool OnScreen(HWND, const Rect &r, bool skipFullscreen) {
  MONITORINFO mi{sizeof(mi)};
  if (!GetMonitorInfoW(SourceMonitor(r), &mi))
    return false;
  if (r.x + r.w <= mi.rcMonitor.left || r.y + r.h <= mi.rcMonitor.top ||
      r.x >= mi.rcMonitor.right || r.y >= mi.rcMonitor.bottom)
    return false;
  return !skipFullscreen ||
         !(r.x <= mi.rcMonitor.left && r.y <= mi.rcMonitor.top &&
           r.x + r.w >= mi.rcMonitor.right && r.y + r.h >= mi.rcMonitor.bottom);
}
Point Anchor(HWND, const Rect &r, const std::string &origin) {
  Point center{r.x + r.w / 2, r.y + r.h / 2};
  if (origin == "center")
    return center;
  if (origin == "cursor") {
    POINT pt{};
    GetCursorPos(&pt);
    return {static_cast<double>(pt.x), static_cast<double>(pt.y)};
  }
  MONITORINFO mi{sizeof(mi)};
  GetMonitorInfoW(SourceMonitor(r), &mi);
  if (origin == "left")
    return {static_cast<double>(mi.rcWork.left), center.y};
  if (origin == "right")
    return {static_cast<double>(mi.rcWork.right), center.y};
  if (origin == "top")
    return {center.x, static_cast<double>(mi.rcWork.top)};
  if (origin == "taskbar") {
    if (mi.rcWork.left > mi.rcMonitor.left)
      return {static_cast<double>(mi.rcWork.left), center.y};
    if (mi.rcWork.right < mi.rcMonitor.right)
      return {static_cast<double>(mi.rcWork.right), center.y};
    if (mi.rcWork.top > mi.rcMonitor.top)
      return {center.x, static_cast<double>(mi.rcWork.top)};
  }
  return {std::clamp(center.x, static_cast<double>(mi.rcWork.left + 20),
                     static_cast<double>(mi.rcWork.right - 20)),
          static_cast<double>(mi.rcWork.bottom)};
}
bool Present(HWND hwnd, const Frame &frame) {
  BITMAPINFO info{};
  info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = frame.image.width;
  info.bmiHeader.biHeight = -frame.image.height;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  HDC dc = CreateCompatibleDC(nullptr);
  void *bits{};
  HBITMAP bmp = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (!bmp || !dc) {
    if (bmp)
      DeleteObject(bmp);
    if (dc)
      DeleteDC(dc);
    return false;
  }
  auto old = SelectObject(dc, bmp);
  memcpy(bits, frame.image.pixels.data(), frame.image.pixels.size() * 4);
  POINT dest{frame.x, frame.y}, src{};
  SIZE size{frame.image.width, frame.image.height};
  BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
  bool ok = UpdateLayeredWindow(hwnd, nullptr, &dest, &size, dc, &src, 0,
                                &blend, ULW_ALPHA) != FALSE;
  if (ok)
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
  SelectObject(dc, old);
  DeleteObject(bmp);
  DeleteDC(dc);
  return ok;
}
const char *EventName(Event e) {
  const char *n[] = {"open", "close", "minimize", "restore"};
  return n[static_cast<int>(e)];
}
const char *ModeName(Mode m) {
  const char *n[] = {"none",   "fade",  "slide", "zoom",
                     "spring", "genie", "wobbly"};
  return n[static_cast<int>(m)];
}
} // namespace
std::wstring Wide(const std::string &s) {
  int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(),
                              static_cast<int>(s.size()), nullptr, 0);
  std::wstring r(n, L'\0');
  if (n)
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(),
                        static_cast<int>(s.size()), r.data(), n);
  return r;
}
std::wstring ExeDirectory() {
  wchar_t path[32768]{};
  GetModuleFileNameW(nullptr, path, 32768);
  return std::filesystem::path(path).parent_path().wstring();
}
bool WriteBitmap(const std::wstring &path, const Image &image) {
  BITMAPFILEHEADER file{};
  BITMAPINFOHEADER info{};
  file.bfType = 0x4d42;
  file.bfOffBits = sizeof(file) + sizeof(info);
  file.bfSize = file.bfOffBits + static_cast<DWORD>(image.pixels.size() * 4);
  info.biSize = sizeof(info);
  info.biWidth = image.width;
  info.biHeight = -image.height;
  info.biPlanes = 1;
  info.biBitCount = 32;
  info.biCompression = BI_RGB;
  std::ofstream f(std::filesystem::path(path), std::ios::binary);
  f.write(reinterpret_cast<const char *>(&file), sizeof(file));
  f.write(reinterpret_cast<const char *>(&info), sizeof(info));
  f.write(reinterpret_cast<const char *>(image.pixels.data()),
          static_cast<std::streamsize>(image.pixels.size() * 4));
  return f.good();
}
struct Runtime::Impl {
  enum class Phase { visible, hidden, minimized };
  struct Window {
    Identity id;
    Rect rect;
    Image snapshot;
    ULONGLONG captured{};
    Phase phase{Phase::visible};
    UINT dpi{96};
    ULONGLONG attempted{};
  };
  struct Animation {
    HWND source{}, overlay{};
    Identity id;
    Rect rect;
    Image snapshot;
    Motion motion;
    Point anchor;
    ULONGLONG started{};
    bool entering{}, saved{};
    int lease{-1};
    Event event{};
    Config style;
  };
  Config config;
  std::map<HWND, Window> windows;
  std::vector<Animation> animations;
  struct Pending {
    DWORD event;
    HWND window;
    ULONGLONG due;
  };
  std::vector<Pending> pending;
  std::vector<HWINEVENTHOOK> hooks;
  std::unique_ptr<Watchdog> watchdog;
  std::unique_ptr<CaptureClient> capture;
  std::wstring scratch, capturePath;
  HANDLE preview{};
  bool reduced{}, started{};
  ULONGLONG lastCache{}, lastStatus{};
  uint64_t frames{}, captures{}, captureFailures{}, skipped{}, cacheBytes{},
      cancelled{};
  std::array<uint64_t, 4> transitions{};
  std::string lastMode;
  double lastFrameMs{};
  bool watchdogAvailable{};
  void End(Animation &a) {
    if (watchdog)
      watchdog->Restore(a.lease);
    if (a.overlay)
      DestroyWindow(a.overlay);
    a.overlay = nullptr;
  }
  void Preview() {
    if (preview && WaitForSingleObject(preview, 0) == WAIT_OBJECT_0) {
      CloseHandle(preview);
      preview = nullptr;
    }
    if (config.previewVisible && !preview) {
      auto exe = ExeDirectory() + L"\\window-animations-preview.exe";
      auto cmd = L"\"" + exe + L"\"";
      STARTUPINFOW si{sizeof(si)};
      PROCESS_INFORMATION pi{};
      if (CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, 0,
                         nullptr, nullptr, &si, &pi)) {
        CloseHandle(pi.hThread);
        preview = pi.hProcess;
      }
    }
    if (!config.previewVisible && preview) {
      DWORD pid = GetProcessId(preview);
      EnumWindows(
          [](HWND h, LPARAM l) -> BOOL {
            DWORD p{};
            GetWindowThreadProcessId(h, &p);
            if (p == static_cast<DWORD>(l))
              PostMessageW(h, WM_CLOSE, 0, 0);
            return TRUE;
          },
          pid);
      CloseHandle(preview);
      preview = nullptr;
    }
  }
};
Runtime::Runtime() : impl_(std::make_unique<Impl>()) {}
Runtime::~Runtime() { Stop(); }
bool Runtime::Start(const Config &config, const std::wstring &scratch) {
  if (impl_->started)
    return false;
  impl_->config = config;
  impl_->scratch = scratch;
  impl_->reduced = Reduced();
  wchar_t captures[32768]{};
  DWORD n = GetEnvironmentVariableW(L"MW_WINDOW_ANIMATIONS_TEST_CAPTURE",
                                    captures, 32768);
  if (n && n < 32768)
    impl_->capturePath = captures;
  WNDCLASSW wc{};
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.lpfnWndProc = OverlayProc;
  wc.lpszClassName = L"MwWindowAnimationOverlay";
  RegisterClassW(&wc);
  impl_->capture = std::make_unique<CaptureClient>();
  if (!impl_->capture->Start())
    return false;
  impl_->watchdog = std::make_unique<Watchdog>();
  impl_->watchdogAvailable = impl_->watchdog->Start();
  eventWindow = window;
  for (auto range : {std::pair<DWORD, DWORD>{EVENT_SYSTEM_MINIMIZESTART,
                                             EVENT_SYSTEM_MINIMIZEEND},
                     {EVENT_OBJECT_DESTROY, EVENT_OBJECT_SHOW}}) {
    auto h = SetWinEventHook(range.first, range.second, nullptr, EventCallback,
                             0, 0, WINEVENT_OUTOFCONTEXT);
    if (!h) {
      Stop();
      return false;
    }
    impl_->hooks.push_back(h);
  }
  impl_->started = true;
  EnumWindows(
      [](HWND h, LPARAM p) -> BOOL {
        auto self = reinterpret_cast<Runtime *>(p);
        auto &s = *self->impl_;
        auto id = Identify(h);
        if (id && IsWindowVisible(h) && !IsIconic(h) &&
            Eligible(h, *id, s.config) &&
            s.windows.size() < static_cast<size_t>(s.config.maxSnapshots)) {
          auto r = Bounds(h);
          if (OnScreen(h, r, s.config.skipFullscreen))
            s.windows.emplace(
                h, Impl::Window{*id, r, {}, 0, Impl::Phase::visible});
        }
        return TRUE;
      },
      reinterpret_cast<LPARAM>(this));
  SetTimer(window, 1, 1000 / config.fps, nullptr);
  impl_->Preview();
  return true;
}
void Runtime::CancelAll() {
  impl_->pending.clear();
  for (auto &a : impl_->animations)
    impl_->End(a);
  impl_->cancelled += impl_->animations.size();
  impl_->animations.clear();
}
void Runtime::Update(const Config &config) {
  CancelAll();
  impl_->windows.clear();
  impl_->cacheBytes = 0;
  impl_->config = config;
  impl_->reduced = Reduced();
  KillTimer(window, 1);
  SetTimer(window, 1, 1000 / config.fps, nullptr);
  impl_->lastCache = 0;
  impl_->Preview();
}
void Runtime::Stop() {
  eventWindow = nullptr;
  for (auto h : impl_->hooks)
    UnhookWinEvent(h);
  impl_->hooks.clear();
  CancelAll();
  KillTimer(window, 1);
  impl_->windows.clear();
  impl_->cacheBytes = 0;
  impl_->watchdog.reset();
  impl_->capture.reset();
  impl_->config.previewVisible = false;
  impl_->Preview();
  impl_->started = false;
}
void Runtime::SystemChanged() {
  impl_->reduced = Reduced();
  CancelAll();
}
void Runtime::OnEvent(DWORD event, HWND hwnd, bool captureReady) {
  if (!captureReady)
    pendingEvents.fetch_sub(1);
  auto &s = *impl_;
  auto now = GetTickCount64();
  if (!s.started || !s.config.enabled)
    return;
  auto it = s.windows.find(hwnd);
  Event transition = Event::open;
  bool entering = true;
  if (event == EVENT_SYSTEM_MINIMIZESTART) {
    transition = Event::minimize;
    entering = false;
  } else if (event == EVENT_SYSTEM_MINIMIZEEND) {
    transition = Event::restore;
  } else if (event == EVENT_OBJECT_DESTROY) {
    transition = Event::close;
    entering = false;
  } else if (event != EVENT_OBJECT_SHOW)
    return;
  auto id =
      entering ? Identify(hwnd)
               : (it != s.windows.end() ? std::optional<Identity>(it->second.id)
                                        : Identify(hwnd));
  if (!id)
    return;
  if (entering && (!Eligible(hwnd, *id, s.config) || !IsWindowVisible(hwnd) ||
                   IsIconic(hwnd)))
    return;
  if (entering && !captureReady) {
    if (std::none_of(s.pending.begin(), s.pending.end(),
                     [hwnd](const auto &p) { return p.window == hwnd; }) &&
        s.pending.size() < 16)
      s.pending.push_back({event, hwnd, now + 64});
    return;
  }
  if (!entering)
    std::erase_if(s.pending,
                  [hwnd](const auto &p) { return p.window == hwnd; });
  if (it != s.windows.end() && entering && !Same(hwnd, it->second.id)) {
    s.cacheBytes -= it->second.snapshot.pixels.size() * 4;
    s.windows.erase(it);
    it = s.windows.end();
  }
  if (it != s.windows.end()) {
    if (entering) {
      if (it->second.phase == Impl::Phase::visible)
        return;
      if (it->second.phase == Impl::Phase::minimized)
        transition = Event::restore;
    } else if (it->second.phase != Impl::Phase::visible)
      return;
  }
  if (it == s.windows.end()) {
    if (!entering)
      return;
    if (s.windows.size() >= static_cast<size_t>(s.config.maxSnapshots)) {
      ++s.skipped;
      return;
    }
    it = s.windows
             .emplace(
                 hwnd,
                 Impl::Window{*id, Bounds(hwnd), {}, 0, Impl::Phase::visible})
             .first;
  }
  auto &w = it->second;
  for (auto a = s.animations.begin(); a != s.animations.end();)
    if (a->source == hwnd) {
      s.End(*a);
      a = s.animations.erase(a);
    } else
      ++a;
  if (entering) {
    w.rect = Bounds(hwnd);
    if (UINT dpi = GetDpiForWindow(hwnd))
      w.dpi = dpi;
    w.phase = Impl::Phase::visible;
  } else
    w.phase = transition == Event::minimize ? Impl::Phase::minimized
                                            : Impl::Phase::hidden;
  auto motion = ResolveMotion(s.config, transition, id->app, id->kind);
  Config style = s.config;
  double dpi = w.dpi / 96.0;
  motion.distance *= dpi;
  motion.wobble *= dpi;
  style.radius *= dpi;
  style.shadow *= dpi;
  style.borderWidth *= dpi;
  if (motion.mode == Mode::none ||
      (s.config.respectReducedMotion && s.reduced) ||
      s.animations.size() >= static_cast<size_t>(s.config.maxConcurrent) ||
      !OnScreen(hwnd, w.rect, s.config.skipFullscreen)) {
    ++s.skipped;
    return;
  }
  Image snapshot;
  if (entering) {
    snapshot = s.capture->Request(hwnd, w.rect, s.config.allowTranslucent);
    if (!Same(hwnd, *id))
      snapshot = {};
    if (snapshot.pixels.empty())
      ++s.captureFailures;
    else
      ++s.captures;
  } else if (now - w.captured <= static_cast<ULONGLONG>(s.config.cacheMaxAge))
    snapshot = w.snapshot;
  if (snapshot.pixels.empty()) {
    ++s.skipped;
    return;
  }
  HWND overlay =
      CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE |
                          WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
                      L"MwWindowAnimationOverlay", L"", WS_POPUP, 0, 0, 1, 1,
                      nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
  if (!overlay) {
    ++s.skipped;
    return;
  }
  // Show the first valid overlay before hiding the source; failed capture
  // never hides a window.
  auto anchor = Anchor(hwnd, w.rect, motion.origin);
  auto initial = Render(snapshot, w.rect, anchor, motion, style, 0, entering);
  if (!initial.valid || !Present(overlay, initial)) {
    DestroyWindow(overlay);
    ++s.skipped;
    return;
  }
  int lease = -1;
  if (entering && s.config.hideEntering && s.watchdogAvailable)
    lease = s.watchdog->Hide(hwnd);
  s.animations.push_back({hwnd, overlay, *id, w.rect, std::move(snapshot),
                          motion, anchor, now, entering, false, lease,
                          transition, std::move(style)});
  ++s.transitions[static_cast<size_t>(transition)];
  s.lastMode = ModeName(motion.mode);
}
void Runtime::Tick() {
  auto &s = *impl_;
  if (!s.started)
    return;
  auto now = GetTickCount64();
  if (s.watchdog)
    s.watchdog->Beat();
  if (s.watchdogAvailable && s.watchdog->Tripped()) {
    CancelAll();
    s.watchdogAvailable = false;
  }
  auto pending = s.pending;
  bool capturedThisTick = false;
  for (const auto &p : pending)
    if (p.due <= now) {
      std::erase_if(s.pending, [&](const auto &entry) {
        return entry.window == p.window;
      });
      OnEvent(p.event, p.window, true);
      // At most one potentially blocking entering capture per message-loop
      // turn, so queued settings/shutdown and the watchdog can make progress.
      capturedThisTick = true;
      break;
    }
  for (auto it = s.animations.begin(); it != s.animations.end();) {
    double t = static_cast<double>(now - it->started) / it->motion.duration;
    if (t >= 1 || (it->entering && !Same(it->source, it->id))) {
      s.End(*it);
      it = s.animations.erase(it);
      continue;
    }
    LARGE_INTEGER before{}, after{}, frequency{};
    QueryPerformanceCounter(&before);
    QueryPerformanceFrequency(&frequency);
    auto frame = Render(it->snapshot, it->rect, it->anchor, it->motion,
                        it->style, t, it->entering);
    if (!frame.valid || !Present(it->overlay, frame)) {
      s.End(*it);
      it = s.animations.erase(it);
      ++s.skipped;
      continue;
    }
    QueryPerformanceCounter(&after);
    s.lastFrameMs =
        1000.0 * (after.QuadPart - before.QuadPart) / frequency.QuadPart;
    ++s.frames;
    if (!s.capturePath.empty() && !it->saved && t >= .38) {
      std::filesystem::create_directories(s.capturePath);
      WriteBitmap(s.capturePath + L"\\" +
                      Wide(std::string(EventName(it->event)) + "-" +
                           ModeName(it->motion.mode)) +
                      L".bmp",
                  frame.image);
      it->saved = true;
    }
    ++it;
  }
  if (now - s.lastCache >= static_cast<ULONGLONG>(s.config.cacheInterval)) {
    s.lastCache = now;
    for (auto it = s.windows.begin(); it != s.windows.end();)
      if (!Same(it->first, it->second.id)) {
        s.cacheBytes -= it->second.snapshot.pixels.size() * 4;
        it = s.windows.erase(it);
      } else
        ++it;
    EnumWindows(
        [](HWND h, LPARAM p) -> BOOL {
          auto self = reinterpret_cast<Runtime *>(p);
          auto &a = *self->impl_;
          if (a.windows.contains(h) ||
              std::any_of(a.pending.begin(), a.pending.end(),
                          [h](const auto &p) { return p.window == h; }) ||
              a.windows.size() >= static_cast<size_t>(a.config.maxSnapshots) ||
              !IsWindowVisible(h) || IsIconic(h))
            return TRUE;
          auto id = Identify(h);
          if (id && Eligible(h, *id, a.config)) {
            auto r = Bounds(h);
            if (OnScreen(h, r, a.config.skipFullscreen))
              a.windows.emplace(
                  h, Impl::Window{*id, r, {}, 0, Impl::Phase::visible});
          }
          return TRUE;
        },
        reinterpret_cast<LPARAM>(this));
  }
  if (!capturedThisTick && s.config.enabled &&
      (!s.config.respectReducedMotion || !s.reduced)) {
    auto candidate = s.windows.end();
    for (auto it = s.windows.begin(); it != s.windows.end(); ++it) {
      auto &[h, w] = *it;
      if (w.phase != Impl::Phase::visible || !IsWindowVisible(h) ||
          IsIconic(h) || !OnScreen(h, Bounds(h), s.config.skipFullscreen) ||
          now - w.attempted < static_cast<ULONGLONG>(s.config.cacheInterval) ||
          std::any_of(s.animations.begin(), s.animations.end(),
                      [h](const auto &a) { return a.source == h; }))
        continue;
      if (candidate == s.windows.end() ||
          w.attempted < candidate->second.attempted)
        candidate = it;
    }
    if (candidate != s.windows.end()) {
      auto &[h, w] = *candidate;
      // Rotate fairly between windows, including failures. Retrying a hung
      // target cannot starve other captures or spin every frame.
      w.attempted = now;
      w.rect = Bounds(h);
      if (UINT dpi = GetDpiForWindow(h))
        w.dpi = dpi;
      auto image = s.capture->Request(h, w.rect, s.config.allowTranslucent);
      if (!Same(h, w.id))
        image = {};
      if (image.pixels.empty()) {
        ++s.captureFailures;
      } else {
        uint64_t bytes = s.cacheBytes - w.snapshot.pixels.size() * 4 +
                         image.pixels.size() * 4;
        if (bytes > 64 * 1024 * 1024) {
          ++s.skipped;
        } else {
          s.cacheBytes = bytes;
          w.snapshot = std::move(image);
          w.captured = GetTickCount64();
          ++s.captures;
        }
      }
    }
  }
  if (now - s.lastStatus >= 500 && !s.scratch.empty()) {
    s.lastStatus = now;
    std::filesystem::create_directories(s.scratch);
    auto path =
        std::filesystem::path(s.scratch) / L"window-animations-status.json";
    auto temp = path;
    temp += L".tmp";
    {
      std::ofstream f(temp);
      f << Status().dump(2);
    }
    MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING);
  }
}
Json Runtime::Status() const {
  const auto &s = *impl_;
  return {
      {"schemaVersion", 1},
      {"profile", s.config.profile},
      {"enabled", s.config.enabled},
      {"reducedMotion", s.reduced},
      {"respectReducedMotion", s.config.respectReducedMotion},
      {"active", s.animations.size()},
      {"tracked", s.windows.size()},
      {"cacheBytes", s.cacheBytes},
      {"frames", s.frames},
      {"captures", s.captures},
      {"captureFailures", s.captureFailures},
      {"skipped", s.skipped},
      {"cancelled", s.cancelled},
      {"open", s.transitions[0]},
      {"close", s.transitions[1]},
      {"minimize", s.transitions[2]},
      {"restore", s.transitions[3]},
      {"lastMode", s.lastMode},
      {"lastFrameMs", s.lastFrameMs},
      {"watchdogAvailable", s.watchdogAvailable},
      {"windowsBuildMechanism",
       "external WinEvent / isolated PrintWindow / layered bitmap overlay"}};
}
} // namespace wa
