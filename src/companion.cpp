#include "capture.hpp"
#include "runtime.hpp"
#include <algorithm>
#include <atomic>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

namespace {
constexpr UINT kRecord = WM_APP + 1, kEof = WM_APP + 2, kWireError = WM_APP + 3;
constexpr size_t kChunk = 1024 * 1024, kRecordLimit = 16 * 1024 * 1024;
wa::Runtime *runtime{};
HANDLE owner{};
bool initialized = false;
bool fatal = false;
std::atomic<size_t> queuedBytes{};
bool Exact(HANDLE h, void *buffer, DWORD length) {
  DWORD offset = 0;
  while (offset < length) {
    DWORD count = 0;
    if (!ReadFile(h, static_cast<char *>(buffer) + offset, length - offset,
                  &count, nullptr) ||
        !count)
      return false;
    offset += count;
  }
  return true;
}
void Send(const wa::Json &json) {
  auto bytes = json.dump();
  uint32_t length = static_cast<uint32_t>(bytes.size());
  DWORD written{};
  HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
  WriteFile(out, &length, 4, &written, nullptr);
  WriteFile(out, bytes.data(), length, &written, nullptr);
}
void Error(const std::string &message, const char *code) {
  Send({{"type", "error"}, {"v", 5}, {"code", code}, {"message", message}});
  fatal = true;
  PostQuitMessage(1);
}
DWORD WINAPI Reader(void *argument) {
  HWND hwnd = reinterpret_cast<HWND>(argument);
  HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
  for (;;) {
    std::string bytes;
    bool first = true;
    for (;;) {
      uint32_t header{};
      DWORD firstByte{};
      if (!ReadFile(input, &header, 1, &firstByte, nullptr) || !firstByte) {
        if (!first)
          PostMessageW(hwnd, kWireError, 0, 0);
        else
          PostMessageW(hwnd, kEof, 0, 0);
        return 0;
      }
      if (!Exact(input, reinterpret_cast<char *>(&header) + 1, 3)) {
        PostMessageW(hwnd, kWireError, 0, 0);
        return 0;
      }
      uint32_t length = header & 0x3fffffff, kind = header >> 30;
      if (!length || length > kChunk || (first && kind != 0 && kind != 1) ||
          (!first && kind != 2 && kind != 3) ||
          bytes.size() + length > kRecordLimit) {
        PostMessageW(hwnd, kWireError, 0, 0);
        return 0;
      }
      size_t old = bytes.size();
      bytes.resize(old + length);
      if (!Exact(input, bytes.data() + old, length)) {
        PostMessageW(hwnd, kWireError, 0, 0);
        return 0;
      }
      if (kind == 0 || kind == 3)
        break;
      first = false;
    }
    size_t count = bytes.size();
    if (queuedBytes.fetch_add(count) + count > kRecordLimit) {
      queuedBytes.fetch_sub(count);
      PostMessageW(hwnd, kWireError, 0, 0);
      return 0;
    }
    auto record = new std::string(std::move(bytes));
    if (!PostMessageW(hwnd, kRecord, 0, reinterpret_cast<LPARAM>(record))) {
      queuedBytes.fetch_sub(count);
      delete record;
      return 0;
    }
  }
}
void Handle(const std::string &text) {
  try {
    auto record = wa::Json::parse(text);
    if (!record.is_object() || record.value("v", 0) != 5) {
      Error("Version process-v2 invalide.", "invalid_wire");
      return;
    }
    auto type = record.value("type", "");
    if ((type == "init" || type == "settings") &&
        (!record.contains("deviceSettings") ||
         !record["deviceSettings"].is_object() ||
         !record.contains("layerSettings"))) {
      if (!initialized)
        Error("Snapshot de paramètres absent ou invalide.", "invalid_init");
      else
        std::cerr << "window-animations: missing settings snapshot; previous "
                     "settings retained\n";
      return;
    }
    if (!initialized) {
      if (type != "init") {
        Error("Le premier message doit être init.", "invalid_wire");
        return;
      }
      auto id = record.at("addonId").get<std::string>(),
           digest = record.at("artifactDigest").get<std::string>();
      bool hex = digest.size() == 71 && digest.starts_with("sha256:") &&
                 std::all_of(digest.begin() + 7, digest.end(), [](char c) {
                   return std::isxdigit(static_cast<unsigned char>(c)) != 0;
                 });
      bool identity = !id.empty() && id.size() <= 256 && id.front() != ' ' &&
                      id.back() != ' ' &&
                      std::none_of(id.begin(), id.end(), [](char c) {
                        return static_cast<unsigned char>(c) < 32;
                      });
      if (!identity || !hex ||
          record.at("version").get<std::string>().empty() ||
          record.at("targetArchitecture") != "windows-x86_64") {
        Error("Identité ou architecture invalide.", "invalid_init");
        return;
      }
      auto settings = wa::ParseSettings(record);
      if (!settings) {
        Error(settings.error, "addon_settings_rejected");
        return;
      }
      Send({{"type", "progress"}, {"v", 5}});
      // Ownership is checked after init/progress. No stdout is emitted before
      // the application has started the process-v2 handshake.
      owner = CreateMutexW(nullptr, FALSE,
                           L"Local\\MyWallpaperWindowAnimationsOwner");
      if (!owner || GetLastError() == ERROR_ALREADY_EXISTS) {
        Error("Animations des fenêtres est déjà actif dans cette session Windows.",
              "addon_owner_conflict");
        return;
      }
      if (!runtime->Start(
              settings.config,
              wa::Wide(record.at("scratchDirectory").get<std::string>()))) {
        Error("Observation Windows indisponible.", "addon_start_failed");
        return;
      }
      initialized = true;
      Send({{"type", "ready"}, {"v", 5}});
      return;
    }
    if (type == "settings") {
      auto parsed = wa::ParseSettings(record);
      if (!parsed) {
        std::cerr << "window-animations: settings rejected, previous snapshot "
                     "retained: "
                  << parsed.error << '\n';
        return;
      }
      runtime->Update(parsed.config);
    } else if (type == "shutdown") {
      runtime->Stop();
      PostQuitMessage(0);
    } else if (type == "message") {
      // Native-only settings have no button actions. Scene messages are
      // optional.
      if (record.contains("payload") && record["payload"].is_object() &&
          record["payload"].value("type", "") == "cancel")
        runtime->CancelAll();
    } else
      Error("Message process-v2 inattendu.", "invalid_wire");
  } catch (const std::exception &e) {
    Error(e.what(), "invalid_wire");
  }
}
LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM w, LPARAM l) {
  if (message == kRecord) {
    std::unique_ptr<std::string> record(reinterpret_cast<std::string *>(l));
    queuedBytes.fetch_sub(record->size());
    Handle(*record);
    return 0;
  }
  if (message == kEof) {
    if (runtime)
      runtime->Stop();
    PostQuitMessage(0);
    return 0;
  }
  if (message == kWireError) {
    Error("Enregistrement process-v2 tronqué, désordonné ou trop volumineux.",
          "invalid_wire");
    return 0;
  }
  if (message == WM_APP + 10) {
    try {
      if (runtime)
        runtime->OnEvent(static_cast<DWORD>(w), reinterpret_cast<HWND>(l));
    } catch (const std::exception &e) {
      Error(e.what(), "addon_runtime_failed");
    }
    return 0;
  }
  if (message == WM_TIMER) {
    try {
      runtime->Tick();
    } catch (const std::exception &e) {
      Error(e.what(), "addon_runtime_failed");
    }
    return 0;
  }
  if (message == WM_SETTINGCHANGE || message == WM_DISPLAYCHANGE) {
    if (runtime)
      runtime->SystemChanged();
    return 0;
  }
  return DefWindowProcW(hwnd, message, w, l);
}
} // namespace
int wmain(int argc, wchar_t **argv) {
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  if (argc == 4 && wcscmp(argv[1], L"--guard") == 0)
    return wa::GuardMain(argv[2],
                         static_cast<DWORD>(wcstoul(argv[3], nullptr, 10)));
  if (argc == 4 && wcscmp(argv[1], L"--capture") == 0)
    return wa::CaptureMain(argv[2],
                           static_cast<DWORD>(wcstoul(argv[3], nullptr, 10)));
  WNDCLASSW wc{};
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.lpszClassName = L"MwWindowAnimationCompanion";
  if (!RegisterClassW(&wc))
    return 2;
  // A hidden top-level window receives broadcasts (a message-only window would
  // not).
  HWND hwnd = CreateWindowW(wc.lpszClassName, L"", WS_POPUP, 0, 0, 0, 0,
                            nullptr, nullptr, wc.hInstance, nullptr);
  if (!hwnd)
    return 2;
  wa::Runtime worker;
  worker.window = hwnd;
  runtime = &worker;
  HANDLE reader = CreateThread(nullptr, 0, Reader, hwnd, 0, nullptr);
  if (!reader) {
    DestroyWindow(hwnd);
    return 2;
  }
  MSG message{};
  while (GetMessageW(&message, nullptr, 0, 0) > 0) {
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }
  worker.Stop();
  runtime = nullptr;
  CancelSynchronousIo(reader);
  WaitForSingleObject(reader, 500);
  CloseHandle(reader);
  while (PeekMessageW(&message, hwnd, kRecord, kRecord, PM_REMOVE)) {
    auto ptr = reinterpret_cast<std::string *>(message.lParam);
    queuedBytes.fetch_sub(ptr->size());
    delete ptr;
  }
  DestroyWindow(hwnd);
  if (owner)
    CloseHandle(owner);
  return fatal ? 1 : 0;
}
