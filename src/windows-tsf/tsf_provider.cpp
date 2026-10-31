#include "input_engine/tamil_transliterator.h"

#include <windows.h>
#include <msctf.h>

#include <atomic>
#include <climits>
#include <new>
#include <utility>
#include <string>
#include <string_view>

static const CLSID CLSID_TamilTextService = {
    0x3a36e4c8, 0x31a8, 0x49a4, {0x9d, 0x67, 0x3e, 0x7b, 0x12, 0x8d, 0xe2, 0x41}
};

namespace {
std::atomic<long> g_object_count{0};
std::atomic<long> g_server_locks{0};

bool utf8_to_wide(std::string_view input, std::wstring* output) {
    if (!output) return false;
    output->clear();
    if (input.empty()) return true;
    if (input.size() > static_cast<std::size_t>(INT_MAX)) return false;

    const int input_size = static_cast<int>(input.size());
    const int required = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, input.data(), input_size, nullptr, 0);
    if (required <= 0) return false;

    output->resize(static_cast<std::size_t>(required));
    const int converted = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, input.data(), input_size,
        output->data(), required);
    return converted == required;
}

class CommitEditSession final : public ITfEditSession {
public:
    CommitEditSession(ITfContext* context, std::wstring text) noexcept
        : context_(context), text_(std::move(text)) {
        if (context_) context_->AddRef();
        ++g_object_count;
    }

