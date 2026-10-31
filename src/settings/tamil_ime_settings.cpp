#include <windows.h>
#include <shellapi.h>

namespace {
constexpr int kEnableId = 1001;
constexpr int kCandidatesId = 1002;
constexpr int kSaveId = 1003;
constexpr int kLanguageSettingsId = 1004;
HWND g_enable = nullptr;
HWND g_candidates = nullptr;

DWORD read_setting(const wchar_t* name, DWORD fallback) {
    DWORD value = fallback;
    DWORD size = sizeof(value);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\KVP\\TamilIme", name,
                     RRF_RT_REG_DWORD, nullptr, &value, &size) != ERROR_SUCCESS) {
        return fallback;
    }
    return value;
}

void save_settings(HWND owner) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\KVP\\TamilIme",
            0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) {
        MessageBoxW(owner, L"Unable to save settings for the current user.",
                    L"Tamil Input Settings", MB_ICONERROR);
        return;
    }

    const DWORD enabled = SendMessageW(g_enable, BM_GETCHECK, 0, 0) == BST_CHECKED ? 1 : 0;
    const DWORD candidates = SendMessageW(g_candidates, BM_GETCHECK, 0, 0) == BST_CHECKED ? 1 : 0;
    RegSetValueExW(key, L"Enabled", 0, REG_DWORD,
                   reinterpret_cast<const BYTE*>(&enabled), sizeof(enabled));
    RegSetValueExW(key, L"ShowCandidates", 0, REG_DWORD,
                   reinterpret_cast<const BYTE*>(&candidates), sizeof(candidates));
    RegCloseKey(key);

    MessageBoxW(owner, L"Settings saved. Re-select Tamil Transliteration if it is already active.",
                L"Tamil Input Settings", MB_ICONINFORMATION);
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM) {
    switch (message) {
    case WM_CREATE:
        CreateWindowW(L"STATIC", L"Tamil Transliteration settings",
            WS_CHILD | WS_VISIBLE, 20, 18, 380, 24, window, nullptr, nullptr, nullptr);
        g_enable = CreateWindowW(L"BUTTON", L"Enable Tamil transliteration",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 24, 58, 360, 26,
            window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kEnableId)), nullptr, nullptr);
        g_candidates = CreateWindowW(L"BUTTON", L"Show candidate suggestions",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 24, 92, 360, 26,
            window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kCandidatesId)), nullptr, nullptr);
        SendMessageW(g_enable, BM_SETCHECK, read_setting(L"Enabled", 1) ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(g_candidates, BM_SETCHECK, read_setting(L"ShowCandidates", 1) ? BST_CHECKED : BST_UNCHECKED, 0);
        CreateWindowW(L"STATIC", L"Switch input methods using Windows: Win + Space.",
            WS_CHILD | WS_VISIBLE, 24, 132, 400, 24, window, nullptr, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Save settings",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 24, 174, 130, 32,
            window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kSaveId)), nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Windows language settings",
            WS_CHILD | WS_VISIBLE, 166, 174, 220, 32,
            window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kLanguageSettingsId)), nullptr, nullptr);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wparam)) {
        case kSaveId:
            save_settings(window);
            return 0;
        case kLanguageSettingsId:
            ShellExecuteW(window, L"open", L"ms-settings:regionlanguage",
                           nullptr, nullptr, SW_SHOWNORMAL);
            return 0;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, 0);
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    const wchar_t class_name[] = L"KVPTamilImeSettingsWindow";
    WNDCLASSW wc{};
    wc.lpfnWndProc = window_proc;
    wc.hInstance = instance;
    wc.lpszClassName = class_name;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    if (!RegisterClassW(&wc)) return 1;

    HWND window = CreateWindowExW(0, class_name, L"Tamil Input Settings",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 440, 270,
        nullptr, nullptr, instance, nullptr);
    if (!window) return 2;

    ShowWindow(window, show_command);
    UpdateWindow(window);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}
