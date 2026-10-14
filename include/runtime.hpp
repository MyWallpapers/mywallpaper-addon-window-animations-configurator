#pragma once
#include "animation.hpp"
#include <memory>
#include <string>
#include <windows.h>
namespace wa {
class Watchdog {
public:
  Watchdog();
  ~Watchdog();
  bool Start();
  int Hide(HWND window);
  void Restore(int slot);
  void Beat();
  bool Tripped() const;

private:
  struct State;
  std::unique_ptr<State> state_;
};
int GuardMain(const std::wstring &name, DWORD parent);
struct Runtime {
  Runtime();
  ~Runtime();
  bool Start(const Config &, const std::wstring &scratch);
  void Update(const Config &);
  void Stop();
  void Tick();
  void OnEvent(DWORD, HWND, bool captureReady = false);
  void CancelAll();
  void SystemChanged();
  Json Status() const;
  HWND window{};

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
std::wstring Wide(const std::string &);
std::wstring ExeDirectory();
bool WriteBitmap(const std::wstring &, const Image &);
} // namespace wa
