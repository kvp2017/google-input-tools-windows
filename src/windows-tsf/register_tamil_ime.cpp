#include <windows.h>
#include <msctf.h>

#include <cwchar>
#include <iostream>
#include <string>

static const CLSID CLSID_TamilTextService = {
    0x3a36e4c8, 0x31a8, 0x49a4, {0x9d, 0x67, 0x3e, 0x7b, 0x12, 0x8d, 0xe2, 0x41}
};
static const GUID GUID_TamilProfile = {
    0x833f4f12, 0x2b90, 0x4c21, {0x93, 0x43, 0x85, 0x69, 0x0b, 0x5c, 0x88, 0x10}
};
static constexpr LANGID kTamilIndiaLangId = 0x0449;

std::wstring clsid_string() {
    wchar_t value[64]{};
    StringFromGUID2(CLSID_TamilTextService, value, 64);
    return value;
}

HRESULT write_com_registration(const std::wstring& dll_path) {
    const std::wstring key_name =
        L"Software\\Classes\\CLSID\\" + clsid_string() + L"\\InprocServer32";
    HKEY key = nullptr;
    const LONG open_result = RegCreateKeyExW(
        HKEY_CURRENT_USER, key_name.c_str(), 0, nullptr, 0, KEY_SET_VALUE,
        nullptr, &key, nullptr);
    if (open_result != ERROR_SUCCESS) return HRESULT_FROM_WIN32(open_result);

    const DWORD path_bytes = static_cast<DWORD>((dll_path.size() + 1) * sizeof(wchar_t));
    LONG result = RegSetValueExW(key, nullptr, 0, REG_SZ,
        reinterpret_cast<const BYTE*>(dll_path.c_str()), path_bytes);
    if (result == ERROR_SUCCESS) {
        constexpr wchar_t threading[] = L"Apartment";
        result = RegSetValueExW(key, L"ThreadingModel", 0, REG_SZ,
            reinterpret_cast<const BYTE*>(threading), sizeof(threading));
    }
    RegCloseKey(key);
    return HRESULT_FROM_WIN32(result);
}

HRESULT register_profile(const std::wstring& dll_path) {
    ITfInputProcessorProfiles* profiles = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr,
        CLSCTX_INPROC_SERVER, IID_ITfInputProcessorProfiles,
        reinterpret_cast<void**>(&profiles));
    if (FAILED(hr)) return hr;

    hr = profiles->Register(CLSID_TamilTextService);
    if (FAILED(hr) && hr != TF_E_ALREADY_EXISTS) {
        profiles->Release();
        return hr;
    }

    constexpr wchar_t description[] = L"Tamil Transliteration (Preview)";
    hr = profiles->AddLanguageProfile(
        CLSID_TamilTextService, kTamilIndiaLangId, GUID_TamilProfile,
        description, static_cast<ULONG>(std::wcslen(description)),
        dll_path.c_str(), static_cast<ULONG>(dll_path.size()), 0);
    if (FAILED(hr) && hr != TF_E_ALREADY_EXISTS) {
        profiles->Release();
        return hr;
    }

    hr = profiles->EnableLanguageProfile(
        CLSID_TamilTextService, kTamilIndiaLangId, GUID_TamilProfile, TRUE);
    profiles->Release();
    return hr;
}

HRESULT unregister_profile() {
    ITfInputProcessorProfiles* profiles = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr,
        CLSCTX_INPROC_SERVER, IID_ITfInputProcessorProfiles,
        reinterpret_cast<void**>(&profiles));
    if (SUCCEEDED(hr)) {
        profiles->RemoveLanguageProfile(
            CLSID_TamilTextService, kTamilIndiaLangId, GUID_TamilProfile);
        profiles->Unregister(CLSID_TamilTextService);
        profiles->Release();
    }

    const std::wstring key_name =
        L"Software\\Classes\\CLSID\\" + clsid_string();
    const LONG remove_result = RegDeleteTreeW(HKEY_CURRENT_USER, key_name.c_str());
    if (remove_result != ERROR_SUCCESS && remove_result != ERROR_FILE_NOT_FOUND) {
        return HRESULT_FROM_WIN32(remove_result);
    }
    return S_OK;
}

int wmain(int argc, wchar_t** argv) {
    if (argc < 2) {
        std::wcerr << L"Usage: TamilImeRegistration.exe /register <dll-path> | /unregister\n";
        return 2;
    }

    const HRESULT init_hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(init_hr) && init_hr != RPC_E_CHANGED_MODE) {
        std::wcerr << L"COM initialization failed: 0x" << std::hex << init_hr << L"\n";
        return 3;
    }

    HRESULT hr = E_INVALIDARG;
    const std::wstring command = argv[1];
    if (_wcsicmp(command.c_str(), L"/register") == 0 && argc >= 3) {
        const std::wstring dll_path = argv[2];
        hr = write_com_registration(dll_path);
        if (SUCCEEDED(hr)) hr = register_profile(dll_path);
    } else if (_wcsicmp(command.c_str(), L"/unregister") == 0) {
        hr = unregister_profile();
    }

    if (init_hr == S_OK || init_hr == S_FALSE) CoUninitialize();

    if (FAILED(hr)) {
        std::wcerr << L"TSF profile operation failed: 0x" << std::hex << hr << L"\n";
        return 1;
    }
    std::wcout << L"TSF profile operation completed. Switch input methods with Win + Space.\n";
    return 0;
}
