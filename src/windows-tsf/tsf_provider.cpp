#include <windows.h>
#include <msctf.h>

#include <atomic>
#include <new>

// Development CLSID. Keep this stable once the language profile is registered.
static const CLSID CLSID_TamilTextService = {
    0x3a36e4c8, 0x31a8, 0x49a4, {0x9d, 0x67, 0x3e, 0x7b, 0x12, 0x8d, 0xe2, 0x41}
};

namespace {
std::atomic<long> g_object_count{0};
std::atomic<long> g_server_locks{0};

class TamilTextService final : public ITfTextInputProcessor {
public:
    TamilTextService() noexcept { ++g_object_count; }
    ~TamilTextService() override {
        if (thread_manager_) {
            thread_manager_->Release();
            thread_manager_ = nullptr;
        }
        --g_object_count;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override {
        if (!object) return E_POINTER;
        *object = nullptr;

        if (iid == IID_IUnknown || iid == IID_ITfTextInputProcessor) {
            *object = static_cast<ITfTextInputProcessor*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return static_cast<ULONG>(++references_);
    }

    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG remaining = static_cast<ULONG>(--references_);
        if (remaining == 0) delete this;
        return remaining;
    }

    HRESULT STDMETHODCALLTYPE Activate(ITfThreadMgr* thread_manager,
                                        TfClientId client_id) override {
        if (!thread_manager) return E_INVALIDARG;
        if (thread_manager_) return TF_E_ALREADY_EXISTS;

        thread_manager->AddRef();
        thread_manager_ = thread_manager;
        client_id_ = client_id;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Deactivate() override {
        if (thread_manager_) {
            thread_manager_->Release();
            thread_manager_ = nullptr;
        }
        client_id_ = TF_CLIENTID_NULL;
        return S_OK;
    }

private:
    std::atomic<ULONG> references_{1};
    ITfThreadMgr* thread_manager_{nullptr};
    TfClientId client_id_{TF_CLIENTID_NULL};
};

class TamilClassFactory final : public IClassFactory {
public:
    TamilClassFactory() noexcept { ++g_object_count; }
    ~TamilClassFactory() override { --g_object_count; }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override {
        if (!object) return E_POINTER;
        *object = nullptr;

        if (iid == IID_IUnknown || iid == IID_IClassFactory) {
            *object = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return static_cast<ULONG>(++references_);
    }

    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG remaining = static_cast<ULONG>(--references_);
        if (remaining == 0) delete this;
        return remaining;
    }

    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* outer, REFIID iid,
                                              void** object) override {
        if (!object) return E_POINTER;
        *object = nullptr;
        if (outer) return CLASS_E_NOAGGREGATION;

        auto* service = new (std::nothrow) TamilTextService();
        if (!service) return E_OUTOFMEMORY;

        const HRESULT result = service->QueryInterface(iid, object);
        service->Release();
        return result;
    }

    HRESULT STDMETHODCALLTYPE LockServer(BOOL lock) override {
        if (lock) {
            ++g_server_locks;
        } else if (g_server_locks.load() > 0) {
            --g_server_locks;
        }
        return S_OK;
    }

private:
    std::atomic<ULONG> references_{1};
};
} // namespace

extern "C" __declspec(dllexport) HRESULT __stdcall
DllGetClassObject(REFCLSID clsid, REFIID iid, void** object) {
    if (clsid != CLSID_TamilTextService) return CLASS_E_CLASSNOTAVAILABLE;
    if (!object) return E_POINTER;
    *object = nullptr;

    auto* factory = new (std::nothrow) TamilClassFactory();
    if (!factory) return E_OUTOFMEMORY;

    const HRESULT result = factory->QueryInterface(iid, object);
    factory->Release();
    return result;
}

extern "C" __declspec(dllexport) HRESULT __stdcall DllCanUnloadNow() {
    return (g_object_count.load() == 0 && g_server_locks.load() == 0)
        ? S_OK : S_FALSE;
}

BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID) {
    return TRUE;
}
