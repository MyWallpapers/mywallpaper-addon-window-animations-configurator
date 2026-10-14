#include "capture.hpp"
#include "runtime.hpp"
#include <algorithm>
#include <array>

namespace wa {
namespace {
constexpr size_t kMaxPixels = 4'000'000;
struct Shared {
  volatile LONG request{}, complete{}, ok{};
  uint64_t hwnd{};
  int width{}, height{};
  std::array<uint32_t, kMaxPixels> pixels;
};
bool RenderWindow(Shared &data) {
  HWND hwnd = reinterpret_cast<HWND>(data.hwnd);
  RECT rect{};
  DWORD affinity{};
  if (!IsWindowVisible(hwnd) || IsIconic(hwnd) || !GetWindowRect(hwnd, &rect) ||
      (GetWindowDisplayAffinity(hwnd, &affinity) && affinity != WDA_NONE))
    return false;
  int w = rect.right - rect.left, h = rect.bottom - rect.top;
  if (w < 32 || h < 32 || static_cast<uint64_t>(w) * h > kMaxPixels)
    return false;
  HDC dc = CreateCompatibleDC(nullptr);
  BITMAPINFO info{};
  info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = w;
  info.bmiHeader.biHeight = -h;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  void *bits{};
  HBITMAP bitmap =
      CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (!dc || !bitmap) {
    if (bitmap)
      DeleteObject(bitmap);
    if (dc)
      DeleteDC(dc);
    return false;
  }
  auto old = SelectObject(dc, bitmap);
  std::fill_n(static_cast<uint32_t *>(bits), static_cast<size_t>(w) * h, 0u);
  // PrintWindow is deliberately isolated: a hung application can never hold
  // the add-on's message loop or a target application hook indefinitely.
  BOOL ok = PrintWindow(hwnd, dc, 2 /* PW_RENDERFULLCONTENT */);
  GdiFlush();
  if (ok) {
    data.width = w;
    data.height = h;
    memcpy(data.pixels.data(), bits, static_cast<size_t>(w) * h * 4);
  }
  SelectObject(dc, old);
  DeleteObject(bitmap);
  DeleteDC(dc);
  return ok != FALSE;
}
} // namespace
struct CaptureClient::State {
  HANDLE mapping{}, request{}, done{}, process{};
  Shared *data{};
  uint64_t generation{};
};
CaptureClient::CaptureClient() : state_(std::make_unique<State>()) {}
CaptureClient::~CaptureClient() { Close(); }
void CaptureClient::Close() {
  if (state_->process) {
    // Only this add-on's disposable capture helper is terminated, never a host.
    if (WaitForSingleObject(state_->process, 0) == WAIT_TIMEOUT)
      TerminateProcess(state_->process, 0);
    WaitForSingleObject(state_->process, 100);
    CloseHandle(state_->process);
    state_->process = nullptr;
  }
  if (state_->data) {
    UnmapViewOfFile(state_->data);
    state_->data = nullptr;
  }
  for (HANDLE *h : {&state_->mapping, &state_->request, &state_->done}) {
    if (*h)
      CloseHandle(*h);
    *h = nullptr;
  }
}
bool CaptureClient::Start() {
  Close();
  auto name = L"Local\\MwWindowCapture-" +
              std::to_wstring(GetCurrentProcessId()) + L"-" +
              std::to_wstring(GetTickCount64()) + L"-" +
              std::to_wstring(++state_->generation);
  state_->mapping =
      CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                         sizeof(Shared), name.c_str());
  if (!state_->mapping || GetLastError() == ERROR_ALREADY_EXISTS)
    return false;
  state_->data = static_cast<Shared *>(MapViewOfFile(
      state_->mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Shared)));
  if (!state_->data)
    return false;
  state_->request =
      CreateEventW(nullptr, FALSE, FALSE, (name + L"-request").c_str());
  state_->done = CreateEventW(nullptr, FALSE, FALSE, (name + L"-done").c_str());
  if (!state_->request || !state_->done)
    return false;
  auto exe = ExeDirectory() + L"\\companion.exe";
  auto command = L"\"" + exe + L"\" --capture \"" + name + L"\" " +
                 std::to_wstring(GetCurrentProcessId());
  STARTUPINFOW si{sizeof(si)};
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdInput = si.hStdOutput = si.hStdError = INVALID_HANDLE_VALUE;
  PROCESS_INFORMATION pi{};
  if (!CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, FALSE,
                      CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
    return false;
  CloseHandle(pi.hThread);
  state_->process = pi.hProcess;
  // The first request includes helper startup. Later requests reuse this
  // process.
  return true;
}
Image CaptureClient::Request(HWND hwnd, const Rect &rect, bool translucent) {
  Image image;
  if ((!state_->process ||
       WaitForSingleObject(state_->process, 0) != WAIT_TIMEOUT) &&
      !Start())
    return image;
  RECT raw{};
  if (!GetWindowRect(hwnd, &raw) || rect.w < 32 || rect.h < 32 ||
      rect.w * rect.h > kMaxPixels)
    return image;
  LONG request = InterlockedCompareExchange(&state_->data->request, 0, 0) + 1;
  state_->data->hwnd = reinterpret_cast<uint64_t>(hwnd);
  InterlockedExchange(&state_->data->request, request);
  ResetEvent(state_->done);
  SetEvent(state_->request);
  if (WaitForSingleObject(state_->done, 120) != WAIT_OBJECT_0 ||
      InterlockedCompareExchange(&state_->data->complete, 0, 0) != request) {
    Close(); // A late result from this generation can never become a new
             // capture.
    return image;
  }
  if (!InterlockedCompareExchange(&state_->data->ok, 0, 0))
    return image;
  int width = state_->data->width, height = state_->data->height;
  if (width <= 0 || height <= 0 ||
      static_cast<uint64_t>(width) * height > kMaxPixels)
    return image;
  image.width = static_cast<int>(rect.w);
  image.height = static_cast<int>(rect.h);
  image.pixels.resize(static_cast<size_t>(image.width) * image.height);
  int ox = static_cast<int>(rect.x) - raw.left,
      oy = static_cast<int>(rect.y) - raw.top;
  bool somePixel = false, someAlpha = false, invalidAlpha = false;
  bool clientPixel = false;
  for (int y = 0; y < image.height; ++y)
    for (int x = 0; x < image.width; ++x) {
      int sx = x + ox, sy = y + oy;
      uint32_t p =
          sx >= 0 && sy >= 0 && sx < width && sy < height
              ? state_->data->pixels[static_cast<size_t>(sy) * width + sx]
              : 0;
      somePixel |= (p & 0xffffff) != 0;
      someAlpha |= (p >> 24) != 0;
      uint32_t alpha = p >> 24;
      invalidAlpha |= (p & 255) > alpha || ((p >> 8) & 255) > alpha ||
                      ((p >> 16) & 255) > alpha;
      if (y > std::min(56, image.height / 3) && x > 8 && x < image.width - 8)
        clientPixel |= (p & 0xffffff) != 0;
      image.pixels[static_cast<size_t>(y) * image.width + x] =
          translucent ? p : (p | 0xff000000);
    }
  if (!somePixel || !clientPixel ||
      (translucent && (!someAlpha || invalidAlpha)))
    return {};
  return image;
}
int CaptureMain(const std::wstring &name, DWORD parent) {
  HANDLE mapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, name.c_str());
  if (!mapping)
    return 2;
  auto data = static_cast<Shared *>(
      MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Shared)));
  HANDLE request = OpenEventW(SYNCHRONIZE, FALSE, (name + L"-request").c_str());
  HANDLE done =
      OpenEventW(EVENT_MODIFY_STATE, FALSE, (name + L"-done").c_str());
  HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, parent);
  if (data && request && done && process) {
    HANDLE waits[]{process, request};
    while (WaitForMultipleObjects(2, waits, FALSE, INFINITE) ==
           WAIT_OBJECT_0 + 1) {
      LONG id = InterlockedCompareExchange(&data->request, 0, 0);
      InterlockedExchange(&data->ok, RenderWindow(*data) ? 1 : 0);
      InterlockedExchange(&data->complete, id);
      SetEvent(done);
    }
  }
  if (process)
    CloseHandle(process);
  if (done)
    CloseHandle(done);
  if (request)
    CloseHandle(request);
  if (data)
    UnmapViewOfFile(data);
  CloseHandle(mapping);
  return 0;
}
} // namespace wa