    ~CommitEditSession() {
        if (context_) context_->Release();
        --g_object_count;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override {
        if (!object) return E_POINTER;
        *object = nullptr;
        if (iid == IID_IUnknown || iid == IID_ITfEditSession) {
            *object = static_cast<ITfEditSession*>(this);
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

    HRESULT STDMETHODCALLTYPE DoEditSession(TfEditCookie cookie) override {
        if (!context_) return E_UNEXPECTED;

        TF_SELECTION selection{};
        ULONG fetched = 0;
        HRESULT hr = context_->GetSelection(
            cookie, TF_DEFAULT_SELECTION, 1, &selection, &fetched);
        if (FAILED(hr)) return hr;
        if (fetched == 0 || !selection.range) return TF_E_NOSELECTION;

        hr = selection.range->SetText(
            cookie, 0, text_.data(), static_cast<LONG>(text_.size()));
        if (SUCCEEDED(hr)) {
            hr = selection.range->Collapse(cookie, TF_ANCHOR_END);
            if (SUCCEEDED(hr)) {
                selection.style.ase = TF_AE_NONE;
                selection.style.fInterimChar = FALSE;
                hr = context_->SetSelection(cookie, 1, &selection);
            }
        }

        selection.range->Release();
        return hr;
    }

private:
    std::atomic<ULONG> references_{1};
    ITfContext* context_{nullptr};
    std::wstring text_;
};

class TamilTextService final : public ITfTextInputProcessor,
                               public ITfKeyEventSink {
public:
    TamilTextService() noexcept {
        ++g_object_count;
    }

    ~TamilTextService() {
        unadvise_key_sink();
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
        } else if (iid == IID_ITfKeyEventSink) {
            *object = static_cast<ITfKeyEventSink*>(this);
        } else {
            return E_NOINTERFACE;
        }

        AddRef();
        return S_OK;
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

        HRESULT hr = thread_manager_->QueryInterface(
            IID_ITfKeystrokeMgr, reinterpret_cast<void**>(&keystroke_manager_));
        if (FAILED(hr)) {
            thread_manager_->Release();
            thread_manager_ = nullptr;
            client_id_ = TF_CLIENTID_NULL;
            return hr;
        }

        hr = keystroke_manager_->AdviseKeyEventSink(
            client_id_, static_cast<ITfKeyEventSink*>(this), TRUE);
        if (FAILED(hr)) {
            unadvise_key_sink();
            thread_manager_->Release();
            thread_manager_ = nullptr;
            client_id_ = TF_CLIENTID_NULL;
            return hr;
        }
        key_sink_advised_ = true;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Deactivate() override {
        pending_.clear();
        unadvise_key_sink();
        if (thread_manager_) {
            thread_manager_->Release();
            thread_manager_ = nullptr;
        }
        client_id_ = TF_CLIENTID_NULL;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnSetFocus(BOOL foreground) override {
        if (!foreground) pending_.clear();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnTestKeyDown(ITfContext*, WPARAM key,
                                             LPARAM, BOOL* eaten) override {
        if (!eaten) return E_INVALIDARG;
        const UINT vk = static_cast<UINT>(key);
        *eaten = (is_roman_letter(vk) ||
                  (!pending_.empty() && is_pending_control(vk))) ? TRUE : FALSE;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnKeyDown(ITfContext* context, WPARAM key,
                                         LPARAM, BOOL* eaten) override {
        if (!eaten) return E_INVALIDARG;
        *eaten = FALSE;
        const UINT vk = static_cast<UINT>(key);

        if (is_roman_letter(vk)) {
            pending_.push_back(static_cast<char>(vk));
            *eaten = TRUE;
            return S_OK;
        }

        if (pending_.empty()) return S_OK;

        if (vk == VK_BACK) {
            pending_.pop_back();
            *eaten = TRUE;
            return S_OK;
        }

        if (vk == VK_ESCAPE) {
            pending_.clear();
            *eaten = TRUE;
            return S_OK;
        }

        std::wstring terminator;
        if (!get_commit_terminator(vk, &terminator)) return S_OK;
        if (!commit_pending(context, terminator)) return S_OK;

        pending_.clear();
        *eaten = TRUE;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnTestKeyUp(ITfContext*, WPARAM, LPARAM,
                                           BOOL* eaten) override {
        if (!eaten) return E_INVALIDARG;
        *eaten = FALSE;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnKeyUp(ITfContext*, WPARAM, LPARAM,
                                       BOOL* eaten) override {
        if (!eaten) return E_INVALIDARG;
        *eaten = FALSE;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnPreservedKey(ITfContext*, REFGUID,
                                               BOOL* eaten) override {
        if (!eaten) return E_INVALIDARG;
        *eaten = FALSE;
        return S_OK;
    }

private:
    static bool is_roman_letter(UINT vk) {
        return vk >= static_cast<UINT>('A') && vk <= static_cast<UINT>('Z');
    }

    bool is_pending_control(UINT vk) const {
        return vk == VK_BACK || vk == VK_ESCAPE || vk == VK_SPACE ||
               vk == VK_RETURN || vk == VK_OEM_PERIOD || vk == VK_OEM_COMMA ||
               vk == VK_OEM_1 || vk == VK_OEM_2;
    }

    static bool get_commit_terminator(UINT vk, std::wstring* text) {
        if (!text) return false;
        switch (vk) {
        case VK_SPACE:       *text = L" "; return true;
        case VK_RETURN:      *text = L"\r\n"; return true;
        case VK_OEM_PERIOD:  *text = L"."; return true;
        case VK_OEM_COMMA:   *text = L","; return true;
        case VK_OEM_1:       *text = L";"; return true;
        case VK_OEM_2:       *text = L"/"; return true;
        default: return false;
        }
    }

    bool commit_pending(ITfContext* context, const std::wstring& terminator) {
        if (!context || pending_.empty()) return false;

        input_engine::TamilTransliterator transliterator;
        std::string utf8 = transliterator.transliterate(pending_);
        std::wstring text;
        if (!utf8_to_wide(utf8, &text)) return false;
        text += terminator;

        auto* session = new (std::nothrow) CommitEditSession(context, std::move(text));
        if (!session) return false;

        HRESULT session_result = E_FAIL;
        const HRESULT request_result = context->RequestEditSession(
            client_id_, session, TF_ES_SYNC | TF_ES_READWRITE, &session_result);
        session->Release();

        return SUCCEEDED(request_result) && SUCCEEDED(session_result);
    }

    void unadvise_key_sink() {
        if (key_sink_advised_ && keystroke_manager_) {
            keystroke_manager_->UnadviseKeyEventSink(client_id_);
        }
        key_sink_advised_ = false;
        if (keystroke_manager_) {
            keystroke_manager_->Release();
            keystroke_manager_ = nullptr;
        }
    }

    std::atomic<ULONG> references_{1};
    ITfThreadMgr* thread_manager_{nullptr};
    ITfKeystrokeMgr* keystroke_manager_{nullptr};
    TfClientId client_id_{TF_CLIENTID_NULL};
    bool key_sink_advised_{false};
    std::string pending_;
};

class TamilClassFactory final : public IClassFactory {
public:
    TamilClassFactory() noexcept { ++g_object_count; }
    ~TamilClassFactory() { --g_object_count; }

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
        if (lock) ++g_server_locks;
        else if (g_server_locks.load() > 0) --g_server_locks;
        return S_OK;
    }

private:
    std::atomic<ULONG> references_{1};
};
} // namespace

extern "C" __declspec(dllexport) HRESULT __stdcall
DllGetClassObject(REFCLSID clsid, REFIID iid, void** object) {
    if (!object) return E_POINTER;
    *object = nullptr;
    if (clsid != CLSID_TamilTextService) return CLASS_E_CLASSNOTAVAILABLE;

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
