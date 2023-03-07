#pragma once
#include <windows.h>
#include <unknwn.h>

#define EPOS_FONT_ROBOTO 102
#define EPOS_FONT_ROBOTO_BLACK 103
#define EPOS_FONT_ROBOTO_MONO 104

namespace epos {

#define EPOS_FONT_RESOURCE_GUID "9d7d3183-7cf2-47fb-82c9-98799214c673"

struct __declspec(uuid(EPOS_FONT_RESOURCE_GUID)) __declspec(novtable) IFontResource : IUnknown {
  virtual LPVOID __stdcall Data() noexcept = 0;
  virtual UINT32 __stdcall Size() noexcept = 0;
};

HRESULT CreateFontResource(HMODULE module, LPCSTR name, LPCSTR type, IFontResource** res);

}  // namespace epos