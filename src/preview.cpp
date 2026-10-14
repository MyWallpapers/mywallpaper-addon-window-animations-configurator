#include <dwmapi.h>
#include <string>
#include <windows.h>
namespace {
HWND canvas{}, controller{};
int serial{};
HFONT font{}, captionFont{};
void RefreshButtons() {
  if (!controller)
    return;
  EnableWindow(GetDlgItem(controller, 100), canvas == nullptr);
  EnableWindow(GetDlgItem(controller, 101), canvas && !IsIconic(canvas));
  EnableWindow(GetDlgItem(controller, 102), canvas && IsIconic(canvas));
  EnableWindow(GetDlgItem(controller, 103), canvas != nullptr);
}
void Draw(HWND hwnd, HDC dc) {
  RECT r{};
  GetClientRect(hwnd, &r);
  HBRUSH background = CreateSolidBrush(RGB(24, 36, 54));
  FillRect(dc, &r, background);
  DeleteObject(background);
  SetBkMode(dc, TRANSPARENT);
  SetTextColor(dc, RGB(232, 241, 250));
  SelectObject(dc, font);
  RECT title{40, 32, r.right - 40, 100};
  DrawTextW(dc, L"Une fenêtre qui prend vie", -1, &title,
            DT_LEFT | DT_SINGLELINE);
  SetTextColor(dc, RGB(165, 194, 218));
  SelectObject(dc, captionFont);
  RECT caption{40, 95, r.right - 40, 170};
  DrawTextW(dc,
            L"Fenêtre d'essai Win32. Ouvrez, réduisez, restaurez et "
            L"fermez.\nLes effets suivent les réglages MyWallpaper.",
            -1, &caption, DT_LEFT | DT_WORDBREAK);
  for (int i = 0; i < 7; ++i) {
    HBRUSH bar = CreateSolidBrush(RGB(70 + i * 14, 167 + i * 5, 216 - i * 9));
    RECT strip{40 + i * 74, 235 + (6 - i) * 10, 96 + i * 74, r.bottom - 40};
    FillRect(dc, &strip, bar);
    DeleteObject(bar);
  }
}
LRESULT CALLBACK CanvasProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
  if (msg == WM_PAINT) {
    PAINTSTRUCT p{};
    HDC dc = BeginPaint(hwnd, &p);
    Draw(hwnd, dc);
    EndPaint(hwnd, &p);
    return 0;
  }
  if (msg == WM_PRINT || msg == WM_PRINTCLIENT) {
    if (msg == WM_PRINT && (l & PRF_NONCLIENT))
      DefWindowProcW(hwnd, msg, w,
                     l & ~(PRF_CLIENT | PRF_CHILDREN | PRF_ERASEBKGND));
    POINT old{};
    if (msg == WM_PRINT) {
      RECT window{};
      GetWindowRect(hwnd, &window);
      POINT client{};
      ClientToScreen(hwnd, &client);
      SetViewportOrgEx(reinterpret_cast<HDC>(w), client.x - window.left,
                       client.y - window.top, &old);
    }
    Draw(hwnd, reinterpret_cast<HDC>(w));
    // The capture helper reads the shared DIB as soon as WM_PRINT returns.
    // Flush this thread's GDI batch before handing the pixels back.
    GdiFlush();
    if (msg == WM_PRINT)
      SetViewportOrgEx(reinterpret_cast<HDC>(w), old.x, old.y, nullptr);
    return 0;
  }
  if (msg == WM_DESTROY) {
    canvas = nullptr;
    RefreshButtons();
    return 0;
  }
  if (msg == WM_SIZE)
    RefreshButtons();
  return DefWindowProcW(hwnd, msg, w, l);
}
void Open() {
  if (canvas) {
    ShowWindow(canvas, SW_RESTORE);
    return;
  }
  auto title = L"Animations · fenêtre d'essai " + std::to_wstring(++serial);
  canvas = CreateWindowW(L"MwAnimationPreviewCanvas", title.c_str(),
                         WS_OVERLAPPEDWINDOW, 260, 190, 650, 430, nullptr,
                         nullptr, GetModuleHandleW(nullptr), nullptr);
  ShowWindow(canvas, SW_SHOWNOACTIVATE);
  UpdateWindow(canvas);
  RefreshButtons();
}
LRESULT CALLBACK ControlProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
  if (msg == WM_CREATE) {
    controller = hwnd;
    const wchar_t *labels[] = {L"Ouvrir", L"Réduire", L"Restaurer", L"Fermer"};
    for (int i = 0; i < 4; ++i) {
      HWND b = CreateWindowW(
          L"BUTTON", labels[i],
          WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 20 + i * 120, 70,
          110, 36, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(100 + i)),
          GetModuleHandleW(nullptr), nullptr);
      SendMessageW(b, WM_SETFONT,
                   reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),
                   TRUE);
    }
    RefreshButtons();
    return 0;
  }
  if (msg == WM_COMMAND || msg == WM_APP + 20) {
    int command = msg == WM_COMMAND ? LOWORD(w) - 100 : static_cast<int>(w);
    if (command == 0)
      Open();
    if (command == 1 && canvas)
      ShowWindow(canvas, SW_MINIMIZE);
    if (command == 2 && canvas)
      ShowWindow(canvas, SW_RESTORE);
    if (command == 3 && canvas)
      DestroyWindow(canvas);
    if (command == 4)
      Sleep(1800); // Owned fixture only: exercise capture timeout isolation.
    return 0;
  }
  if (msg == WM_PAINT) {
    PAINTSTRUCT p{};
    HDC dc = BeginPaint(hwnd, &p);
    RECT r{20, 18, 505, 60};
    SetBkMode(dc, TRANSPARENT);
    DrawTextW(dc, L"Essayez les quatre transitions avec vos réglages.", -1, &r,
              DT_LEFT | DT_WORDBREAK);
    EndPaint(hwnd, &p);
    return 0;
  }
  if (msg == WM_DESTROY) {
    if (canvas)
      DestroyWindow(canvas);
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcW(hwnd, msg, w, l);
}
} // namespace
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  font = CreateFontW(-22, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                     CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
  captionFont =
      CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                  DEFAULT_PITCH, L"Segoe UI");
  WNDCLASSW wc{};
  wc.hInstance = instance;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  wc.lpfnWndProc = CanvasProc;
  wc.lpszClassName = L"MwAnimationPreviewCanvas";
  RegisterClassW(&wc);
  wc.lpfnWndProc = ControlProc;
  wc.lpszClassName = L"MwAnimationPreviewControl";
  RegisterClassW(&wc);
  controller =
      CreateWindowW(wc.lpszClassName, L"Essayer les animations MyWallpaper",
                    WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, 200,
                    35, 535, 155, nullptr, nullptr, instance, nullptr);
  // Consume STARTUPINFO's show state before the first test canvas opens.
  ShowWindow(controller, SW_SHOWNOACTIVATE);
  MSG msg{};
  while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
    if (!IsDialogMessageW(controller, &msg)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
  }
  DeleteObject(font);
  DeleteObject(captionFont);
  return 0;
}
