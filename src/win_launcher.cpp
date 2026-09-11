#include <windows.h>
#include <shellapi.h>
#include <string>
#include <vector>

static std::wstring getArg(const std::wstring& cmd) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(cmd.c_str(), &argc);
    std::wstring res;
    if (argv && argc > 1) {
        res = argv[1];
        LocalFree(argv);
    }
    return res;
}

static bool runBrowser(const std::wstring& url) {
    std::vector<std::wstring> browsers = {
        L"msedge.exe",
        L"chrome.exe",
        L"brave.exe"
    };

    std::wstring args = L"--app=" + url + L" --disable-gpu-vsync --disable-frame-rate-limit --force-gpu-rasterization";

    for (const auto& b : browsers) {
        HINSTANCE h = ShellExecuteW(NULL, L"open", b.c_str(), args.c_str(), NULL, SW_SHOWNORMAL);
        if ((INT_PTR)h > 32) return true;
    }

    HINSTANCE h = ShellExecuteW(NULL, L"open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
    return ((INT_PTR)h > 32);
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    std::wstring cmd = GetCommandLineW();
    std::wstring target = getArg(cmd);

    if (target.empty()) {
        wchar_t buf[256] = L"127.0.0.1:8080";
        target = buf;
    }

    if (target.find(L"://") == std::wstring::npos) {
        target = L"http://" + target + L"/";
    }

    if (!runBrowser(target)) {
        MessageBoxW(NULL, L"Could not launch browser for BurstVNC stream.", L"BurstVNC", MB_ICONERROR);
        return 1;
    }

    return 0;
}
