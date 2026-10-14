#pragma once
#include "animation.hpp"
#include <memory>
#include <windows.h>
namespace wa {
class CaptureClient {
public:
  CaptureClient();
  ~CaptureClient();
  bool Start();
  Image Request(HWND, const Rect &, bool translucent);

private:
  struct State;
  std::unique_ptr<State> state_;
  void Close();
};
int CaptureMain(const std::wstring &name, DWORD parent);
} // namespace wa
