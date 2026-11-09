// SPDX-License-Identifier: GPL-3.0-or-later
#define SECURITY_WIN32
#include <windows.h>
#include <credentialprovider.h>
#include <wincred.h>
#include <ntsecapi.h>
#include <shlwapi.h>
#include <propkey.h>
#include <ShlGuid.h>
#include <atomic>
#include <string>
#include <vector>
#include <new>

static const CLSID ProviderId = { 0x2b2e39be, 0x4e8a, 0x47aa, { 0x9f, 0xc1, 0xe3, 0x88, 0xf3, 0x4f, 0x26, 0x01 } };
static std::atomic<long> objects { 0 }, locks { 0 };
static bool chinese() { return PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_CHINESE; }
static HRESULT copy(const wchar_t* value, PWSTR* out) { return out ? SHStrDupW(value, out) : E_POINTER; }
static HBITMAP tile()
{
    HDC screen = GetDC(nullptr), dc = CreateCompatibleDC(screen);
    HBITMAP bitmap = CreateCompatibleBitmap(screen, 192, 192);
    auto old = SelectObject(dc, bitmap);
    RECT rect { 0, 0, 192, 192 };
    HBRUSH brush = CreateSolidBrush(RGB(61, 174, 233));
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    HFONT font = CreateFontW(
        -62, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    auto previous = SelectObject(dc, font);
    DrawTextW(dc, L"KDE", -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, previous);
    DeleteObject(font);
    SelectObject(dc, old);
    DeleteDC(dc);
    ReleaseDC(nullptr, screen);
    return bitmap;
}

class Credential final : public ICredentialProviderCredential2 {
    std::atomic<ULONG> refs { 1 };
    std::wstring user, sid;
    std::vector<wchar_t> password;
    ICredentialProviderCredentialEvents* events = nullptr;
    void clear()
    {
        if (!password.empty())
            SecureZeroMemory(password.data(), password.size() * sizeof(wchar_t));
        password.clear();
    }

public:
    Credential(std::wstring name, std::wstring identity)
        : user(std::move(name))
        , sid(std::move(identity))
    {
        ++objects;
    }
    ~Credential()
    {
        clear();
        if (events)
            events->Release();
        --objects;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** out) override
    {
        if (!out)
            return E_POINTER;
        *out = nullptr;
        if (id != IID_IUnknown && id != IID_ICredentialProviderCredential && id != IID_ICredentialProviderCredential2)
            return E_NOINTERFACE;
        *out = static_cast<ICredentialProviderCredential2*>(this);
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override
    {
        auto n = --refs;
        if (!n)
            delete this;
        return n;
    }
    HRESULT STDMETHODCALLTYPE Advise(ICredentialProviderCredentialEvents* value) override
    {
        if (events)
            events->Release();
        events = value;
        if (events)
            events->AddRef();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE UnAdvise() override
    {
        if (events)
            events->Release();
        events = nullptr;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetSelected(BOOL* automatic) override
    {
        if (!automatic)
            return E_POINTER;
        *automatic = FALSE;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetDeselected() override
    {
        clear();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetUserSid(PWSTR* out) override { return copy(sid.c_str(), out); }
    HRESULT STDMETHODCALLTYPE GetFieldState(DWORD id, CREDENTIAL_PROVIDER_FIELD_STATE* state,
        CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE* interactive) override
    {
        if (id >= 6 || !state || !interactive)
            return E_INVALIDARG;
        *state = id < 2 ? CPFS_DISPLAY_IN_BOTH : CPFS_DISPLAY_IN_SELECTED_TILE;
        *interactive = id == 4 ? CPFIS_FOCUSED : (id == 3 ? CPFIS_DISABLED : CPFIS_NONE);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetStringValue(DWORD id, PWSTR* out) override
    {
        switch (id) {
        case 1:
            return copy(L"KDE-on-Win", out);
        case 2:
            return copy(chinese() ? L"登录 KDE · 本地账户" : L"Sign in to KDE · Local account", out);
        case 3:
            return copy(user.c_str(), out);
        case 4:
            return copy(password.empty() ? L"" : password.data(), out);
        case 5:
            return copy(chinese() ? L"登录" : L"Sign in", out);
        default:
            return E_INVALIDARG;
        }
    }
    HRESULT STDMETHODCALLTYPE GetBitmapValue(DWORD id, HBITMAP* out) override
    {
        if (id != 0 || !out)
            return E_INVALIDARG;
        *out = tile();
        return *out ? S_OK : E_OUTOFMEMORY;
    }
    HRESULT STDMETHODCALLTYPE GetCheckboxValue(DWORD, BOOL*, PWSTR*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetSubmitButtonValue(DWORD id, DWORD* adjacent) override
    {
        if (id != 5 || !adjacent)
            return E_INVALIDARG;
        *adjacent = 4;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetComboBoxValueCount(DWORD, DWORD*, DWORD*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetComboBoxValueAt(DWORD, DWORD, PWSTR*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetStringValue(DWORD id, PCWSTR text) override
    {
        if (id != 4 || !text)
            return E_INVALIDARG;
        clear();
        try {
            password.assign(text, text + wcslen(text) + 1);
        } catch (...) {
            clear();
            return E_OUTOFMEMORY;
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetCheckboxValue(DWORD, BOOL) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetComboBoxSelectedValue(DWORD, DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CommandLinkClicked(DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetSerialization(CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE* response,
        CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* serialization, PWSTR* status,
        CREDENTIAL_PROVIDER_STATUS_ICON* icon) override
    {
        if (!response || !serialization || !status || !icon)
            return E_POINTER;
        *response = CPGSR_NO_CREDENTIAL_NOT_FINISHED;
        *serialization = {};
        *status = nullptr;
        *icon = CPSI_NONE;
        if (user.empty())
            return E_INVALIDARG;
        DWORD size = 0;
        wchar_t empty[] = L"";
        wchar_t* secret = password.empty() ? empty : password.data();
        // Windows packs/protects the secret. No custom authentication or logging.
        CredPackAuthenticationBufferW(CRED_PACK_PROTECTED_CREDENTIALS, user.data(), secret, nullptr, &size);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
            return HRESULT_FROM_WIN32(GetLastError());
        const DWORD allocated = size;
        BYTE* buffer = static_cast<BYTE*>(CoTaskMemAlloc(size));
        if (!buffer)
            return E_OUTOFMEMORY;
        if (!CredPackAuthenticationBufferW(CRED_PACK_PROTECTED_CREDENTIALS, user.data(), secret, buffer, &size)) {
            auto error = GetLastError();
            SecureZeroMemory(buffer, allocated);
            CoTaskMemFree(buffer);
            return HRESULT_FROM_WIN32(error);
        }
        HANDLE connection = nullptr;
        LSA_STRING package {};
        char name[] = "Negotiate";
        package.Buffer = name;
        package.Length = 9;
        package.MaximumLength = 10;
        ULONG authentication = 0;
        NTSTATUS result = LsaConnectUntrusted(&connection);
        if (!result) {
            result = LsaLookupAuthenticationPackage(connection, &package, &authentication);
            LsaDeregisterLogonProcess(connection);
        }
        if (result) {
            SecureZeroMemory(buffer, size);
            CoTaskMemFree(buffer);
            return HRESULT_FROM_WIN32(LsaNtStatusToWinError(result));
        }
        serialization->ulAuthenticationPackage = authentication;
        serialization->clsidCredentialProvider = ProviderId;
        serialization->cbSerialization = size;
        serialization->rgbSerialization = buffer;
        *response = CPGSR_RETURN_CREDENTIAL_FINISHED;
        clear();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE ReportResult(
        NTSTATUS result, NTSTATUS, PWSTR* status, CREDENTIAL_PROVIDER_STATUS_ICON* icon) override
    {
        if (!status || !icon)
            return E_POINTER;
        *status = nullptr;
        *icon = CPSI_NONE;
        clear();
        if (events)
            events->SetFieldString(this, 4, L"");
        if (result) {
            *icon = CPSI_ERROR;
            return copy(chinese() ? L"登录未完成，请重试或选择系统登录方式。"
                                  : L"Sign-in failed. Retry or choose a Windows sign-in option.",
                status);
        }
        return S_OK;
    }
};

class Provider final : public ICredentialProvider, public ICredentialProviderSetUserArray {
    std::atomic<ULONG> refs { 1 };
    std::vector<Credential*> credentials;
    void clear()
    {
        for (auto* c : credentials)
            c->Release();
        credentials.clear();
    }

public:
    Provider() { ++objects; }
    ~Provider()
    {
        clear();
        --objects;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** out) override
    {
        if (!out)
            return E_POINTER;
        *out = nullptr;
        if (id == IID_IUnknown || id == IID_ICredentialProvider)
            *out = static_cast<ICredentialProvider*>(this);
        else if (id == IID_ICredentialProviderSetUserArray)
            *out = static_cast<ICredentialProviderSetUserArray*>(this);
        else
            return E_NOINTERFACE;
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override
    {
        auto n = --refs;
        if (!n)
            delete this;
        return n;
    }
    HRESULT STDMETHODCALLTYPE SetUsageScenario(CREDENTIAL_PROVIDER_USAGE_SCENARIO scenario, DWORD) override
    {
        return scenario == CPUS_LOGON ? S_OK : E_NOTIMPL; // Never participate in UAC/CredUI/password-change.
    }
    HRESULT STDMETHODCALLTYPE SetSerialization(const CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION*) override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE Advise(ICredentialProviderEvents*, UINT_PTR) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE UnAdvise() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetUserArray(ICredentialProviderUserArray* array) override
    {
        clear();
        if (!array)
            return E_INVALIDARG;
        wchar_t computer[MAX_COMPUTERNAME_LENGTH + 1] {};
        DWORD length = MAX_COMPUTERNAME_LENGTH + 1;
        GetComputerNameW(computer, &length);
        std::wstring prefix;
        try {
            prefix = std::wstring(computer) + L"\\";
        } catch (...) {
            return E_OUTOFMEMORY;
        }
        DWORD count = 0;
        array->GetCount(&count);
        for (DWORD i = 0; i < count; ++i) {
            ICredentialProviderUser* user = nullptr;
            if (FAILED(array->GetAt(i, &user)))
                continue;
            PWSTR name = nullptr, sid = nullptr;
            user->GetStringValue(PKEY_Identity_QualifiedUserName, &name);
            user->GetSid(&sid);
            Credential* added = nullptr;
            try {
                if (name && sid && _wcsnicmp(name, prefix.c_str(), prefix.size()) == 0) {
                    added = new Credential(name, sid);
                    credentials.push_back(added);
                    added = nullptr;
                }
            } catch (...) {
                if (added)
                    added->Release();
                CoTaskMemFree(name);
                CoTaskMemFree(sid);
                user->Release();
                clear();
                return E_OUTOFMEMORY;
            }
            CoTaskMemFree(name);
            CoTaskMemFree(sid);
            user->Release();
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetFieldDescriptorCount(DWORD* out) override
    {
        if (!out)
            return E_POINTER;
        *out = 6;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetFieldDescriptorAt(DWORD id, CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR** out) override
    {
        if (!out || id >= 6)
            return E_INVALIDARG;
        *out = nullptr;
        static const CREDENTIAL_PROVIDER_FIELD_TYPE types[] = { CPFT_TILE_IMAGE, CPFT_LARGE_TEXT, CPFT_SMALL_TEXT,
            CPFT_EDIT_TEXT, CPFT_PASSWORD_TEXT, CPFT_SUBMIT_BUTTON };
        auto* field = static_cast<CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR*>(
            CoTaskMemAlloc(sizeof(CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR)));
        if (!field)
            return E_OUTOFMEMORY;
        *field = {};
        field->dwFieldID = id;
        field->cpft = types[id];
        if (id == 0)
            field->guidFieldType = CPFG_CREDENTIAL_PROVIDER_LOGO;
        if (id == 1)
            field->guidFieldType = CPFG_CREDENTIAL_PROVIDER_LABEL;
        if (id == 3)
            field->guidFieldType = CPFG_LOGON_USERNAME;
        if (id == 4)
            field->guidFieldType = CPFG_LOGON_PASSWORD;
        const wchar_t* label = id == 4 ? (chinese() ? L"密码" : L"Password")
                                       : (id == 3 ? (chinese() ? L"本地账户" : L"Local account") : L"KDE-on-Win");
        HRESULT hr = copy(label, &field->pszLabel);
        if (FAILED(hr)) {
            CoTaskMemFree(field);
            return hr;
        }
        *out = field;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetCredentialCount(DWORD* count, DWORD* selected, BOOL* automatic) override
    {
        if (!count || !selected || !automatic)
            return E_POINTER;
        *count = static_cast<DWORD>(credentials.size());
        *selected = CREDENTIAL_PROVIDER_NO_DEFAULT;
        *automatic = FALSE;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetCredentialAt(DWORD index, ICredentialProviderCredential** out) override
    {
        if (!out || index >= credentials.size())
            return E_INVALIDARG;
        *out = credentials[index];
        (*out)->AddRef();
        return S_OK;
    }
};
class Factory final : public IClassFactory {
    std::atomic<ULONG> refs { 1 };

public:
    Factory() { ++objects; }
    ~Factory() { --objects; }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** out) override
    {
        if (!out)
            return E_POINTER;
        *out = nullptr;
        if (id != IID_IUnknown && id != IID_IClassFactory)
            return E_NOINTERFACE;
        *out = this;
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override
    {
        auto n = --refs;
        if (!n)
            delete this;
        return n;
    }
    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* outer, REFIID id, void** out) override
    {
        if (outer)
            return CLASS_E_NOAGGREGATION;
        auto* provider = new (std::nothrow) Provider;
        if (!provider)
            return E_OUTOFMEMORY;
        auto result = provider->QueryInterface(id, out);
        provider->Release();
        return result;
    }
    HRESULT STDMETHODCALLTYPE LockServer(BOOL lock) override
    {
        if (lock)
            ++locks;
        else
            --locks;
        return S_OK;
    }
};
extern "C" HRESULT __stdcall DllGetClassObject(REFCLSID clsid, REFIID iid, void** out)
{
    if (clsid != ProviderId)
        return CLASS_E_CLASSNOTAVAILABLE;
    auto* factory = new (std::nothrow) Factory;
    if (!factory)
        return E_OUTOFMEMORY;
    auto result = factory->QueryInterface(iid, out);
    factory->Release();
    return result;
}
extern "C" HRESULT __stdcall DllCanUnloadNow() { return objects == 0 && locks == 0 ? S_OK : S_FALSE; }
extern "C" HRESULT __stdcall KDELogonSelfTest()
{
    // Synthetic credentials only: pack/unpack; never call LsaLogonUser.
    Credential* credential = new Credential(L"KDEFixture\\SyntheticUser", L"S-1-5-21-1-2-3-9999");
    credential->SetStringValue(4, L"synthetic-not-an-account-password");
    CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE response {};
    CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION data {};
    PWSTR status = nullptr;
    CREDENTIAL_PROVIDER_STATUS_ICON icon {};
    HRESULT result = credential->GetSerialization(&response, &data, &status, &icon);
    bool ok = SUCCEEDED(result) && response == CPGSR_RETURN_CREDENTIAL_FINISHED
        && data.clsidCredentialProvider == ProviderId;
    wchar_t name[512] {}, domain[512] {}, password[512] {};
    DWORD n = 512, d = 512, p = 512;
    ok = ok
        && CredUnPackAuthenticationBufferW(CRED_PACK_PROTECTED_CREDENTIALS, data.rgbSerialization, data.cbSerialization,
            name, &n, domain, &d, password, &p)
        && wcscmp(password, L"synthetic-not-an-account-password") == 0;
    SecureZeroMemory(password, sizeof(password));
    if (data.rgbSerialization) {
        SecureZeroMemory(data.rgbSerialization, data.cbSerialization);
        CoTaskMemFree(data.rgbSerialization);
    }
    CoTaskMemFree(status);
    credential->Release();
    Provider* provider = new Provider;
    DWORD count = 9, selected = 0;
    BOOL automatic = TRUE;
    ok = ok && provider->SetUsageScenario(CPUS_CREDUI, 0) == E_NOTIMPL
        && SUCCEEDED(provider->GetCredentialCount(&count, &selected, &automatic)) && count == 0 && !automatic
        && selected == CREDENTIAL_PROVIDER_NO_DEFAULT;
    CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR* field = nullptr;
    ok = ok && SUCCEEDED(provider->GetFieldDescriptorAt(0, &field))
        && field->guidFieldType == CPFG_CREDENTIAL_PROVIDER_LOGO;
    if (field) {
        CoTaskMemFree(field->pszLabel);
        CoTaskMemFree(field);
    }
    field = nullptr;
    ok = ok && SUCCEEDED(provider->GetFieldDescriptorAt(4, &field)) && field->cpft == CPFT_PASSWORD_TEXT
        && field->guidFieldType == CPFG_LOGON_PASSWORD;
    if (field) {
        CoTaskMemFree(field->pszLabel);
        CoTaskMemFree(field);
    }
    provider->Release();
    return ok ? S_OK : E_FAIL;
}
