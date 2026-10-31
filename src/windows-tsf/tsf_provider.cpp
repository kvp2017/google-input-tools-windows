#include "input_engine/tamil_transliterator.h"

#include <windows.h>
#include <msctf.h>

#include <algorithm>
#include <atomic>
#include <climits>
#include <new>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

static const CLSID CLSID_TamilTextService = {
    0x3a36e4c8, 0x31a8, 0x49a4, {0x9d, 0x67, 0x3e, 0x7b, 0x12, 0x8d, 0xe2, 0x41}
};

namespace {
std::atomic<long> g_object_count{0};
std::atomic<long> g_server_locks{0};

enum class EditAction { Update, Commit, Cancel };

bool utf8_to_wide(std::string_view input, std::wstring* output) {
    if (!output) return false;
    output->clear();
    if (input.empty()) return true;
    if (input.size() > static_cast<std::size_t>(INT_MAX)) return false;

    const int count = static_cast<int>(input.size());
    const int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                              input.data(), count, nullptr, 0);
    if (required <= 0) return false;
    output->resize(static_cast<std::size_t>(required));
    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input.data(), count,
                               output->data(), required) == required;
}

DWORD read_user_setting(const wchar_t* name, DWORD fallback) {
    DWORD value = fallback;
    DWORD size = sizeof(value);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\KVP\\TamilIme", name,
                     RRF_RT_REG_DWORD, nullptr, &value, &size) != ERROR_SUCCESS) {
        return fallback;
    }
    return value;
}

class TamilTextService;

class CompositionEditSession final : public ITfEditSession {
public:
    CompositionEditSession(TamilTextService* service, ITfContext* context,
                           EditAction action, std::wstring text) noexcept;
    ~CompositionEditSession();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;
    HRESULT STDMETHODCALLTYPE DoEditSession(TfEditCookie cookie) override;

private:
    std::atomic<ULONG> references_{1};
    TamilTextService* service_{nullptr};
    ITfContext* context_{nullptr};
    EditAction action_;
    std::wstring text_;
};

