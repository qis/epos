#include "fonts.hpp"

namespace epos {

class FontResource : public IFontResource {
public:
  HRESULT __stdcall Load(HMODULE module, LPCSTR name, LPCSTR type) noexcept
  {
    if (const auto resource = FindResource(module, name, type)) {
      if (const auto handle = LoadResource(module, resource)) {
        data_ = LockResource(handle);
        size_ = static_cast<UINT32>(SizeofResource(module, resource));
        return S_OK;
      }
    }
    return E_FAIL;
  }

  ULONG __stdcall AddRef() noexcept override
  {
    return InterlockedIncrement(&references_);
  }

  ULONG __stdcall Release() noexcept override
  {
    const auto count = InterlockedDecrement(&references_);
    if (count == 0) {
      delete this;
    }
    return count;
  }

  HRESULT __stdcall QueryInterface(const IID& id, void** object) noexcept override
  {
    if (id == __uuidof(IFontResource) || id == __uuidof(IUnknown)) {
      *object = static_cast<IFontResource*>(this);
    } else {
      *object = nullptr;
      return E_NOINTERFACE;
    }
    static_cast<IUnknown*>(*object)->AddRef();
    return S_OK;
  }

  virtual LPVOID __stdcall Data() noexcept override
  {
    return data_;
  }

  virtual UINT32 __stdcall Size() noexcept override
  {
    return size_;
  }

private:
  LONG references_ = 1;
  LPVOID data_ = nullptr;
  UINT32 size_ = 0;
};

HRESULT CreateFontResource(HMODULE module, LPCSTR name, LPCSTR type, IFontResource** res)
{
  if (!res) {
    return E_INVALIDARG;
  }
  const auto resource = new FontResource();
  const auto hr = resource->Load(module, name, type);
  if (SUCCEEDED(hr)) {
    *res = resource;
  } else {
    delete resource;
  }
  return hr;
}

}  // namespace epos