#include <epos/window.hpp>
#include <windows.h>
#include <exception>
#include <format>
#include <cstdlib>

int WINAPI WinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE, _In_ LPSTR cmd, _In_ int show)
{
  try {
    epos::window window(instance);
    window.create();
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
      do {
        if (msg.message == WM_QUIT) {
          return static_cast<int>(msg.wParam);
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      } while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE) > 0);
    }
  }
  catch (const std::system_error& e) {
    std::string text = "Unexpected error during startup.\r\n\r\n";
    std::format_to(std::back_inserter(text), "Source: {}\r\n", e.code().category().name());
    std::format_to(std::back_inserter(text), "{}", e.what());
    MessageBox(nullptr, text.data(), "EPOS Error", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
    return EXIT_FAILURE;
  }
  catch (const std::exception& e) {
    std::string text = "Unexpected error during startup.\r\n\r\n";
    text.append(e.what());
    MessageBox(nullptr, text.data(), "EPOS Error", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
    return EXIT_FAILURE;
  }
  catch (...) {
    std::string text = "Unexpected error during startup.\r\n\r\n";
    MessageBox(nullptr, text.data(), "EPOS Error", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}