class TamilTextService final : public ITfTextInputProcessor,
                               public ITfKeyEventSink {
public:
    TamilTextService() noexcept { ++g_object_count; }

    ~TamilTextService() {
        hide_candidate_window();
        if (candidate_window_) DestroyWindow(candidate_window_);
        unadvise_key_sink();
        release_composition();
        if (thread_manager_) thread_manager_->Release();
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
        const ULONG left = static_cast<ULONG>(--references_);
        if (left == 0) delete this;
        return left;
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
        hide_candidate_window();
        unadvise_key_sink();
        release_composition();
        if (thread_manager_) {
            thread_manager_->Release();
            thread_manager_ = nullptr;
        }
        client_id_ = TF_CLIENTID_NULL;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnSetFocus(BOOL foreground) override {
        if (!foreground) hide_candidate_window();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnTestKeyDown(ITfContext*, WPARAM key,
                                             LPARAM, BOOL* eaten) override {
        if (!eaten) return E_INVALIDARG;
        *eaten = FALSE;
        if (read_user_setting(L"Enabled", 1) == 0) return S_OK;

        const UINT vk = static_cast<UINT>(key);
        if (is_roman_letter(vk) || (!pending_.empty() && is_pending_key(vk))) {
            *eaten = TRUE;
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnKeyDown(ITfContext* context, WPARAM key,
                                         LPARAM, BOOL* eaten) override {
        if (!eaten) return E_INVALIDARG;
        *eaten = FALSE;
        if (!context || read_user_setting(L"Enabled", 1) == 0) return S_OK;

        const UINT vk = static_cast<UINT>(key);
        if (is_roman_letter(vk)) {
            pending_.push_back(static_cast<char>(vk));
            selected_candidate_ = 0;
            if (request_edit(context, EditAction::Update, selected_text())) {
                *eaten = TRUE;
            } else {
                pending_.pop_back();
            }
            return S_OK;
        }

        if (pending_.empty()) return S_OK;

        if (vk == VK_BACK) {
            pending_.pop_back();
            const EditAction action = pending_.empty() ? EditAction::Cancel : EditAction::Update;
            const std::wstring text = pending_.empty() ? L"" : selected_text();
            if (request_edit(context, action, text)) *eaten = TRUE;
            return S_OK;
        }

        if (vk == VK_ESCAPE) {
            if (request_edit(context, EditAction::Cancel, L"")) {
                pending_.clear();
                selected_candidate_ = 0;
                *eaten = TRUE;
            }
            return S_OK;
        }

        const auto values = current_candidates();
        if (vk >= '1' && vk <= '9') {
            const std::size_t index = static_cast<std::size_t>(vk - '1');
            if (values.size() > 1 && index < values.size()) {
                selected_candidate_ = index;
                if (request_edit(context, EditAction::Update, selected_text())) {
                    *eaten = TRUE;
                }
            }
            return S_OK;
        }

        if (vk == VK_UP || vk == VK_DOWN) {
            if (values.size() > 1) {
                if (vk == VK_DOWN) selected_candidate_ = (selected_candidate_ + 1) % values.size();
                else selected_candidate_ = (selected_candidate_ + values.size() - 1) % values.size();
                if (request_edit(context, EditAction::Update, selected_text())) {
                    *eaten = TRUE;
                }
            }
            return S_OK;
        }

        std::wstring delimiter;
        if (!get_commit_terminator(vk, &delimiter)) return S_OK;
        std::wstring final_text = selected_text();
        final_text += delimiter;
        if (request_edit(context, EditAction::Commit, final_text)) {
            pending_.clear();
            selected_candidate_ = 0;
            *eaten = TRUE;
        }
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

    HRESULT apply_edit(TfEditCookie cookie, ITfContext* context,
                       EditAction action, const std::wstring& text) {
        if (!context) return E_INVALIDARG;

        if (action == EditAction::Update && !composition_) {
            ITfInsertAtSelection* inserter = nullptr;
            HRESULT hr = context->QueryInterface(
                IID_ITfInsertAtSelection, reinterpret_cast<void**>(&inserter));
            if (FAILED(hr)) return hr;

            ITfRange* inserted_range = nullptr;
            hr = inserter->InsertTextAtSelection(
                cookie, TF_IAS_NO_DEFAULT_COMPOSITION, text.data(),
                static_cast<LONG>(text.size()), &inserted_range);
            inserter->Release();
            if (FAILED(hr)) return hr;
            if (!inserted_range) return E_FAIL;

            ITfContextComposition* context_composition = nullptr;
            hr = context->QueryInterface(
                IID_ITfContextComposition, reinterpret_cast<void**>(&context_composition));
            if (SUCCEEDED(hr)) {
                hr = context_composition->StartComposition(
                    cookie, inserted_range, nullptr, &composition_);
                context_composition->Release();
            }

            if (FAILED(hr) || !composition_) {
                inserted_range->SetText(cookie, 0, L"", 0);
                inserted_range->Release();
                composition_ = nullptr;
                return FAILED(hr) ? hr : E_FAIL;
            }

            inserted_range->Release();
            context->AddRef();
            active_context_ = context;
            update_candidate_window(cookie, context);
            return S_OK;
        }

        if (!composition_ || active_context_ != context) return TF_E_DISCONNECTED;

        ITfRange* range = nullptr;
        HRESULT hr = composition_->GetRange(&range);
        if (FAILED(hr) || !range) return FAILED(hr) ? hr : E_FAIL;

        if (action == EditAction::Cancel) {
            hr = range->SetText(cookie, 0, L"", 0);
            if (SUCCEEDED(hr)) hr = composition_->EndComposition(cookie);
            range->Release();
            if (SUCCEEDED(hr)) {
                release_composition();
                hide_candidate_window();
            }
            return hr;
        }

        hr = range->SetText(cookie, 0, text.data(), static_cast<LONG>(text.size()));
        if (FAILED(hr)) {
            range->Release();
            return hr;
        }

        hr = range->Collapse(cookie, TF_ANCHOR_END);
        if (SUCCEEDED(hr)) {
            TF_SELECTION selection{};
            selection.range = range;
            selection.style.ase = TF_AE_NONE;
            selection.style.fInterimChar = FALSE;
            hr = context->SetSelection(cookie, 1, &selection);
        }
        range->Release();
        if (FAILED(hr)) return hr;

        if (action == EditAction::Commit) {
            hr = composition_->EndComposition(cookie);
            if (SUCCEEDED(hr)) {
                release_composition();
                hide_candidate_window();
            }
            return hr;
        }

        update_candidate_window(cookie, context);
        return S_OK;
    }

private:
    static bool is_roman_letter(UINT vk) {
        return vk >= static_cast<UINT>('A') && vk <= static_cast<UINT>('Z');
    }

    bool is_pending_key(UINT vk) const {
        if (vk == VK_UP || vk == VK_DOWN || (vk >= '1' && vk <= '9')) {
            return current_candidates().size() > 1;
        }
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

    std::vector<std::string> current_candidates() const {
        input_engine::TamilTransliterator engine;
        return engine.candidates(pending_);
    }

    std::wstring selected_text() const {
        const auto values = current_candidates();
        if (values.empty()) return {};
        const std::size_t index = std::min(selected_candidate_, values.size() - 1);
        std::wstring converted;
        if (!utf8_to_wide(values[index], &converted)) return {};
        return converted;
    }

    bool request_edit(ITfContext* context, EditAction action, const std::wstring& text) {
        auto* session = new (std::nothrow)
            CompositionEditSession(this, context, action, text);
        if (!session) return false;

        HRESULT session_result = E_FAIL;
        const HRESULT request_result = context->RequestEditSession(
            client_id_, session, TF_ES_SYNC | TF_ES_READWRITE, &session_result);
        session->Release();
        return SUCCEEDED(request_result) && SUCCEEDED(session_result);
    }

    void update_candidate_window(TfEditCookie cookie, ITfContext* context) {
        if (read_user_setting(L"ShowCandidates", 1) == 0) {
            hide_candidate_window();
            return;
        }

        const auto values = current_candidates();
        if (values.size() < 2) {
            hide_candidate_window();
            return;
        }

        std::wstring display;
        for (std::size_t i = 0; i < values.size() && i < 9; ++i) {
            std::wstring item;
            if (!utf8_to_wide(values[i], &item)) continue;
            display += (i == selected_candidate_ ? L"> " : L"  ");
            display += static_cast<wchar_t>(L'1' + i);
            display += L". ";
            display += item;
            display += L"\r\n";
        }

        RECT rect{80, 80, 80, 80};
        ITfRange* range = nullptr;
        ITfContextView* view = nullptr;
        if (composition_ && SUCCEEDED(composition_->GetRange(&range))) {
            if (SUCCEEDED(context->GetActiveView(&view))) {
                BOOL clipped = FALSE;
                if (FAILED(view->GetTextExt(cookie, range, &rect, &clipped))) {
                    rect = RECT{80, 80, 80, 80};
                }
                view->Release();
            }
            range->Release();
        }

        if (!candidate_window_) {
            candidate_window_ = CreateWindowExW(
                WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                L"STATIC", display.c_str(),
                WS_POPUP | SS_LEFT | SS_NOPREFIX | WS_BORDER,
                rect.left, rect.bottom, 260, 60,
                nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        } else {
            SetWindowTextW(candidate_window_, display.c_str());
        }

        if (candidate_window_) {
            const int height = (std::min)(9, static_cast<int>(values.size())) * 26 + 8;
            SetWindowPos(candidate_window_, HWND_TOPMOST, rect.left, rect.bottom,
                         260, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
            ShowWindow(candidate_window_, SW_SHOWNOACTIVATE);
        }
    }

    void hide_candidate_window() {
        if (candidate_window_) ShowWindow(candidate_window_, SW_HIDE);
    }

    void release_composition() {
        if (composition_) {
            composition_->Release();
            composition_ = nullptr;
        }
        if (active_context_) {
            active_context_->Release();
            active_context_ = nullptr;
        }
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
    ITfComposition* composition_{nullptr};
    ITfContext* active_context_{nullptr};
    TfClientId client_id_{TF_CLIENTID_NULL};
    bool key_sink_advised_{false};
    std::string pending_;
    std::size_t selected_candidate_{0};
    HWND candidate_window_{nullptr};

    friend class CompositionEditSession;
};

CompositionEditSession::CompositionEditSession(
    TamilTextService* service, ITfContext* context, EditAction action,
    std::wstring text) noexcept
    : service_(service), context_(context), action_(action), text_(std::move(text)) {
    if (service_) service_->AddRef();
    if (context_) context_->AddRef();
    ++g_object_count;
}

CompositionEditSession::~CompositionEditSession() {
    if (context_) context_->Release();
    if (service_) service_->Release();
    --g_object_count;
}

HRESULT STDMETHODCALLTYPE CompositionEditSession::QueryInterface(
    REFIID iid, void** object) {
    if (!object) return E_POINTER;
    *object = nullptr;
    if (iid == IID_IUnknown || iid == IID_ITfEditSession) {
        *object = static_cast<ITfEditSession*>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE CompositionEditSession::AddRef() {
    return static_cast<ULONG>(++references_);
}

ULONG STDMETHODCALLTYPE CompositionEditSession::Release() {
    const ULONG left = static_cast<ULONG>(--references_);
    if (left == 0) delete this;
    return left;
}

HRESULT STDMETHODCALLTYPE CompositionEditSession::DoEditSession(TfEditCookie cookie) {
    return service_ ? service_->apply_edit(cookie, context_, action_, text_) : E_UNEXPECTED;
}

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
        const ULONG left = static_cast<ULONG>(--references_);
        if (left == 0) delete this;
        return left;
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

STDAPI DllGetClassObject(REFCLSID clsid, REFIID iid, void** object) {
    if (!object) return E_POINTER;
    *object = nullptr;
    if (clsid != CLSID_TamilTextService) return CLASS_E_CLASSNOTAVAILABLE;

    auto* factory = new (std::nothrow) TamilClassFactory();
    if (!factory) return E_OUTOFMEMORY;
    const HRESULT result = factory->QueryInterface(iid, object);
    factory->Release();
    return result;
}

STDAPI DllCanUnloadNow(void) {
    return (g_object_count.load() == 0 && g_server_locks.load() == 0)
        ? S_OK : S_FALSE;
}

BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID) {
    return TRUE;
}
