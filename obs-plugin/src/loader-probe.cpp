#include <obs-module.h>
#include <windows.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("t0g-loader-probe", "en-US")

static std::filesystem::path report_path()
{
    wchar_t localAppData[MAX_PATH] = {};
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH);
    std::filesystem::path base = (n > 0 && n < MAX_PATH) ? localAppData : L".";
    base /= L"T0G-Stream-Control";
    std::error_code ec;
    std::filesystem::create_directories(base, ec);
    return base / L"obs-loader-probe.txt";
}

static std::filesystem::path self_directory()
{
    HMODULE self = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(&self_directory), &self)) {
        return {};
    }

    wchar_t path[MAX_PATH] = {};
    DWORD len = GetModuleFileNameW(self, path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH)
        return {};

    return std::filesystem::path(path).parent_path();
}

static void write_probe_line(std::ofstream &out, const std::string &line)
{
    out << line << "\n";
    out.flush();
    blog(LOG_INFO, "[T0G Loader Probe] %s", line.c_str());
}

MODULE_EXPORT const char *obs_module_description(void)
{
    return "T0G in-process Windows loader diagnostic";
}

MODULE_EXPORT bool obs_module_load(void)
{
    const auto report = report_path();
    std::ofstream out(report, std::ios::out | std::ios::trunc);

    write_probe_line(out, "T0G in-process OBS loader probe");
    write_probe_line(out, "Probe is executing inside obs64.exe.");

    const auto dir = self_directory();
    const auto target = dir / L"t0g-stream-control.dll";
    write_probe_line(out, "Target: " + target.string());

    constexpr DWORD flags =
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS;

    SetLastError(ERROR_SUCCESS);
    HMODULE module = LoadLibraryExW(target.c_str(), nullptr, flags);
    if (!module) {
        DWORD code = GetLastError();
        write_probe_line(out, "LoadLibraryExW FAILED. Win32 error: " +
                                  std::to_string(code));

        LPSTR message = nullptr;
        DWORD chars = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPSTR>(&message), 0, nullptr);

        if (chars && message) {
            write_probe_line(out, std::string("Message: ") + message);
            LocalFree(message);
        }
        return true;
    }

    write_probe_line(out, "LoadLibraryExW PASS.");

    const char *required[] = {
        "obs_module_load",
        "obs_module_set_pointer",
        "obs_module_ver",
    };

    for (const char *name : required) {
        FARPROC proc = GetProcAddress(module, name);
        write_probe_line(out, std::string(proc ? "EXPORT PASS: " : "EXPORT MISSING: ") +
                                  name);
    }

    FreeLibrary(module);
    write_probe_line(out, "Probe complete; target library released.");
    return true;
}

MODULE_EXPORT void obs_module_unload(void)
{
}
