// SPDX-License-Identifier: GPL-3.0-or-later
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <string>
#include <vector>
#include <cstdio>
static std::wstring quote(const std::wstring& value)
{
    std::wstring result = L"\"";
    size_t slashes = 0;
    for (wchar_t c : value) {
        if (c == L'\\') {
            ++slashes;
            continue;
        }
        result.append(slashes * (c == L'"' ? 2 : 1), L'\\');
        slashes = 0;
        if (c == L'"')
            result += L'\\';
        result += c;
    }
    result.append(slashes * 2, L'\\');
    return result + L'"';
}
static std::wstring directory()
{
    wchar_t path[32768] {};
    GetModuleFileNameW(nullptr, path, 32768);
    auto* last = wcsrchr(path, L'\\');
    if (last)
        *last = 0;
    return path;
}
static std::wstring logDirectory()
{
    wchar_t value[32768] {};
    if (GetEnvironmentVariableW(L"KDE_ON_WIN_SETTINGS", value, 32768)) {
        auto* last = wcsrchr(value, L'\\');
        if (!last)
            last = wcsrchr(value, L'/');
        if (last) {
            *last = 0;
            return std::wstring(value) + L"\\logs";
        }
    }
    GetEnvironmentVariableW(L"LOCALAPPDATA", value, 32768);
    return std::wstring(value) + L"\\plasma-win-session\\logs";
}
// Restrict inheritance to the redirected standard handles. Session services must
// never inherit arbitrary handles belonging to the launcher or another child.
static bool launchProcess(const std::wstring& executable, std::wstring command, const std::wstring& label,
    const wchar_t* workingDirectory, bool wait, DWORD* exitCode = nullptr)
{
    const auto logs = logDirectory();
    SHCreateDirectoryExW(nullptr, logs.c_str(), nullptr);
    SECURITY_ATTRIBUTES security { sizeof(security), nullptr, TRUE };
    HANDLE output = CreateFileW((logs + L"\\" + label + L".log").c_str(), FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, &security, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    // Logging failure must not prevent a shell from starting or inherit a console.
    if (output == INVALID_HANDLE_VALUE)
        output = CreateFileW(
            L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING, 0, nullptr);
    HANDLE input
        = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING, 0, nullptr);
    if (output == INVALID_HANDLE_VALUE || input == INVALID_HANDLE_VALUE) {
        const DWORD error = GetLastError();
        if (output != INVALID_HANDLE_VALUE)
            CloseHandle(output);
        if (input != INVALID_HANDLE_VALUE)
            CloseHandle(input);
        SetLastError(error);
        return false;
    }
    STARTUPINFOEXW startup {};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdOutput = output;
    startup.StartupInfo.hStdError = output;
    startup.StartupInfo.hStdInput = input;
    SIZE_T size = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
    auto* attributes = static_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(HeapAlloc(GetProcessHeap(), 0, size));
    bool initialized = attributes && InitializeProcThreadAttributeList(attributes, 1, 0, &size);
    HANDLE handles[] { input, output };
    PROCESS_INFORMATION process {};
    BOOL created = FALSE;
    DWORD error = initialized ? ERROR_SUCCESS : GetLastError();
    if (initialized) {
        startup.lpAttributeList = attributes;
        if (UpdateProcThreadAttribute(
                attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, handles, sizeof(handles), nullptr, nullptr))
            created = CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, TRUE,
                CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT, nullptr, workingDirectory, &startup.StartupInfo,
                &process);
        error = GetLastError();
        DeleteProcThreadAttributeList(attributes);
    }
    if (attributes)
        HeapFree(GetProcessHeap(), 0, attributes);
    CloseHandle(output);
    CloseHandle(input);
    if (!created) {
        SetLastError(error);
        return false;
    }
    CloseHandle(process.hThread);
    bool ok = true;
    if (wait) {
        DWORD result = 1;
        ok = WaitForSingleObject(process.hProcess, INFINITE) == WAIT_OBJECT_0
            && GetExitCodeProcess(process.hProcess, &result);
        if (exitCode)
            *exitCode = ok ? result : 1;
    }
    CloseHandle(process.hProcess);
    return ok;
}
static bool launch(
    const std::wstring& program, const std::vector<std::wstring>& args, bool wait, DWORD* exitCode = nullptr)
{
    std::wstring executable = program;
    if (program.find_first_of(L"\\/") == std::wstring::npos) {
        wchar_t resolved[32768] {};
        const DWORD length = SearchPathW(nullptr, program.c_str(), L".exe", 32768, resolved, nullptr);
        if (length && length < 32768)
            executable = resolved;
    }
    std::wstring command = quote(executable);
    for (const auto& arg : args)
        command += L" " + (arg.empty() || arg.find_first_of(L" \t\n\v\"") != std::wstring::npos ? quote(arg) : arg);
    const auto separator = program.find_last_of(L"\\/");
    const auto label = program.substr(separator == std::wstring::npos ? 0 : separator + 1);
    return launchProcess(executable, command, label, nullptr, wait, exitCode);
}
enum class Mode { Session, Spawn, Test, Child, Invalid };
static Mode parseMode(const std::vector<std::wstring>& args)
{
    if (args.empty())
        return Mode::Session;
    if (args[0] == L"--spawn-hidden" && args.size() >= 2 && !args[1].empty())
        return Mode::Spawn;
    if (args[0] == L"--self-test" && args.size() == 1)
        return Mode::Test;
    if (args[0] == L"--test-child" && args.size() == 3)
        return Mode::Child;
    return Mode::Invalid;
}
static void explorerFallback()
{
    if (GetShellWindow())
        return;
    wchar_t windows[32768] {};
    GetWindowsDirectoryW(windows, 32768);
    const auto explorer = std::wstring(windows) + L"\\explorer.exe";
    auto command = quote(explorer);
    STARTUPINFOW startup { sizeof(startup) };
    PROCESS_INFORMATION process {};
    if (CreateProcessW(
            explorer.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process)) {
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
    }
}
static bool fallbackRequired(DWORD code, bool configured, bool desktop)
{
    return !desktop && (configured || code != 0);
}
static bool configuredShell()
{
    wchar_t value[32768] {};
    DWORD size = sizeof(value), type = 0;
    return SHGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon", L"Shell", &type,
               value, &size)
        == ERROR_SUCCESS
        && type == REG_SZ && StrStrIW(value, L"kde-session-host.exe");
}
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    int count = 0;
    auto argv = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!argv)
        return 2;
    std::vector<std::wstring> args;
    for (int i = 1; i < count; ++i)
        args.emplace_back(argv[i]);
    LocalFree(argv);
    const auto mode = parseMode(args);
    if (mode == Mode::Invalid)
        return 2;
    if (mode == Mode::Child) {
        const char text[] = "KDE hidden service fixture\r\n";
        DWORD written = 0;
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), text, sizeof(text) - 1, &written, nullptr);
        const HANDLE unrelated
            = reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(_wcstoui64(args[1].c_str(), nullptr, 10)));
        DWORD flags = 0;
        bool isolated = !GetHandleInformation(unrelated, &flags);
        if (!isolated) {
            HANDLE expected = OpenEventW(SYNCHRONIZE, FALSE, args[2].c_str());
            // This diagnostic API is not supplied by every SDK import library.
            // Resolve it only in the fixture; production startup needs no new import.
            using Compare = BOOL(WINAPI*)(HANDLE, HANDLE);
            auto compare = reinterpret_cast<Compare>(
                GetProcAddress(GetModuleHandleW(L"kernelbase.dll"), "CompareObjectHandles"));
            isolated = expected && compare && !compare(unrelated, expected);
            if (expected)
                CloseHandle(expected);
        }
        return GetConsoleWindow() == nullptr && written == sizeof(text) - 1 && isolated ? 37 : 38;
    }
    if (mode == Mode::Test) {
        DWORD code = 0;
        SECURITY_ATTRIBUTES security { sizeof(security), nullptr, TRUE };
        const auto eventName = L"Local\\KDE-HandleFixture-" + std::to_wstring(GetCurrentProcessId()) + L"-"
            + std::to_wstring(GetTickCount64());
        HANDLE unrelated = CreateEventW(&security, FALSE, FALSE, eventName.c_str());
        bool ok = unrelated
            && launch(directory() + L"\\kde-session-host.exe",
                { L"--test-child", std::to_wstring(reinterpret_cast<UINT_PTR>(unrelated)), eventName }, true, &code)
            && code == 37;
        if (unrelated)
            CloseHandle(unrelated);
        ok = ok && parseMode({ L"--self-test-extra" }) == Mode::Invalid
            && parseMode({ L"--self-test", L"extra" }) == Mode::Invalid
            && parseMode({ L"--spawn-hidden" }) == Mode::Invalid
            && parseMode({ L"--spawn-hidden", L"" }) == Mode::Invalid;
        wchar_t system[32768] {};
        GetSystemDirectoryW(system, 32768);
        DWORD consoleCode = 0;
        ok = ok && launch(std::wstring(system) + L"\\cmd.exe", { L"/d", L"/c", L"exit 19" }, true, &consoleCode)
            && consoleCode == 19;
        const std::wstring value = L"C:\\space & unicode-测试\\";
        auto command = L"fixture " + quote(value);
        int n = 0;
        auto parsed = CommandLineToArgvW(command.c_str(), &n);
        ok = ok && parsed && n == 2 && value == parsed[1];
        ok = ok && fallbackRequired(0, true, false) && !fallbackRequired(0, true, true)
            && !fallbackRequired(0, false, false) && fallbackRequired(1, false, false);
        LocalFree(parsed);
        if (!ok)
            std::printf("fixture child=%lu console=%lu last-error=%lu\n", code, consoleCode, GetLastError());
        std::puts(ok ? "PASS: hidden synthetic child, restricted handle inheritance, strict arguments, redirected logs "
                       "and Windows quoting; no desktop "
                       "session launched"
                     : "FAIL: session host fixture");
        return ok ? 0 : 1;
    }
    if (mode == Mode::Spawn) {
        if (args.size() < 2)
            return 2;
        auto program = args[1];
        std::vector<std::wstring> child(args.begin() + 2, args.end());
        return launch(program, child, false) ? 0 : 1;
    }
    auto root = directory();
    root = root.substr(0, root.find_last_of(L"\\/"));
    wchar_t system[32768] {};
    GetSystemDirectoryW(system, 32768);
    const std::wstring cmd = std::wstring(system) + L"\\cmd.exe";
    const auto script = root + L"\\session-shell.cmd";
    if (GetFileAttributesW(script.c_str()) == INVALID_FILE_ATTRIBUTES) {
        explorerFallback();
        return 2;
    }
    // cmd /s /c requires a second pair of quotes around a quoted script path.
    std::wstring command = quote(cmd) + L" /d /s /c \"" + quote(script) + L"\"";
    DWORD code = 1;
    if (!launchProcess(cmd, command, L"session-shell", root.c_str(), true, &code)) {
        explorerFallback();
        return 1;
    }
    if (fallbackRequired(code, configuredShell(), GetShellWindow() != nullptr))
        explorerFallback();
    return static_cast<int>(code);
}
