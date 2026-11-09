// SPDX-License-Identifier: GPL-3.0-or-later
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
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
static bool launch(
    const std::wstring& program, const std::vector<std::wstring>& args, bool wait, DWORD* exitCode = nullptr)
{
    std::wstring executable = program;
    if (program.find_first_of(L"\\/") == std::wstring::npos) {
        wchar_t resolved[32768] {};
        if (SearchPathW(nullptr, program.c_str(), L".exe", 32768, resolved, nullptr))
            executable = resolved;
    }
    std::wstring command = quote(executable);
    for (const auto& arg : args)
        command += L" " + (arg.empty() || arg.find_first_of(L" \t\n\v\"") != std::wstring::npos ? quote(arg) : arg);
    const auto logs = logDirectory();
    SHCreateDirectoryExW(nullptr, logs.c_str(), nullptr);
    auto separator = program.find_last_of(L"\\/");
    auto label = program.substr(separator == std::wstring::npos ? 0 : separator + 1);
    SECURITY_ATTRIBUTES security { sizeof(security), nullptr, TRUE };
    HANDLE output = CreateFileW((logs + L"\\" + label + L".log").c_str(), FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, &security, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    HANDLE input
        = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING, 0, nullptr);
    STARTUPINFOW startup { sizeof(startup) };
    PROCESS_INFORMATION process {};
    if (output != INVALID_HANDLE_VALUE && input != INVALID_HANDLE_VALUE) {
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdOutput = output;
        startup.hStdError = output;
        startup.hStdInput = input;
    }
    const BOOL created = CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
        nullptr, nullptr, &startup, &process);
    if (output != INVALID_HANDLE_VALUE)
        CloseHandle(output);
    if (input != INVALID_HANDLE_VALUE)
        CloseHandle(input);
    if (!created)
        return false;
    CloseHandle(process.hThread);
    if (wait) {
        WaitForSingleObject(process.hProcess, INFINITE);
        DWORD result = 1;
        GetExitCodeProcess(process.hProcess, &result);
        if (exitCode)
            *exitCode = result;
    }
    CloseHandle(process.hProcess);
    return true;
}
static void explorerFallback()
{
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
    if (!args.empty() && args[0] == L"--test-child") {
        const char text[] = "KDE hidden service fixture\r\n";
        DWORD written = 0;
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), text, sizeof(text) - 1, &written, nullptr);
        return GetConsoleWindow() == nullptr && written == sizeof(text) - 1 ? 37 : 38;
    }
    if (!args.empty() && args[0] == L"--self-test") {
        DWORD code = 0;
        bool ok = launch(directory() + L"\\kde-session-host.exe", { L"--test-child" }, true, &code) && code == 37;
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
        LocalFree(parsed);
        std::puts(ok ? "PASS: hidden synthetic child, redirected service logs and Windows argument quoting; no desktop "
                       "session launched"
                     : "FAIL: session host fixture");
        return ok ? 0 : 1;
    }
    if (!args.empty() && args[0] == L"--spawn-hidden") {
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
    SECURITY_ATTRIBUTES inherit { sizeof(inherit), nullptr, TRUE };
    auto logs = logDirectory();
    SHCreateDirectoryExW(nullptr, logs.c_str(), nullptr);
    HANDLE output = CreateFileW((logs + L"\\session-shell.log").c_str(), FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE, &inherit, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    HANDLE input
        = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherit, OPEN_EXISTING, 0, nullptr);
    STARTUPINFOW startup { sizeof(startup) };
    PROCESS_INFORMATION process {};
    if (output != INVALID_HANDLE_VALUE && input != INVALID_HANDLE_VALUE) {
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdOutput = output;
        startup.hStdError = output;
        startup.hStdInput = input;
    }
    const BOOL created = CreateProcessW(cmd.c_str(), command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
        root.c_str(), &startup, &process);
    if (output != INVALID_HANDLE_VALUE)
        CloseHandle(output);
    if (input != INVALID_HANDLE_VALUE)
        CloseHandle(input);
    if (!created) {
        explorerFallback();
        return 1;
    }
    CloseHandle(process.hThread);
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hProcess);
    if (code != 0)
        explorerFallback();
    return static_cast<int>(code);
}
