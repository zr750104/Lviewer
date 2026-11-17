// Lviewer - entry point
#include "Win.h"
#include "Viewer.h"
#include "Settings.h"
#include "I18n.h"
#include "Commands.h"
#include "core/Filters.h"
#include "Updater.h"

using namespace lv;

namespace {

// Thread-based parallel-for used by the image core (rendering/resizing large images)
struct Job { const std::function<void(int, int)>* body; int b, e; };
DWORD WINAPI Worker(LPVOID p) { Job* j = static_cast<Job*>(p); (*j->body)(j->b, j->e); return 0; }

void ParallelForImpl(int n, const std::function<void(int, int)>& body) {
    SYSTEM_INFO si; GetSystemInfo(&si);
    int t = std::max(1, std::min<int>(int(si.dwNumberOfProcessors), 16));
    if (t == 1 || n < t * 4) { body(0, n); return; }
    std::vector<Job> jobs(t);
    std::vector<HANDLE> hs;
    int chunk = (n + t - 1) / t;
    for (int i = 0; i < t; ++i) {
        jobs[i] = {&body, i * chunk, std::min(n, (i + 1) * chunk)};
        if (jobs[i].b >= jobs[i].e) continue;
        if (i == 0) continue;
        HANDLE h = CreateThread(nullptr, 0, Worker, &jobs[i], 0, nullptr);
        if (h) hs.push_back(h); else body(jobs[i].b, jobs[i].e);
    }
    body(jobs[0].b, jobs[0].e);
    if (!hs.empty()) WaitForMultipleObjects(DWORD(hs.size()), hs.data(), TRUE, INFINITE);
    for (HANDLE h : hs) CloseHandle(h);
}

StartupOptions ParseArgs() {
    StartupOptions o;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i = 1; i < argc; ++i) {
        std::wstring a = argv[i];
        std::wstring low = ToLower(a);
        if (low == L"/slide" || low == L"-slide") o.slide = true;
        else if (low == L"/getclip" || low == L"-getclip") o.getclip = true;
        else if (low == L"/blank" || low == L"-blank") o.blank = true;
        else if (low == L"/newwnd" || low == L"-newwnd") o.newwnd = true;
        else if (low.compare(0, 6, L"/user=") == 0) { o.user = a.substr(6); o.userGiven = true; }
        else if (o.path.empty()) {
            wchar_t full[32768];
            DWORD n = GetFullPathNameW(a.c_str(), 32768, full, nullptr);
            o.path = (n && n < 32768) ? full : a;
        }
    }
    LocalFree(argv);
    if (o.blank) o.getclip = false;
    return o;
}

bool ForwardToRunningInstance(const StartupOptions& o) {
    HWND w = FindWindowW(L"LviewerMainWnd", nullptr);
    if (!w) return false;
    COPYDATASTRUCT cd = {};
    cd.dwData = 0x4C56;
    cd.cbData = DWORD((o.path.size() + 1) * sizeof(wchar_t));
    cd.lpData = (void*)o.path.c_str();
    DWORD_PTR res = 0;
    AllowSetForegroundWindow(ASFW_ANY);
    return SendMessageTimeoutW(w, WM_COPYDATA, 0, LPARAM(&cd), SMTO_ABORTIFHUNG, 3000, &res) && res;
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    // Per-monitor DPI (also declared in the manifest)
    typedef BOOL(WINAPI * SetCtx)(HANDLE);
    if (auto fn = (SetCtx)(void*)GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetProcessDpiAwarenessContext"))
        fn((HANDLE)-4 /* DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 */);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    OleInitialize(nullptr);
    INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_WIN95_CLASSES | ICC_BAR_CLASSES | ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES | ICC_UPDOWN_CLASS};
    InitCommonControlsEx(&icc);
    SetParallelFor(ParallelForImpl);

    CleanupAfterUpdate();
    StartupOptions opt = ParseArgs();
    g_set.Load(ResolveIniPath(opt.user, opt.userGiven));
    I18nLoad(g_set.lang);
    g_keymap.Build();

    if (!g_set.multiInstance && !opt.newwnd) {
        HANDLE mtx = CreateMutexW(nullptr, FALSE, L"Local\\LviewerSingleInstance");
        bool exists = GetLastError() == ERROR_ALREADY_EXISTS;
        if (exists && !opt.path.empty() && ForwardToRunningInstance(opt)) return 0;
        if (exists && opt.path.empty()) {
            if (HWND w = FindWindowW(L"LviewerMainWnd", nullptr)) { SetForegroundWindow(w); return 0; }
        }
        (void)mtx;
    }

    Viewer v;
    if (!v.Create(opt)) return 1;
    int r = v.Run();
    OleUninitialize();
    CoUninitialize();
    return r;
}
