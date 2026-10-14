#include "runtime.hpp"
#include <array>
#include <dwmapi.h>
namespace wa {
namespace {
struct Lease {
  volatile LONG64 hwnd{};
  DWORD pid{}, tid{};
  uint64_t creation{};
};
struct Shared {
  volatile LONG64 beat{};
  volatile LONG stop{}, tripped{};
  std::array<Lease, 4> leases;
};
uint64_t Creation(HANDLE process) {
  FILETIME a{}, b{}, c{}, d{};
  if (!GetProcessTimes(process, &a, &b, &c, &d))
    return 0;
  return (static_cast<uint64_t>(a.dwHighDateTime) << 32) | a.dwLowDateTime;
}
void RestoreLease(Lease &lease) {
  auto hwnd = reinterpret_cast<HWND>(InterlockedExchange64(&lease.hwnd, 0));
  if (!hwnd || hwnd == reinterpret_cast<HWND>(-1))
    return;
  DWORD pid = 0;
  DWORD tid = GetWindowThreadProcessId(hwnd, &pid);
  if (pid != lease.pid || tid != lease.tid)
    return;
  HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
  if (!p)
    return;
  bool same = Creation(p) == lease.creation;
  CloseHandle(p);
  if (same) {
    COLORREF key{};
    BYTE alpha{};
    DWORD flags{};
    bool readable =
        GetLayeredWindowAttributes(hwnd, &key, &alpha, &flags) != FALSE;
    // An application may change its own alpha while we animate. Preserve that
    // change rather than overwriting a value that is no longer ours.
    if (readable && ((flags & LWA_ALPHA) == 0 || alpha != 0 || key != 0))
      return;
    if (readable)
      SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
    auto ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, ex & ~WS_EX_LAYERED);
  }
}
} // namespace
struct Watchdog::State {
  HANDLE mapping{}, process{};
  Shared *data{};
};
Watchdog::Watchdog() : state_(std::make_unique<State>()) {}
Watchdog::~Watchdog() {
  if (state_->data) {
    for (auto &l : state_->data->leases)
      RestoreLease(l);
    InterlockedExchange(&state_->data->stop, 1);
  }
  if (state_->process) {
    WaitForSingleObject(state_->process, 500);
    CloseHandle(state_->process);
  }
  if (state_->data)
    UnmapViewOfFile(state_->data);
  if (state_->mapping)
    CloseHandle(state_->mapping);
}
bool Watchdog::Start() {
  auto name = L"Local\\MwWindowAnimations-" +
              std::to_wstring(GetCurrentProcessId()) + L"-" +
              std::to_wstring(Creation(GetCurrentProcess()));
  state_->mapping =
      CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                         sizeof(Shared), name.c_str());
  if (!state_->mapping || GetLastError() == ERROR_ALREADY_EXISTS)
    return false;
  state_->data = static_cast<Shared *>(MapViewOfFile(
      state_->mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Shared)));
  if (!state_->data)
    return false;
  new (state_->data) Shared{};
  Beat();
  auto exe = ExeDirectory() + L"\\companion.exe";
  auto command = L"\"" + exe + L"\" --guard \"" + name + L"\" " +
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
  return true;
}
int Watchdog::Hide(HWND window) {
  if (!state_->data || !state_->process || Tripped() ||
      WaitForSingleObject(state_->process, 0) != WAIT_TIMEOUT)
    return -1;
  DWORD current = 0;
  if (FAILED(DwmGetWindowAttribute(window, DWMWA_CLOAKED, &current,
                                   sizeof(current))) ||
      current)
    return -1;
  // SetLayeredWindowAttributes changes the UpdateLayeredWindow contract.
  // Existing layered windows retain their own compositor and are never hidden.
  if (GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_LAYERED)
    return -1;
  for (size_t i = 0; i < state_->data->leases.size(); ++i) {
    auto &l = state_->data->leases[i];
    if (InterlockedCompareExchange64(&l.hwnd, -1, 0) != 0)
      continue;
    l.tid = GetWindowThreadProcessId(window, &l.pid);
    HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, l.pid);
    if (!p) {
      InterlockedExchange64(&l.hwnd, 0);
      return -1;
    }
    l.creation = Creation(p);
    CloseHandle(p);
    if (!l.creation) {
      InterlockedExchange64(&l.hwnd, 0);
      return -1;
    }
    LONG_PTR originalEx = GetWindowLongPtrW(window, GWL_EXSTYLE);
    if (originalEx & WS_EX_LAYERED) {
      InterlockedExchange64(&l.hwnd, 0);
      return -1;
    }
    InterlockedExchange64(&l.hwnd, reinterpret_cast<LONG64>(window));
    SetLastError(0);
    LONG_PTR previousEx = SetWindowLongPtrW(window, GWL_EXSTYLE,
                                          originalEx | WS_EX_LAYERED);
    // A nonzero previous style is a successful return. LastError alone is
    // undefined on success and must not retire a lease after changing style.
    if (previousEx == 0 && GetLastError() != 0) {
      InterlockedExchange64(&l.hwnd, 0);
      return -1;
    }
    if (!SetLayeredWindowAttributes(window, 0, 0, LWA_ALPHA)) {
      RestoreLease(l);
      return -1;
    }
    // A watchdog may have restored this lease while a Win32 setter was slow.
    // Never leave a late hide behind after ownership has already been retired.
    if (Tripped() || InterlockedCompareExchange64(&l.hwnd, 0, 0) !=
                         reinterpret_cast<LONG64>(window)) {
      InterlockedCompareExchange64(&l.hwnd, reinterpret_cast<LONG64>(window),
                                   0);
      RestoreLease(l);
      return -1;
    }
    return static_cast<int>(i);
  }
  return -1;
}
void Watchdog::Restore(int slot) {
  if (state_->data && slot >= 0 && slot < 4)
    RestoreLease(state_->data->leases[slot]);
}
void Watchdog::Beat() {
  if (state_->data)
    InterlockedExchange64(&state_->data->beat,
                          static_cast<LONG64>(GetTickCount64()));
}
bool Watchdog::Tripped() const {
  return !state_->data ||
         InterlockedCompareExchange(&state_->data->tripped, 0, 0) != 0;
}
int GuardMain(const std::wstring &name, DWORD parent) {
  HANDLE mapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, name.c_str());
  if (!mapping)
    return 2;
  auto data = static_cast<Shared *>(
      MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Shared)));
  if (!data) {
    CloseHandle(mapping);
    return 2;
  }
  HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, parent);
  while (process && WaitForSingleObject(process, 100) == WAIT_TIMEOUT &&
         InterlockedCompareExchange(&data->stop, 0, 0) == 0) {
    auto last =
        static_cast<ULONGLONG>(InterlockedCompareExchange64(&data->beat, 0, 0));
    if (GetTickCount64() - last > 1500) {
      InterlockedExchange(&data->tripped, 1);
      for (auto &l : data->leases)
        RestoreLease(l);
    }
  }
  for (auto &l : data->leases)
    RestoreLease(l);
  if (process)
    CloseHandle(process);
  UnmapViewOfFile(data);
  CloseHandle(mapping);
  return 0;
}
} // namespace wa
