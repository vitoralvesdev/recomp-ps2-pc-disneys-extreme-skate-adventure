#include "app.h"

#include "config.h"
#include "install.h"
#include "layout.h"
#include "log.h"
#include "package.h"
#include "child_process.h"
#include "resource.h"
#include "ui.h"
#include "update.h"
#include "version.h"

#include <commdlg.h>
#include <gdiplus.h>
#include <shellapi.h>
#include <shlobj.h>
#include <softpub.h>
#include <tlhelp32.h>
#include <WinTrust.h>
#include <windowsx.h>
#include <wintrust.h>

#pragma comment(lib, "wintrust.lib")

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <cwchar>
#include <deque>
#include <process.h>
#include <functional>
#include <iterator>
#include <sstream>
#include <thread>
#include <vector>

namespace ds {
namespace {

constexpr wchar_t kWindowClass[] = L"HighSkyDisneySkateLauncher";
constexpr wchar_t kWindowTitle[] = L"Disney's Extreme Skate Adventure";
constexpr UINT WM_APP_INSTALL_EVENT = WM_APP + 11;
constexpr UINT WM_APP_INSTALL_DONE = WM_APP + 12;
constexpr UINT WM_APP_GAME_EXITED = WM_APP + 13;
constexpr UINT WM_APP_UPDATE_DONE = WM_APP + 14;
constexpr UINT WM_APP_DOWNLOAD_PROGRESS = WM_APP + 15;
constexpr UINT WM_APP_DOWNLOAD_DONE = WM_APP + 16;
constexpr UINT WM_APP_STARTUP = WM_APP + 17;
constexpr UINT WM_APP_UNINSTALL_DONE = WM_APP + 18;
constexpr UINT_PTR kUpdateSpinnerTimer = 1;

constexpr uint32_t kBgTop = 0x080D19;
constexpr uint32_t kBgBottom = 0x141C32;
constexpr uint32_t kCard = 0x111A2E;
constexpr uint32_t kCardSoft = 0x17213A;
constexpr uint32_t kField = 0x0B1324;
constexpr uint32_t kBorder = 0x26334F;
constexpr uint32_t kText = 0xF2F5FF;
constexpr uint32_t kMuted = 0x94A2C4;
constexpr uint32_t kFaint = 0x64739A;
constexpr uint32_t kAccent = 0xFF8A1E;
constexpr uint32_t kAccentLight = 0xFFB25A;
constexpr uint32_t kSuccess = 0x45D483;
constexpr uint32_t kWarning = 0xF2C14E;
constexpr uint32_t kDanger = 0xFF6B6B;
constexpr uint32_t kBlue = 0x5AA9FF;

using Gdiplus::RectF;

enum Page { PageHome, PageInstalling, PageReady, PageSettings, PageUpdates, PageLogs };
enum NoticeKind { NoticeInfo, NoticeSuccess, NoticeWarning, NoticeError };
enum UpdateModalState { UpdateModalNone, UpdateModalChecking, UpdateModalNoUpdates };

enum Action {
    ActNone = 0,
    ActSelectIso,
    ActInstall,
    ActRefreshPackage,
    ActCancelInstall,
    ActRetryInstall,
    ActBackHome,
    ActPlay,
    ActBringGame,
    ActSettings,
    ActUpdates,
    ActLogs,
    ActOpenGameFolder,
    ActBack,
    ActSaveSettings,
    ActCancelSettings,
    ActScale1,
    ActScale2,
    ActScale3,
    ActGpu,
    ActSoftware,
    ActVblankOn,
    ActVblankOff,
    ActDiagOn,
    ActDiagOff,
    ActUpdatesOn,
    ActUpdatesOff,
    ActCheckUpdates,
    ActOpenRelease,
    ActDownloadInstaller,
    ActOpenSaves,
    ActOpenLogsFolder,
    ActOpenControls,
    ActRefreshLogs,
    ActCopyDiagnostics,
    ActUninstall,
    ActCloseUpdateModal,
    ActClose,
};

struct HitTarget {
    RectF rect;
    int id = 0;
};

struct FontSet {
    std::unique_ptr<Gdiplus::Font> title;
    std::unique_ptr<Gdiplus::Font> heading;
    std::unique_ptr<Gdiplus::Font> subheading;
    std::unique_ptr<Gdiplus::Font> body;
    std::unique_ptr<Gdiplus::Font> bodyBold;
    std::unique_ptr<Gdiplus::Font> smallText;
    std::unique_ptr<Gdiplus::Font> smallBoldText;
    std::unique_ptr<Gdiplus::Font> button;
    std::unique_ptr<Gdiplus::Font> buttonSmall;
    std::unique_ptr<Gdiplus::Font> mono;
    std::unique_ptr<Gdiplus::Font> huge;
};

struct UpdateOutcome {
    bool ok = false;
    std::wstring error;
    ReleaseInfo release;
    bool hasUpdate = false;
    bool manual = false;
};

struct DownloadOutcome {
    bool ok = false;
    std::wstring error;
    fs::path path;
};

struct UninstallOutcome {
    bool ok = false;
    std::wstring error;
};

int hitId(int action, int value) { return action * 16 + value; }

std::wstring firstLine(const std::wstring& text) {
    const size_t newline = text.find(L'\n');
    std::wstring line = newline == std::wstring::npos ? text : text.substr(0, newline);
    if (line.size() > 180) line = line.substr(0, 177) + L"...";
    return line;
}

bool hasAsciiOnly(const std::wstring& text) {
    for (wchar_t ch : text) {
        if (ch > 0x7f) return false;
    }
    return true;
}

bool verifyInstallerSignature(const fs::path& path, std::wstring& error) {
    WINTRUST_FILE_INFO fileInfo{};
    fileInfo.cbStruct = sizeof(fileInfo);
    fileInfo.pcwszFilePath = path.c_str();

    GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    WINTRUST_DATA trustData{};
    trustData.cbStruct = sizeof(trustData);
    trustData.dwUIChoice = WTD_UI_NONE;
    trustData.fdwRevocationChecks = WTD_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT;
    trustData.dwUnionChoice = WTD_CHOICE_FILE;
    trustData.pFile = &fileInfo;
    trustData.dwStateAction = WTD_STATEACTION_VERIFY;

    const LONG status = WinVerifyTrust(nullptr, &action, &trustData);
    trustData.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(nullptr, &action, &trustData);
    if (status != S_OK) {
        error = L"O Windows não validou uma assinatura digital confiável para o instalador: " +
                windowsErrorMessage(static_cast<DWORD>(status));
        return false;
    }
    return true;
}

std::wstring relativeDisplayPath(const fs::path& path, const fs::path& base) {
    std::error_code ec;
    const fs::path relative = fs::relative(path, base, ec);
    if (!ec && !relative.empty()) {
        const std::wstring text = relative.wstring();
        if (text.rfind(L"..", 0) != 0) return text;
    }
    return path.wstring();
}

bool runtimeProcessRunning(const Layout& layout, std::wstring& error) {
    error.clear();
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        error = L"Não foi possível verificar os processos antes da desinstalação.";
        return true;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    bool running = false;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, L"DisneySkateRuntime.exe") != 0) continue;

            HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID);
            if (!process) {
                error = L"Não foi possível verificar se o jogo está aberto. Feche-o e tente novamente.";
                running = true;
                break;
            }

            wchar_t processPath[32768]{};
            DWORD pathSize = static_cast<DWORD>(std::size(processPath));
            if (!QueryFullProcessImageNameW(process, 0, processPath, &pathSize)) {
                error = L"Não foi possível verificar se o jogo está aberto. Feche-o e tente novamente.";
                running = true;
            } else {
                std::error_code ec;
                const bool sameRuntime = fs::equivalent(fs::path(processPath), layout.runtimeExe(), ec);
                if ((!ec && sameRuntime) ||
                    (ec && _wcsicmp(processPath, layout.runtimeExe().wstring().c_str()) == 0)) {
                    running = true;
                }
            }
            CloseHandle(process);
            if (running) break;
        } while (Process32NextW(snapshot, &entry));
        if (!running && GetLastError() != ERROR_NO_MORE_FILES) {
            error = L"Não foi possível verificar os processos antes da desinstalação.";
            running = true;
        }
    } else {
        error = L"Não foi possível verificar os processos antes da desinstalação.";
        running = true;
    }
    CloseHandle(snapshot);
    return running;
}

std::wstring relativeDataArgument(const Layout& layout, const fs::path& executable) {
    std::error_code ec;
    const fs::path relative = fs::relative(executable, layout.dataRoot, ec);
    if (!ec && !relative.empty()) {
        const std::wstring generic = relative.generic_wstring();
        if (hasAsciiOnly(generic)) return generic;
    }
    return executable.wstring();
}

void applyGameEnvironment(const Layout& layout, const Settings& settings, bool diagnostics) {
    SetEnvironmentVariableW(L"DISNEY_RENDERER", settings.gpuRenderer ? L"gpu" : L"software");
    SetEnvironmentVariableW(L"DISNEY_NATIVE_VBLANK", settings.nativeVblank ? L"1" : L"0");
    SetEnvironmentVariableW(L"PS2X_GS_HW_SCALE", std::to_wstring(settings.renderScale).c_str());
    SetEnvironmentVariableW(L"PS2X_GS_DIRECT_PRESENT", L"0");
    // The runtime reads DISNEY_SAVE_ROOT with getenv (ANSI). Only override it
    // when the path is ASCII; otherwise its own default below the user profile
    // is safer than a mojibake path.
    if (hasAsciiOnly(layout.savesDir.wstring())) {
        SetEnvironmentVariableW(L"DISNEY_SAVE_ROOT", layout.savesDir.wstring().c_str());
    } else {
        SetEnvironmentVariableW(L"DISNEY_SAVE_ROOT", nullptr);
    }
    for (const wchar_t* name : {
             L"DISNEY_HANG_DIAGNOSTICS", L"DISNEY_HANG_OUTPUT", L"DISNEY_DIAG_MENU_INPUT",
             L"DISNEY_GPU_CAPTURE", L"DISNEY_GPU_VERIFY_SECONDS", L"DISNEY_AOT_TEST_FRAMES",
             L"DISNEY_PORT_DIAGNOSTICS", L"DISNEY_AOT_PROFILE", L"DISNEY_AOT_TRACE_VU",
             L"DISNEY_AOT_TRACE_CHAIN", L"DISNEY_AOT_TRACE_KICK", L"DISNEY_AOT_TRACE_PAD",
             L"DISNEY_AOT_HOST_CLOCK", L"PS2X_VU1_TRACE", L"PS2X_VU1_CAPTURE"}) {
        SetEnvironmentVariableW(name, nullptr);
    }
    if (settings.hangDiagnostics || diagnostics) {
        std::error_code ec;
        const fs::path reports = layout.logsDir / L"hang-reports";
        fs::create_directories(reports, ec);
        SetEnvironmentVariableW(L"DISNEY_HANG_DIAGNOSTICS", L"1");
        SetEnvironmentVariableW(L"DISNEY_HANG_OUTPUT", reports.wstring().c_str());
    }
}

struct ChildWindowTitle {
    DWORD processId = 0;
    bool changed = false;
};

BOOL CALLBACK setChildWindowTitle(HWND window, LPARAM data) {
    auto* child = reinterpret_cast<ChildWindowTitle*>(data);
    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    if (processId != child->processId || !IsWindowVisible(window)) return TRUE;
    SetWindowTextW(window, kWindowTitle);
    child->changed = true;
    return FALSE;
}

void nameGameWindow(DWORD processId) {
    for (unsigned attempt = 0; attempt < 150; ++attempt) {
        ChildWindowTitle child{processId};
        EnumWindows(setChildWindowTitle, reinterpret_cast<LPARAM>(&child));
        if (child.changed) return;
        Sleep(100);
    }
}

} // namespace

class App::Impl {
public:
    Impl() = default;
    ~Impl();
    friend class App;

    bool initialize(HINSTANCE instance, bool diagnosticsMode);
    int run();

    static LRESULT CALLBACK wndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    bool createWindow();
    void createFonts();
    void createLogEdit();
    void destroyGraphics();

    void loadState();
    void loadCover();
    void setPage(int page);
    void setNotice(const std::wstring& text, NoticeKind kind);
    void setUpdateModal(UpdateModalState state);
    void refreshPackage();
    bool canInstall() const;
    bool validateSelectedIso();
    bool saveSettings();
    void applyDraft();

    void paint(HDC target);
    void drawBackground(Gdiplus::Graphics& graphics, float width, float height);
    void drawHeader(Gdiplus::Graphics& graphics, float width);
    void drawNotice(Gdiplus::Graphics& graphics, float width);
    void drawHome(Gdiplus::Graphics& graphics, float width, float height, float top);
    void drawInstalling(Gdiplus::Graphics& graphics, float width, float height, float top);
    void drawReady(Gdiplus::Graphics& graphics, float width, float height, float top);
    void drawSettings(Gdiplus::Graphics& graphics, float width, float height, float top);
    void drawUpdates(Gdiplus::Graphics& graphics, float width, float height, float top);
    void drawLogs(Gdiplus::Graphics& graphics, float width, float height, float top);
    void drawUpdateModal(Gdiplus::Graphics& graphics, float width, float height);

    void drawCard(Gdiplus::Graphics& graphics, const RectF& rect);
    void drawButton(Gdiplus::Graphics& graphics, const RectF& rect, const std::wstring& label,
                    int style, bool enabled, int action, int value = 0);
    void drawPill(Gdiplus::Graphics& graphics, const RectF& rect, const std::wstring& label,
                  bool selected, int action, int value);
    void drawStatusDot(Gdiplus::Graphics& graphics, float x, float y, const Gdiplus::Color& color);
    void drawChecklistRow(Gdiplus::Graphics& graphics, float x, float y, float width,
                          const std::wstring& label, const std::wstring& detail,
                          const Gdiplus::Color& color);
    void drawStepIcon(Gdiplus::Graphics& graphics, float x, float y, StepState state,
                      int index);
    int addHit(const RectF& rect, int action, int value, bool enabled);
    int hitTest(POINT point) const;

    void onAction(int action, int value);
    void onMouseMove(POINT point);
    void onMouseLeave();
    void onMouseDown(POINT point);
    void onMouseUp(POINT point);

    void startInstall();
    void cancelInstall();
    void startUninstall();
    void startGame();
    void bringGameToFront();
    void startUpdateCheck(bool manual);
    void startDownload();
    bool openPath(const fs::path& path);
    void openControls();
    void refreshLogEdit();
    void layoutLogEdit();
    void copyDiagnostics();
    std::wstring diagnosticsText() const;

    void handleInstallEvent(std::unique_ptr<InstallEvent> event);
    void handleInstallDone(std::unique_ptr<InstallResult> result);
    void handleUninstallDone(std::unique_ptr<UninstallOutcome> outcome);
    void handleUpdateDone(std::unique_ptr<UpdateOutcome> outcome);
    void handleDownloadDone(std::unique_ptr<DownloadOutcome> outcome);
    void handleGameExited(DWORD exitCode);

    float S(float value) const { return value * scale_; }
    RectF R(float x, float y, float w, float h) const {
        return RectF(S(x), S(y), S(w), S(h));
    }

    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    HWND logEdit_ = nullptr;
    HFONT logEditFont_ = nullptr;
    HBRUSH logEditBrush_ = nullptr;
    bool diagnosticsMode_ = false;

    Layout layout_;
    Config config_;
    Settings settings_;
    Settings draft_;
    PackageReport report_;
    int page_ = PageHome;

    std::wstring notice_;
    NoticeKind noticeKind_ = NoticeInfo;
    std::wstring isoPath_;
    bool isoValid_ = false;
    std::wstring isoStatus_;
    std::wstring isoDetail_;

    std::array<StepState, 5> stepStates_{};
    std::array<std::wstring, 5> stepMessages_{};
    int overallPercent_ = 0;
    bool installing_ = false;
    bool installFinished_ = false;
    bool installSucceeded_ = false;
    std::shared_ptr<CancelToken> installCancel_;
    std::thread installThread_;
    std::deque<std::wstring> installLog_;

    bool gameRunning_ = false;
    DWORD gamePid_ = 0;

    bool uninstalling_ = false;
    bool externalGameRunning_ = false;
    std::wstring runtimeCheckError_;
    std::thread uninstallThread_;

    bool updateChecking_ = false;
    bool updateHasNew_ = false;
    UpdateModalState updateModal_ = UpdateModalNone;
    int updateSpinnerAngle_ = 0;
    ReleaseInfo release_;
    std::wstring updateStatus_;
    std::wstring updateError_;
    std::wstring downloadedInstaller_;
    bool downloading_ = false;
    int downloadPercent_ = 0;
    std::shared_ptr<CancelToken> downloadCancel_;

    std::unique_ptr<Gdiplus::Bitmap> cover_;
    IStream* coverStream_ = nullptr;
    FontSet fonts_;
    float scale_ = 1.0f;
    UINT dpi_ = 96;

    std::vector<HitTarget> hits_;
    int hoverId_ = 0;
    int pressedId_ = 0;
    bool trackingMouse_ = false;
    bool startupChecked_ = false;
    bool closing_ = false;
};

App::Impl::~Impl() {
    if (installCancel_) installCancel_->cancel();
    if (downloadCancel_) downloadCancel_->cancel();
    if (installThread_.joinable()) installThread_.join();
    if (uninstallThread_.joinable()) uninstallThread_.join();
    destroyGraphics();
    Logger::instance().close();
}

void App::Impl::destroyGraphics() {
    fonts_ = FontSet{};
    cover_.reset();
    if (coverStream_) {
        coverStream_->Release();
        coverStream_ = nullptr;
    }
    if (logEditFont_) {
        DeleteObject(logEditFont_);
        logEditFont_ = nullptr;
    }
    if (logEditBrush_) {
        DeleteObject(logEditBrush_);
        logEditBrush_ = nullptr;
    }
}

bool App::Impl::initialize(HINSTANCE instance, bool diagnosticsMode) {
    instance_ = instance;
    diagnosticsMode_ = diagnosticsMode;
    ui::initialize();

    const fs::path exe = thisExecutablePath();
    if (exe.empty()) return false;
    layout_ = Layout::detect(exe);

    std::wstring error;
    if (!layout_.ensureUserDirectories(error)) {
        MessageBoxW(nullptr, (L"Não foi possível preparar os dados do usuário:\n" + error).c_str(),
                    kWindowTitle, MB_OK | MB_ICONERROR);
        ui::shutdown();
        return false;
    }
    if (!Logger::instance().open(layout_.launcherLog(), error)) {
        MessageBoxW(nullptr, error.c_str(), kWindowTitle, MB_OK | MB_ICONWARNING);
    }
    logInfo(L"Launcher " DISNEY_LAUNCHER_VERSION_STRING L" iniciado");
    logInfo(layout_.describe());

    if (!config_.load(layout_.configPath, error)) {
        setNotice(L"Não foi possível ler as configurações: " + error, NoticeWarning);
    }
    settings_ = config_.settings();
    draft_ = settings_;
    isoPath_ = settings_.lastIsoPath;

    report_ = inspectPackageForLaunch(layout_);
    page_ = report_.ready ? PageReady : PageHome;
    if (page_ == PageHome) {
        logInfo(L"Pacote incompleto; exibindo a tela de instalação.");
    }

    if (!createWindow()) {
        ui::shutdown();
        return false;
    }
    createFonts();
    loadCover();
    createLogEdit();
    layoutLogEdit();

    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
    PostMessageW(hwnd_, WM_APP_STARTUP, 0, 0);
    return true;
}

bool App::Impl::createWindow() {
    static bool registered = false;
    HICON icon = LoadIconW(instance_, MAKEINTRESOURCEW(IDI_APP));
    if (!registered) {
        WNDCLASSEXW windowClass{};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.style = CS_HREDRAW | CS_VREDRAW;
        windowClass.lpfnWndProc = &Impl::wndProc;
        windowClass.hInstance = instance_;
        windowClass.hIcon = icon;
        windowClass.hIconSm = icon;
        windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        windowClass.hbrBackground = nullptr;
        windowClass.lpszClassName = kWindowClass;
        if (!RegisterClassExW(&windowClass)) return false;
        registered = true;
    }

    const HDC screen = GetDC(nullptr);
    dpi_ = static_cast<UINT>(GetDeviceCaps(screen, LOGPIXELSX));
    ReleaseDC(nullptr, screen);
    if (dpi_ == 0) dpi_ = 96;
    scale_ = static_cast<float>(dpi_) / 96.0f;

    RECT desired{0, 0, static_cast<LONG>(S(1040)), static_cast<LONG>(S(700))};
    AdjustWindowRectEx(&desired, WS_OVERLAPPEDWINDOW, FALSE, 0);
    const int width = desired.right - desired.left;
    const int height = desired.bottom - desired.top;

    hwnd_ = CreateWindowExW(0, kWindowClass, kWindowTitle, WS_OVERLAPPEDWINDOW,
                            CW_USEDEFAULT, CW_USEDEFAULT, width, height, nullptr, nullptr,
                            instance_, this);
    if (!hwnd_) return false;

    RECT windowRect{};
    GetWindowRect(hwnd_, &windowRect);
    MONITORINFO monitor{};
    monitor.cbSize = sizeof(monitor);
    GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTOPRIMARY), &monitor);
    const int x = monitor.rcWork.left +
                  ((monitor.rcWork.right - monitor.rcWork.left) -
                   (windowRect.right - windowRect.left)) /
                      2;
    const int y = monitor.rcWork.top +
                  ((monitor.rcWork.bottom - monitor.rcWork.top) -
                   (windowRect.bottom - windowRect.top)) /
                      2;
    SetWindowPos(hwnd_, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    return true;
}

void App::Impl::createFonts() {
    const wchar_t* family = L"Segoe UI";
    auto size = [this](float value) { return value * scale_; };
    fonts_.title = ui::createFont(family, size(26), true);
    fonts_.heading = ui::createFont(family, size(21), true);
    fonts_.subheading = ui::createFont(family, size(16), true);
    fonts_.body = ui::createFont(family, size(13.5f), false);
    fonts_.bodyBold = ui::createFont(family, size(13.5f), true);
    fonts_.smallText = ui::createFont(family, size(11), false);
    fonts_.smallBoldText = ui::createFont(family, size(11), true);
    fonts_.button = ui::createFont(family, size(14), true);
    fonts_.buttonSmall = ui::createFont(family, size(12), true);
    fonts_.mono = ui::createFont(L"Consolas", size(10.5f), false);
    fonts_.huge = ui::createFont(family, size(38), true);

    if (logEditFont_) DeleteObject(logEditFont_);
    logEditFont_ = CreateFontW(-static_cast<int>(size(11.5f)), 0, 0, 0, FW_NORMAL, FALSE, FALSE,
                               FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
    if (logEdit_) SendMessageW(logEdit_, WM_SETFONT, reinterpret_cast<WPARAM>(logEditFont_),
                               TRUE);
}

void App::Impl::createLogEdit() {
    logEdit_ = CreateWindowExW(
        0, L"EDIT", L"", WS_CHILD | ES_MULTILINE | ES_READONLY | WS_VSCROLL | ES_AUTOVSCROLL,
        0, 0, 10, 10, hwnd_, nullptr, instance_, nullptr);
    if (logEdit_) {
        SendMessageW(logEdit_, WM_SETFONT, reinterpret_cast<WPARAM>(logEditFont_), TRUE);
        ShowWindow(logEdit_, SW_HIDE);
    }
    logEditBrush_ = CreateSolidBrush(RGB(12, 18, 32));
}

void App::Impl::loadCover() {
    cover_ = ui::loadImageFromResource(instance_, IDR_COVER_IMAGE, &coverStream_);
    if (!cover_) {
        cover_ = ui::loadImageFromFile(layout_.exeDir / L"docs" / L"capa-jogo.png",
                                       &coverStream_);
    }
}

int App::Impl::run() {
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

LRESULT CALLBACK App::Impl::wndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    Impl* self = reinterpret_cast<Impl*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        self = reinterpret_cast<Impl*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    }
    if (!self) return DefWindowProcW(hwnd, message, wParam, lParam);
    return self->handleMessage(message, wParam, lParam);
}

LRESULT App::Impl::handleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_PAINT: {
        PAINTSTRUCT paintStruct{};
        HDC dc = BeginPaint(hwnd_, &paintStruct);
        paint(dc);
        EndPaint(hwnd_, &paintStruct);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_TIMER:
        if (wParam == kUpdateSpinnerTimer && updateModal_ == UpdateModalChecking) {
            updateSpinnerAngle_ = (updateSpinnerAngle_ + 30) % 360;
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        return 0;
    case WM_SIZE:
        layoutLogEdit();
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    case WM_DPICHANGED: {
        dpi_ = HIWORD(wParam);
        scale_ = static_cast<float>(dpi_) / 96.0f;
        const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
        SetWindowPos(hwnd_, nullptr, suggested->left, suggested->top,
                     suggested->right - suggested->left, suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        createFonts();
        layoutLogEdit();
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    }
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        info->ptMinTrackSize.x = static_cast<LONG>(S(980));
        info->ptMinTrackSize.y = static_cast<LONG>(S(660));
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (!trackingMouse_) {
            TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, hwnd_, 0};
            TrackMouseEvent(&track);
            trackingMouse_ = true;
        }
        onMouseMove({GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
        return 0;
    }
    case WM_MOUSELEAVE:
        trackingMouse_ = false;
        onMouseLeave();
        return 0;
    case WM_LBUTTONDOWN:
        onMouseDown({GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
        return 0;
    case WM_LBUTTONUP:
        onMouseUp({GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
        return 0;
    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT && hoverId_ != 0) {
            SetCursor(LoadCursorW(nullptr, IDC_HAND));
            return TRUE;
        }
        break;
    case WM_KEYDOWN:
        if (updateModal_ != UpdateModalNone) {
            if (updateModal_ == UpdateModalNoUpdates &&
                (wParam == VK_RETURN || wParam == VK_ESCAPE)) {
                onAction(ActCloseUpdateModal, 0);
            }
            return 0;
        }
        if (wParam == VK_RETURN) {
            if (page_ == PageHome && canInstall()) onAction(ActInstall, 0);
            else if (page_ == PageReady && !gameRunning_) onAction(ActPlay, 0);
            else if (page_ == PageSettings) onAction(ActSaveSettings, 0);
            else if (page_ == PageUpdates) onAction(ActCheckUpdates, 0);
        } else if (wParam == VK_ESCAPE) {
            if (page_ == PageSettings || page_ == PageUpdates || page_ == PageLogs)
                onAction(ActBack, 0);
            else if (page_ == PageInstalling && installFinished_)
                onAction(ActBackHome, 0);
        } else if (wParam == VK_F5) {
            if (page_ == PageLogs) refreshLogEdit();
            else refreshPackage();
        }
        return 0;
    case WM_CTLCOLORSTATIC:
        if (reinterpret_cast<HWND>(lParam) == logEdit_) {
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetTextColor(dc, RGB(208, 219, 241));
            SetBkColor(dc, RGB(12, 18, 32));
            return reinterpret_cast<LRESULT>(logEditBrush_);
        }
        break;
    case WM_APP_INSTALL_EVENT:
        handleInstallEvent(std::unique_ptr<InstallEvent>(
            reinterpret_cast<InstallEvent*>(lParam)));
        return 0;
    case WM_APP_INSTALL_DONE:
        handleInstallDone(std::unique_ptr<InstallResult>(
            reinterpret_cast<InstallResult*>(lParam)));
        return 0;
    case WM_APP_UNINSTALL_DONE:
        handleUninstallDone(std::unique_ptr<UninstallOutcome>(
            reinterpret_cast<UninstallOutcome*>(lParam)));
        return 0;
    case WM_APP_GAME_EXITED:
        handleGameExited(static_cast<DWORD>(wParam));
        return 0;
    case WM_APP_UPDATE_DONE:
        handleUpdateDone(std::unique_ptr<UpdateOutcome>(reinterpret_cast<UpdateOutcome*>(lParam)));
        return 0;
    case WM_APP_DOWNLOAD_PROGRESS:
        downloadPercent_ = static_cast<int>(wParam);
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    case WM_APP_DOWNLOAD_DONE:
        handleDownloadDone(std::unique_ptr<DownloadOutcome>(
            reinterpret_cast<DownloadOutcome*>(lParam)));
        return 0;
    case WM_APP_STARTUP:
        if (!startupChecked_) {
            startupChecked_ = true;
            if (settings_.checkUpdatesOnStart) startUpdateCheck(false);
        }
        return 0;
    case WM_CLOSE:
        if (installing_ && !installFinished_) {
            MessageBoxW(hwnd_, L"A instalação está em andamento. Cancele antes de fechar.",
                        kWindowTitle, MB_OK | MB_ICONINFORMATION);
            return 0;
        }
        if (uninstalling_) {
            MessageBoxW(hwnd_, L"A desinstalação está em andamento. Aguarde a conclusão.",
                        kWindowTitle, MB_OK | MB_ICONINFORMATION);
            return 0;
        }
        if (downloadCancel_) downloadCancel_->cancel();
        if (gameRunning_) {
            if (MessageBoxW(hwnd_,
                            L"O jogo continuará aberto. Deseja fechar o launcher?",
                            kWindowTitle, MB_YESNO | MB_ICONQUESTION) != IDYES) {
                return 0;
            }
        }
        closing_ = true;
        DestroyWindow(hwnd_);
        return 0;
    case WM_DESTROY:
        logInfo(L"Launcher encerrado.");
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd_, message, wParam, lParam);
}

void App::Impl::paint(HDC target) {
    RECT client{};
    GetClientRect(hwnd_, &client);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    if (width <= 0 || height <= 0) return;

    HDC memory = CreateCompatibleDC(target);
    HBITMAP bitmap = CreateCompatibleBitmap(target, width, height);
    if (!memory || !bitmap) {
        if (memory) DeleteDC(memory);
        if (bitmap) DeleteObject(bitmap);
        return;
    }
    HGDIOBJ previous = SelectObject(memory, bitmap);

    {
        Gdiplus::Graphics graphics(memory);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
        hits_.clear();

        const float logicalWidth = width / scale_;
        const float logicalHeight = height / scale_;
        drawBackground(graphics, logicalWidth, logicalHeight);
        drawHeader(graphics, logicalWidth);
        const float top = notice_.empty() ? 94.0f : 140.0f;
        drawNotice(graphics, logicalWidth);

        switch (page_) {
        case PageHome: drawHome(graphics, logicalWidth, logicalHeight, top); break;
        case PageInstalling: drawInstalling(graphics, logicalWidth, logicalHeight, top); break;
        case PageReady: drawReady(graphics, logicalWidth, logicalHeight, top); break;
        case PageSettings: drawSettings(graphics, logicalWidth, logicalHeight, top); break;
        case PageUpdates: drawUpdates(graphics, logicalWidth, logicalHeight, top); break;
        case PageLogs: drawLogs(graphics, logicalWidth, logicalHeight, top); break;
        default: break;
        }
        if (updateModal_ != UpdateModalNone) {
            drawUpdateModal(graphics, logicalWidth, logicalHeight);
        }
    }

    BitBlt(target, 0, 0, width, height, memory, 0, 0, SRCCOPY);
    SelectObject(memory, previous);
    DeleteObject(bitmap);
    DeleteDC(memory);
}

void App::Impl::drawBackground(Gdiplus::Graphics& graphics, float width, float height) {
    RectF bounds(0, 0, width, height);
    Gdiplus::LinearGradientBrush gradient(bounds, ui::color(kBgTop), ui::color(kBgBottom),
                                          Gdiplus::LinearGradientModeVertical);
    graphics.FillRectangle(&gradient, bounds);

    // Keep the background quiet: a single soft accent wash at the very top,
    // clipped by the window, avoids the hard rectangle a large ellipse can
    // produce behind the header.
    Gdiplus::SolidBrush wash(ui::color(kAccent, 10));
    graphics.FillRectangle(&wash, RectF(0, 0, width, 3));
}

void App::Impl::drawHeader(Gdiplus::Graphics& graphics, float width) {
    ui::fillRoundRect(graphics, R(26, 22, 4, 36), S(2), ui::color(kAccent));
    ui::drawText(graphics, L"Disney's Extreme Skate Adventure", *fonts_.title,
                 ui::color(kText), R(42, 16, width - 220, 38));
    ui::drawText(graphics, L"Recompilação nativa para Windows  ·  OpenGL 3.3  ·  x64",
                 *fonts_.smallText, ui::color(kMuted), R(43, 52, width - 220, 18));

    const RectF version = R(width - 112, 24, 86, 28);
    ui::fillRoundRect(graphics, version, S(8), ui::color(kBlue, 22));
    ui::strokeRoundRect(graphics, version, S(8), ui::color(kBlue, 90));
    ui::drawText(graphics, L"v" DISNEY_LAUNCHER_VERSION_STRING, *fonts_.smallBoldText,
                 ui::color(0xBFD6FF), version, ui::TextAlign::Center, false, true, 0, true);

    Gdiplus::Pen line(ui::color(kBorder, 150), 1.0f);
    graphics.DrawLine(&line, 26.0f, S(82), width - 26.0f, S(82));
}

void App::Impl::drawNotice(Gdiplus::Graphics& graphics, float width) {
    if (notice_.empty()) return;
    uint32_t accent = kBlue;
    if (noticeKind_ == NoticeSuccess) accent = kSuccess;
    else if (noticeKind_ == NoticeWarning) accent = kWarning;
    else if (noticeKind_ == NoticeError) accent = kDanger;
    const RectF rect = R(26, 92, width - 52, 38);
    ui::fillRoundRect(graphics, rect, S(9), ui::color(accent, 22));
    ui::strokeRoundRect(graphics, rect, S(9), ui::color(accent, 90));
    ui::fillEllipse(graphics, R(40, 104, 10, 10), ui::color(accent));
    ui::drawText(graphics, firstLine(notice_), *fonts_.smallText, ui::color(0xE8EEFF),
                 R(58, 102, width - 90, 20));
}

void App::Impl::drawCard(Gdiplus::Graphics& graphics, const RectF& rect) {
    ui::fillRoundRect(graphics, rect, S(16), ui::color(kCard, 246));
    ui::strokeRoundRect(graphics, rect, S(16), ui::color(kBorder, 210));
}

int App::Impl::addHit(const RectF& rect, int action, int value, bool enabled) {
    const int id = hitId(action, value);
    if (enabled) hits_.push_back({rect, id});
    return id;
}

void App::Impl::drawButton(Gdiplus::Graphics& graphics, const RectF& rect,
                           const std::wstring& label, int style, bool enabled, int action,
                           int value) {
    const int id = addHit(rect, action, value, enabled);
    const bool hot = enabled && hoverId_ == id;
    const bool down = enabled && pressedId_ == id;

    Gdiplus::Color fill;
    Gdiplus::Color border;
    Gdiplus::Color text;
    if (style == 0) { // primary
        fill = enabled ? ui::color(down ? 0xE87512 : hot ? kAccentLight : kAccent)
                       : ui::color(0x352C25);
        border = enabled ? ui::color(down ? 0xC9640C : kAccentLight, 160) : ui::color(0x4A3A2C);
        text = enabled ? ui::color(0x1B1004) : ui::color(0x8C7A66);
    } else if (style == 1) { // secondary
        fill = ui::color(hot ? 0x223052 : kCardSoft);
        border = ui::color(hot ? 0x3A4D75 : kBorder);
        text = enabled ? ui::color(0xE7EDFF) : ui::color(kFaint);
    } else if (style == 2) { // danger
        fill = ui::color(enabled ? (hot ? 0x4A222C : 0x361A22) : 0x241C28);
        border = ui::color(enabled ? 0x7A3040 : 0x40303E);
        text = ui::color(enabled ? 0xFFB9C2 : kFaint);
    } else { // ghost
        fill = ui::color(0, 0);
        border = ui::color(0, 0);
        text = enabled ? ui::color(kMuted) : ui::color(kFaint);
    }
    if (fill.GetA() > 0) {
        if (style == 0 && enabled) {
            RectF gradientRect = rect;
            if (down) gradientRect.Offset(0, S(1));
            Gdiplus::LinearGradientBrush gradient(gradientRect, ui::color(kAccentLight),
                                                  ui::color(down ? 0xE87512 : kAccent),
                                                  Gdiplus::LinearGradientModeVertical);
            Gdiplus::GraphicsPath path;
            const float radius = std::min({S(10), rect.Width / 2, rect.Height / 2});
            path.AddArc(rect.X, rect.Y, radius * 2, radius * 2, 180, 90);
            path.AddArc(rect.X + rect.Width - radius * 2, rect.Y, radius * 2, radius * 2, 270,
                        90);
            path.AddArc(rect.X + rect.Width - radius * 2, rect.Y + rect.Height - radius * 2,
                        radius * 2, radius * 2, 0, 90);
            path.AddArc(rect.X, rect.Y + rect.Height - radius * 2, radius * 2, radius * 2, 90,
                        90);
            path.CloseFigure();
            graphics.FillPath(&gradient, &path);
        } else {
            ui::fillRoundRect(graphics, rect, S(10), fill);
        }
    }
    if (border.GetA() > 0) ui::strokeRoundRect(graphics, rect, S(10), border);
    ui::drawText(graphics, label, fonts_.button, text, rect, ui::TextAlign::Center, false, true,
                 0, true);
}

void App::Impl::drawPill(Gdiplus::Graphics& graphics, const RectF& rect,
                         const std::wstring& label, bool selected, int action, int value) {
    const int id = addHit(rect, action, value, true);
    const bool hot = hoverId_ == id;
    if (selected) {
        ui::fillRoundRect(graphics, rect, S(8), ui::color(kAccent, 36));
        ui::strokeRoundRect(graphics, rect, S(8), ui::color(kAccent, 210));
    } else {
        ui::fillRoundRect(graphics, rect, S(8), ui::color(hot ? 0x1F2B47 : kCardSoft));
        ui::strokeRoundRect(graphics, rect, S(8), ui::color(kBorder));
    }
    ui::drawText(graphics, label, fonts_.buttonSmall,
                 ui::color(selected ? kAccentLight : hot ? 0xE7EDFF : kMuted), rect,
                 ui::TextAlign::Center, false, true, 0, true);
}

void App::Impl::drawStatusDot(Gdiplus::Graphics& graphics, float x, float y,
                              const Gdiplus::Color& dotColor) {
    ui::fillEllipse(graphics, R(x, y, 9, 9), dotColor);
}

void App::Impl::drawChecklistRow(Gdiplus::Graphics& graphics, float x, float y, float width,
                                 const std::wstring& label, const std::wstring& detail,
                                 const Gdiplus::Color& dotColor) {
    drawStatusDot(graphics, x, y + 4, dotColor);
    ui::drawText(graphics, label, fonts_.bodyBold, ui::color(0xE9EEFF),
                 R(x + 18, y - 2, width - 18, 20));
    ui::drawText(graphics, detail, *fonts_.smallText, ui::color(kMuted),
                 R(x + 18, y + 19, width - 18, 18));
}

void App::Impl::drawStepIcon(Gdiplus::Graphics& graphics, float x, float y, StepState state,
                             int index) {
    RectF circle = R(x, y, 24, 24);
    Gdiplus::Color fill = ui::color(0x1D2740);
    Gdiplus::Color border = ui::color(kBorder);
    Gdiplus::Color symbol = ui::color(kMuted);
    std::wstring glyph = std::to_wstring(index + 1);
    if (state == StepState::Running) {
        fill = ui::color(kAccent, 34);
        border = ui::color(kAccent, 200);
        symbol = ui::color(kAccentLight);
    } else if (state == StepState::Done) {
        fill = ui::color(kSuccess, 32);
        border = ui::color(kSuccess, 190);
        symbol = ui::color(kSuccess);
        glyph = L"\x2713";
    } else if (state == StepState::Warning) {
        fill = ui::color(kWarning, 32);
        border = ui::color(kWarning, 190);
        symbol = ui::color(kWarning);
        glyph = L"!";
    } else if (state == StepState::Failed) {
        fill = ui::color(kDanger, 32);
        border = ui::color(kDanger, 190);
        symbol = ui::color(kDanger);
        glyph = L"\x00d7";
    } else if (state == StepState::Skipped) {
        fill = ui::color(0x1A2438);
        border = ui::color(kBorder);
        symbol = ui::color(kFaint);
        glyph = L"\x2713";
    }
    ui::fillEllipse(graphics, circle, fill);
    Gdiplus::Pen pen(border, 1.0f);
    graphics.DrawEllipse(&pen, circle);
    ui::drawText(graphics, glyph, *fonts_.smallBoldText, symbol, circle, ui::TextAlign::Center,
                 false, true, 0, true);
}

void App::Impl::drawHome(Gdiplus::Graphics& graphics, float width, float height, float top) {
    const float cardHeight = height - top - 24;
    const float gap = 18;
    const float leftWidth = (width - 52) * 0.61f;
    const RectF left = R(26, top, leftWidth, cardHeight);
    const RectF right = R(26 + leftWidth + gap, top, width - 52 - leftWidth - gap, cardHeight);
    drawCard(graphics, left);
    drawCard(graphics, right);

    ui::drawText(graphics, L"Instalar o jogo", *fonts_.heading, ui::color(kText),
                 R(left.X / scale_ + 24, top + 20, left.Width / scale_ - 48, 30));
    ui::drawText(graphics,
                 L"Selecione a imagem do disco de PlayStation 2 (edição americana). "
                 L"Os arquivos são extraídos e preparados apenas neste computador.",
                 *fonts_.body, ui::color(kMuted),
                 R(left.X / scale_ + 24, top + 58, left.Width / scale_ - 48, 48), 
                 ui::TextAlign::Left, true);

    ui::drawText(graphics,
                 L"1  validar a ISO     2  extrair os dados     3  preparar os assets e o áudio",
                 *fonts_.smallText, ui::color(kFaint),
                 R(left.X / scale_ + 24, top + 112, left.Width / scale_ - 48, 20));

    const float fieldX = left.X / scale_ + 24;
    const float fieldY = top + 146;
    const float fieldW = left.Width / scale_ - 48;
    const RectF field = R(fieldX, fieldY, fieldW, 52);
    ui::fillRoundRect(graphics, field, S(9), ui::color(kField));
    ui::strokeRoundRect(graphics, field, S(9), ui::color(isoValid_ ? kSuccess : kBorder, 190));
    ui::drawText(graphics, L"IMAGEM DO DISCO", *fonts_.smallBoldText, ui::color(kFaint),
                 R(fieldX + 14, fieldY + 7, fieldW - 170, 16));
    const std::wstring pathText =
        isoPath_.empty() ? L"Nenhuma imagem selecionada"
                         : fs::path(isoPath_).filename().wstring();
    ui::drawText(graphics, pathText, fonts_.body,
                 ui::color(isoPath_.empty() ? kFaint : 0xE9EEFF),
                 R(fieldX + 14, fieldY + 24, fieldW - 176, 20));
    drawButton(graphics, R(fieldX + fieldW - 154, fieldY + 9, 140, 34), L"Selecionar ISO",
               1, true, ActSelectIso);

    std::wstring status = isoStatus_;
    Gdiplus::Color statusColor = ui::color(kMuted);
    if (status.empty()) {
        status = L"A imagem será verificada antes de qualquer extração.";
    } else if (isoValid_) {
        statusColor = ui::color(kSuccess);
    } else {
        statusColor = ui::color(kDanger);
    }
    ui::drawText(graphics, status, *fonts_.smallText, statusColor,
                 R(fieldX, fieldY + 60, fieldW, 22));
    const float buttonY = top + cardHeight - 70;
    drawButton(graphics, R(fieldX, buttonY, 210, 46), L"Instalar jogo", 0,
               canInstall(), ActInstall);
    drawButton(graphics, R(fieldX + 222, buttonY, 160, 46), L"Verificar pacote", 1, true,
               ActRefreshPackage);

    ui::drawText(graphics, L"Saiba mais em docs\\launcher.md", *fonts_.smallText,
                 ui::color(kFaint), R(fieldX, buttonY + 50, fieldW, 16));

    // Component status -------------------------------------------------------
    const float rightX = right.X / scale_ + 22;
    const float rightW = right.Width / scale_ - 44;
    ui::drawText(graphics, L"Componentes", *fonts_.subheading, ui::color(kText),
                 R(rightX, top + 20, rightW, 26));
    struct Row {
        const wchar_t* label;
    };
    static constexpr Row rows[] = {{L"Runtime do jogo"},
                                   {L"Dados originais"},
                                   {L"Módulos IOP"},
                                   {L"Assets nativos"},
                                   {L"Catálogo de áudio"}};
    float y = top + 62;
    for (const Row& row : rows) {
        const CheckItem* item = report_.find(row.label);
        std::wstring detail = item ? item->detail : L"não verificado";
        Gdiplus::Color dotColor = ui::color(kFaint);
        if (item) {
            if (item->status == CheckStatus::Ok) dotColor = ui::color(kSuccess);
            else if (item->status == CheckStatus::Warning) dotColor = ui::color(kWarning);
            else dotColor = ui::color(kDanger);
        }
        drawChecklistRow(graphics, rightX, y, rightW, row.label, detail, dotColor);
        y += 60;
    }
    drawButton(graphics, R(rightX, top + cardHeight - 58, rightW, 38), L"Verificar novamente",
               1, true, ActRefreshPackage);
}

void App::Impl::drawInstalling(Gdiplus::Graphics& graphics, float width, float height,
                               float top) {
    const float cardHeight = height - top - 24;
    const RectF card = R(26, top, width - 52, cardHeight);
    drawCard(graphics, card);
    const float x = 26 + 26;
    const float w = width - 52 - 52;

    ui::drawText(graphics, L"Preparando o jogo", *fonts_.heading, ui::color(kText),
                 R(x, top + 20, w - 120, 30));
    const std::wstring percentText = std::to_wstring(overallPercent_) + L"%";
    ui::drawText(graphics, percentText, fonts_.huge,
                 ui::color(installFinished_ && !installSucceeded_ ? kDanger : kAccentLight),
                 R(x, top + 10, w, 44), ui::TextAlign::Right);

    std::wstring status;
    for (int i = 0; i < 5; ++i) {
        if (stepStates_[i] == StepState::Running) status = stepMessages_[i];
    }
    if (status.empty()) {
        if (installFinished_ && installSucceeded_) status = L"Instalação concluída.";
        else if (installFinished_) status = L"A instalação parou. Veja os detalhes abaixo.";
        else status = L"Preparando os arquivos...";
    }
    ui::drawText(graphics, status, fonts_.body, ui::color(kMuted),
                 R(x, top + 56, w, 22), ui::TextAlign::Left, false, true);

    const float barY = top + 84;
    const RectF bar = R(x, barY, w, 10);
    ui::fillRoundRect(graphics, bar, S(5), ui::color(0x1B2640));
    if (overallPercent_ > 0) {
        RectF fill = bar;
        fill.Width = bar.Width * std::clamp(overallPercent_, 0, 100) / 100.0f;
        Gdiplus::Color fillColor = ui::color(kAccent);
        if (installFinished_ && installSucceeded_) fillColor = ui::color(kSuccess);
        else if (installFinished_) fillColor = ui::color(kDanger);
        if (fill.Width > S(2)) ui::fillRoundRect(graphics, fill, S(5), fillColor);
    }

    float stepY = top + 112;
    for (int i = 0; i < 5; ++i) {
        drawStepIcon(graphics, x, stepY, stepStates_[i], i);
        ui::drawText(graphics, installStepName(static_cast<InstallStep>(i)), fonts_.bodyBold,
                     ui::color(stepStates_[i] == StepState::Pending ? kMuted : 0xE9EEFF),
                     R(x + 36, stepY - 1, w - 36, 19));
        ui::drawText(graphics, stepMessages_[i], *fonts_.smallText, ui::color(kMuted),
                     R(x + 36, stepY + 19, w - 36, 17));
        stepY += 46;
    }

    const float buttonsY = top + cardHeight - 60;
    const float logY = buttonsY - 108;
    const RectF logRect = R(x, logY, w, 92);
    ui::fillRoundRect(graphics, logRect, S(9), ui::color(0x0B1220));
    ui::strokeRoundRect(graphics, logRect, S(9), ui::color(kBorder, 170));
    float lineY = logY + 9;
    const int firstLine = static_cast<int>(installLog_.size()) -
                          std::min<size_t>(installLog_.size(), 5);
    for (size_t i = static_cast<size_t>(std::max(0, firstLine)); i < installLog_.size(); ++i) {
        ui::drawText(graphics, installLog_[i], fonts_.mono, ui::color(0x9FB0D6),
                     R(x + 12, lineY, w - 24, 15));
        lineY += 15;
    }
    if (installLog_.empty()) {
        ui::drawText(graphics, L"Os detalhes da preparação aparecem aqui e em logs\\preparation.",
                     fonts_.mono, ui::color(kFaint), R(x + 12, lineY, w - 24, 15));
    }

    if (!installFinished_) {
        drawButton(graphics, R(x + w - 160, buttonsY, 160, 42), L"Cancelar", 2, true,
                   ActCancelInstall);
    } else {
        drawButton(graphics, R(x + w - 160, buttonsY, 160, 42), L"Voltar", 1, true,
                   ActBackHome);
        if (!installSucceeded_) {
            drawButton(graphics, R(x + w - 336, buttonsY, 160, 42), L"Tentar novamente", 0,
                       canInstall(), ActRetryInstall);
        } else {
            drawButton(graphics, R(x + w - 336, buttonsY, 160, 42), L"Jogar", 0,
                       report_.ready && !gameRunning_, ActPlay);
        }
    }
}

void App::Impl::drawReady(Gdiplus::Graphics& graphics, float width, float height, float top) {
    const float cardHeight = height - top - 24;
    const float leftWidth = 306;
    const RectF left = R(26, top, leftWidth, cardHeight);
    const RectF right = R(26 + leftWidth + 18, top, width - 52 - leftWidth - 18, cardHeight);
    drawCard(graphics, left);
    drawCard(graphics, right);

    const float coverX = 26 + 22;
    const float coverY = top + 22;
    const float coverW = leftWidth - 44;
    const float coverH = cardHeight - 148;
    const RectF coverBounds = R(coverX, coverY, coverW, coverH);
    if (cover_) {
        const RectF fitted = ui::fitInto(coverBounds, 315.0f / 451.0f);
        Gdiplus::SolidBrush shadow(ui::color(0x000000, 90));
        graphics.FillRectangle(&shadow, RectF(fitted.X + S(4), fitted.Y + S(6), fitted.Width,
                                              fitted.Height));
        graphics.DrawImage(cover_.get(), fitted);
        ui::strokeRoundRect(graphics, fitted, S(4), ui::color(kBorder, 200));
    } else {
        ui::fillRoundRect(graphics, coverBounds, S(10), ui::color(kCardSoft));
        ui::drawText(graphics, L"Disney's Extreme\nSkate Adventure", *fonts_.subheading,
                     ui::color(kMuted), coverBounds, ui::TextAlign::Center, true);
    }

    drawButton(graphics, R(coverX, top + cardHeight - 108, coverW, 62),
               gameRunning_ ? L"JOGO ABERTO" : L"\u25B6  JOGAR", 0,
               report_.ready && !gameRunning_, ActPlay);
    if (gameRunning_) {
        drawButton(graphics, R(coverX, top + cardHeight - 40, coverW, 28), L"Trazer para frente",
                   1, true, ActBringGame);
    } else {
        ui::drawText(graphics, L"Salve e feche o jogo para voltar ao launcher.",
                     fonts_.smallText, ui::color(kFaint),
                     R(coverX, top + cardHeight - 40, coverW, 16), ui::TextAlign::Center);
    }

    const float rx = right.X / scale_ + 26;
    const float rw = right.Width / scale_ - 52;
    ui::drawText(graphics, L"Pronto para jogar", *fonts_.heading, ui::color(kText),
                 R(rx, top + 22, rw, 30));
    const RectF pill = R(rx + rw - 150, top + 27, 150, 26);
    ui::fillRoundRect(graphics, pill, S(13), ui::color(report_.ready ? kSuccess : kWarning, 28));
    ui::strokeRoundRect(graphics, pill, S(13), ui::color(report_.ready ? kSuccess : kWarning, 120));
    drawStatusDot(graphics, pill.X / scale_ + 12, top + 36,
                  ui::color(report_.ready ? kSuccess : kWarning));
    ui::drawText(graphics, report_.ready ? L"Pacote pronto" : L"Revisar pacote",
                 fonts_.smallBoldText, ui::color(0xDDE7FF), R(pill.X / scale_ + 26, top + 30, 116, 20));

    if (gameRunning_) {
        ui::drawText(graphics,
                     L"O jogo está em execução. Feche a janela do jogo para voltar a esta tela.",
                     fonts_.body, ui::color(kAccentLight), R(rx, top + 60, rw, 22));
    } else {
        ui::drawText(graphics, L"Versão do launcher " DISNEY_LAUNCHER_VERSION_STRING
                     L"  ·  " + report_.runtimeStamp,
                     fonts_.smallText, ui::color(kMuted), R(rx, top + 60, rw, 18));
    }

    float rowY = top + 96;
    for (const wchar_t* label : {L"Runtime do jogo", L"Dados originais", L"Assets nativos",
                                 L"Catálogo de áudio"}) {
        const CheckItem* item = report_.find(label);
        if (!item) continue;
        Gdiplus::Color dotColor = item->status == CheckStatus::Ok
                                      ? ui::color(kSuccess)
                                      : item->status == CheckStatus::Warning ? ui::color(kWarning)
                                                                             : ui::color(kDanger);
        drawChecklistRow(graphics, rx, rowY, rw, label, item->detail, dotColor);
        rowY += 48;
    }

    if (report_.warningCount > 0) {
        ui::drawText(graphics,
                     std::to_wstring(report_.warningCount) +
                         L" aviso(s) de mídia. O jogo pode abrir com áudio ou vídeo incompletos.",
                     fonts_.smallText, ui::color(kWarning), R(rx, rowY + 2, rw, 18));
    }

    const float buttonsY = top + cardHeight - 112;
    const float buttonW = (rw - 16) / 2;
    drawButton(graphics, R(rx, buttonsY, buttonW, 42), L"Configurações", 1, true, ActSettings);
    drawButton(graphics, R(rx + buttonW + 16, buttonsY, buttonW, 42), L"Atualizações", 1, true,
               ActUpdates);
    drawButton(graphics, R(rx, buttonsY + 50, buttonW, 42), L"Logs e diagnóstico", 1, true,
               ActLogs);
    drawButton(graphics, R(rx + buttonW + 16, buttonsY + 50, buttonW, 42), L"Abrir pasta do jogo",
               1, true, ActOpenGameFolder);

    ui::drawText(graphics,
                 L"Os saves ficam em " + relativeDisplayPath(layout_.savesDir, layout_.dataRoot),
                 fonts_.smallText, ui::color(kFaint), R(rx, buttonsY + 98, rw, 16));
    ui::drawText(graphics,
                 L"Projeto não oficial, sem vínculo com os titulares. "
                 L"A ISO e os dados do jogo não acompanham o launcher.",
                 fonts_.smallText, ui::color(kFaint), R(rx, top + 330, rw, 38),
                 ui::TextAlign::Left, true);
}

void App::Impl::drawSettings(Gdiplus::Graphics& graphics, float width, float height,
                             float top) {
    const float cardHeight = height - top - 24;
    const RectF card = R(26, top, width - 52, cardHeight);
    drawCard(graphics, card);
    const float x = 26 + 26;
    const float w = width - 52 - 52;
    ui::drawText(graphics, L"Configurações", *fonts_.heading, ui::color(kText),
                 R(x, top + 20, w, 30));
    drawButton(graphics, R(x + w - 110, top + 18, 110, 34), L"Voltar", 1, !uninstalling_,
               ActBack);

    const float leftWidth = (w - 24) * 0.57f;
    const float rightX = x + leftWidth + 24;
    const float rightW = w - leftWidth - 24;
    float y = top + 68;
    auto optionRow = [&](const std::wstring& label, const std::wstring& hint,
                         const std::function<void(float)>& pills) {
        ui::drawText(graphics, label, fonts_.bodyBold, ui::color(0xE9EEFF), R(x, y, leftWidth, 20));
        ui::drawText(graphics, hint, *fonts_.smallText, ui::color(kFaint),
                     R(x, y + 19, leftWidth, 16));
        pills(y + 34);
        y += 66;
    };

    optionRow(L"Qualidade de renderização", L"Aumenta a resolução interna do GS; 1x é o mais leve.",
              [&](float pillY) {
                  drawPill(graphics, R(x, pillY, 86, 30), L"1x", draft_.renderScale == 1,
                           ActScale1, 0);
                  drawPill(graphics, R(x + 94, pillY, 86, 30), L"2x", draft_.renderScale == 2,
                           ActScale2, 0);
                  drawPill(graphics, R(x + 188, pillY, 86, 30), L"3x", draft_.renderScale == 3,
                           ActScale3, 0);
              });
    optionRow(L"Renderizador", L"GPU usa OpenGL 3.3; software é um fallback mais lento.",
              [&](float pillY) {
                  drawPill(graphics, R(x, pillY, 110, 30), L"GPU", draft_.gpuRenderer, ActGpu,
                           0);
                  drawPill(graphics, R(x + 118, pillY, 110, 30), L"Software",
                           !draft_.gpuRenderer, ActSoftware, 0);
              });
    optionRow(L"V-Sync nativo", L"Alinha o ritmo do jogo ao refresh do monitor.",
              [&](float pillY) {
                  drawPill(graphics, R(x, pillY, 110, 30), L"Ligado", draft_.nativeVblank,
                           ActVblankOn, 0);
                  drawPill(graphics, R(x + 118, pillY, 110, 30), L"Desligado",
                           !draft_.nativeVblank, ActVblankOff, 0);
              });
    optionRow(L"Diagnóstico de travamentos", L"Grava relatórios em logs\\hang-reports quando a GPU para.",
              [&](float pillY) {
                  drawPill(graphics, R(x, pillY, 110, 30), L"Ligado", draft_.hangDiagnostics,
                           ActDiagOn, 0);
                  drawPill(graphics, R(x + 118, pillY, 110, 30), L"Desligado",
                           !draft_.hangDiagnostics, ActDiagOff, 0);
              });
    optionRow(L"Verificar atualizações", L"Consulta o GitHub ao abrir o launcher.",
              [&](float pillY) {
                  drawPill(graphics, R(x, pillY, 130, 30), L"Ao abrir", draft_.checkUpdatesOnStart,
                           ActUpdatesOn, 0);
                  drawPill(graphics, R(x + 138, pillY, 130, 30), L"Manual",
                           !draft_.checkUpdatesOnStart, ActUpdatesOff, 0);
              });

    const bool uninstallEnabled = !gameRunning_ && !installing_ && !uninstalling_ &&
                                  !externalGameRunning_;
    drawButton(graphics, R(x, y + 8, 190, 36), L"Desinstalar jogo", 2, uninstallEnabled,
               ActUninstall);
    std::wstring uninstallHint = L"Remove dados e assets; saves, configuração, logs e runtime são mantidos.";
    if (uninstalling_) uninstallHint = L"Aguarde a conclusão da desinstalação.";
    else if (externalGameRunning_) {
        uninstallHint = runtimeCheckError_.empty()
                            ? L"Feche o jogo antes de desinstalar."
                            : firstLine(runtimeCheckError_);
    }
    ui::drawText(graphics, uninstallHint, *fonts_.smallText, ui::color(kFaint),
                 R(x + 202, y + 10, leftWidth - 202, 34), ui::TextAlign::Left, true);

    ui::drawText(graphics, L"Pastas", *fonts_.subheading, ui::color(kText),
                 R(rightX, top + 68, rightW, 24));
    auto pathRow = [&](const std::wstring& label, const fs::path& path, int action, float rowY) {
        ui::drawText(graphics, label, *fonts_.smallBoldText, ui::color(kMuted),
                     R(rightX, rowY, rightW, 16));
        ui::drawText(graphics, relativeDisplayPath(path, layout_.dataRoot), *fonts_.smallText,
                     ui::color(0xD5DEF5), R(rightX, rowY + 17, rightW - 80, 18));
        drawButton(graphics, R(rightX + rightW - 72, rowY + 8, 72, 28), L"Abrir", 1, true,
                   action);
    };
    pathRow(L"Dados do jogo", layout_.gameDir, ActOpenGameFolder, top + 100);
    pathRow(L"Saves", layout_.savesDir, ActOpenSaves, top + 142);
    pathRow(L"Logs", layout_.logsDir, ActOpenLogsFolder, top + 184);

    ui::drawText(graphics, L"Controles", *fonts_.subheading, ui::color(kText),
                 R(rightX, top + 226, rightW, 24));
    ui::drawText(graphics,
                 L"Teclado: WASD/setas, Espaço/X, C, Enter, Tab, Q/E.\n"
                 L"Controle Xbox: botões por posição, com zona morta de 18%.",
                 fonts_.smallText, ui::color(kMuted), R(rightX, top + 256, rightW, 40),
                 ui::TextAlign::Left, true);
    drawButton(graphics, R(rightX, top + 302, rightW, 36), L"Abrir guia de controles", 1, true,
               ActOpenControls);
    ui::drawText(graphics,
                 L"As alterações valem na próxima vez que o jogo for aberto.\n"
                 L"Configuração: " + relativeDisplayPath(layout_.configPath, layout_.dataRoot),
                 fonts_.smallText, ui::color(kFaint), R(rightX, top + 348, rightW, 36),
                 ui::TextAlign::Left, true);

    const float buttonsY = top + cardHeight - 58;
    drawButton(graphics, R(x + w - 130, buttonsY, 130, 40), L"Salvar", 0, !uninstalling_,
               ActSaveSettings);
    drawButton(graphics, R(x + w - 272, buttonsY, 130, 40), L"Cancelar", 1, !uninstalling_,
               ActCancelSettings);
    if (draft_.renderScale != settings_.renderScale ||
        draft_.gpuRenderer != settings_.gpuRenderer ||
        draft_.nativeVblank != settings_.nativeVblank ||
        draft_.hangDiagnostics != settings_.hangDiagnostics ||
        draft_.checkUpdatesOnStart != settings_.checkUpdatesOnStart) {
        ui::drawText(graphics, L"Alterações não salvas", *fonts_.smallText, ui::color(kWarning),
                     R(x, buttonsY + 10, w - 300, 20));
    }
}

void App::Impl::drawUpdates(Gdiplus::Graphics& graphics, float width, float height, float top) {
    const float cardHeight = height - top - 24;
    const RectF card = R(26, top, width - 52, cardHeight);
    drawCard(graphics, card);
    const float x = 26 + 26;
    const float w = width - 52 - 52;
    ui::drawText(graphics, L"Atualizações", *fonts_.heading, ui::color(kText),
                 R(x, top + 20, w, 30));
    drawButton(graphics, R(x + w - 110, top + 18, 110, 34), L"Voltar", 1, true, ActBack);

    ui::drawText(graphics, L"Versão instalada: " DISNEY_LAUNCHER_VERSION_STRING, fonts_.bodyBold,
                 ui::color(kText), R(x, top + 68, w, 20));
    if (!updateStatus_.empty()) {
        ui::drawText(graphics, updateStatus_, fonts_.body, ui::color(kMuted),
                     R(x, top + 94, w, 44), ui::TextAlign::Left, true);
    } else {
        ui::drawText(graphics, L"Verifique se existe uma versão mais nova do launcher e do port.",
                     fonts_.body, ui::color(kMuted), R(x, top + 94, w, 22));
    }

    if (!release_.tag.empty()) {
        const RectF box = R(x, top + 146, w, 196);
        ui::fillRoundRect(graphics, box, S(10), ui::color(0x0C1322));
        ui::strokeRoundRect(graphics, box, S(10), ui::color(kBorder, 180));
        ui::drawText(graphics, release_.tag + (release_.title.empty() ? L"" : L"  ·  " + release_.title),
                     fonts_.subheading, ui::color(updateHasNew_ ? kAccentLight : kSuccess),
                     R(x + 16, top + 160, w - 32, 24));
        std::wstring notes = release_.notes;
        if (notes.size() > 1400) notes = notes.substr(0, 1400) + L"...";
        ui::drawText(graphics, notes, *fonts_.smallText, ui::color(0xC8D3EC),
                     R(x + 16, top + 190, w - 32, 140), ui::TextAlign::Left, true, true, 9);
    }

    const float buttonsY = top + cardHeight - 58;
    drawButton(graphics, R(x, buttonsY, 170, 40),
               updateChecking_ ? L"Verificando..." : L"Verificar novamente", 1,
               !updateChecking_, ActCheckUpdates);
    if (!release_.htmlUrl.empty()) {
        drawButton(graphics, R(x + 182, buttonsY, 170, 40), L"Abrir página da versão", 1, true,
                   ActOpenRelease);
    }
    if (updateHasNew_) {
        const ReleaseAsset* asset = release_.findAssetWithSuffix(L".exe");
        if (asset) {
            drawButton(graphics, R(x + w - 210, buttonsY, 210, 40),
                       downloading_ ? L"Baixando..." : L"Baixar instalador", 0, !downloading_,
                       ActDownloadInstaller);
        } else {
            ui::drawText(graphics, L"Esta versão não publica um instalador .exe.",
                         fonts_.smallText, ui::color(kFaint), R(x + w - 300, buttonsY + 10, 290, 20),
                         ui::TextAlign::Right);
        }
    }
    if (downloading_) {
        const RectF bar = R(x, buttonsY - 20, w - 230, 8);
        ui::fillRoundRect(graphics, bar, S(4), ui::color(0x1B2640));
        RectF fill = bar;
        fill.Width = bar.Width * std::clamp(downloadPercent_, 0, 100) / 100.0f;
        if (fill.Width > S(2)) ui::fillRoundRect(graphics, fill, S(4), ui::color(kAccent));
    }
    if (!downloadedInstaller_.empty()) {
        ui::drawText(graphics, L"Instalador baixado: " + downloadedInstaller_, *fonts_.smallText,
                     ui::color(kFaint), R(x, buttonsY - 44, w, 18));
    }
    ui::drawText(graphics,
                 L"A verificação consulta apenas metadados públicos do GitHub; "
                 L"nenhum dado do jogo é enviado.",
                 *fonts_.smallText, ui::color(kFaint), R(x, top + cardHeight - 92, w, 18));
}

void App::Impl::drawUpdateModal(Gdiplus::Graphics& graphics, float width, float height) {
    if (updateModal_ == UpdateModalNone) return;
    hits_.clear();

    Gdiplus::SolidBrush overlay(ui::color(0x03060D, 180));
    graphics.FillRectangle(&overlay, RectF(0, 0, width, height));

    const bool checking = updateModal_ == UpdateModalChecking;
    const float cardWidth = 430;
    const float cardHeight = checking ? 220 : 280;
    const float cardX = (width - cardWidth) / 2;
    const float cardY = (height - cardHeight) / 2;
    const RectF card = R(cardX, cardY, cardWidth, cardHeight);
    drawCard(graphics, card);

    if (checking) {
        const RectF spinner = R(cardX + 191, cardY + 30, 48, 48);
        Gdiplus::Pen track(ui::color(kBorder), S(4));
        Gdiplus::Pen arc(ui::color(kAccentLight), S(4));
        graphics.DrawEllipse(&track, spinner);
        graphics.DrawArc(&arc, spinner, static_cast<float>(updateSpinnerAngle_), 250);
        ui::drawText(graphics, L"Verificando atualizações", fonts_.heading, ui::color(kText),
                     R(cardX + 24, cardY + 94, cardWidth - 48, 30), ui::TextAlign::Center);
        ui::drawText(graphics, L"Consultando a versão mais recente no GitHub...", fonts_.body,
                     ui::color(kMuted), R(cardX + 24, cardY + 132, cardWidth - 48, 44),
                     ui::TextAlign::Center, true);
        return;
    }

    const RectF status = R(cardX + 193, cardY + 28, 44, 44);
    ui::fillEllipse(graphics, status, ui::color(kSuccess, 38));
    Gdiplus::Pen statusBorder(ui::color(kSuccess), S(1.5f));
    graphics.DrawEllipse(&statusBorder, status);
    ui::drawText(graphics, L"\x2713", fonts_.subheading, ui::color(kSuccess), status,
                 ui::TextAlign::Center);
    ui::drawText(graphics, L"Nenhuma atualização encontrada", fonts_.heading, ui::color(kText),
                 R(cardX + 24, cardY + 90, cardWidth - 48, 30), ui::TextAlign::Center);
    const std::wstring detail = release_.tag.empty()
        ? L"Ainda não há versões publicadas no GitHub. Esta é a build local "
          DISNEY_LAUNCHER_VERSION_STRING L"."
        : L"A versão instalada é a " DISNEY_LAUNCHER_VERSION_STRING
          L", a mais recente disponível.";
    ui::drawText(graphics, detail, fonts_.body, ui::color(kMuted),
                 R(cardX + 32, cardY + 128, cardWidth - 64, 52), ui::TextAlign::Center, true);
    drawButton(graphics, R(cardX + 145, cardY + 214, 140, 42), L"Entendi", 0, true,
               ActCloseUpdateModal);
}

void App::Impl::drawLogs(Gdiplus::Graphics& graphics, float width, float height, float top) {
    const float cardHeight = height - top - 24;
    const RectF card = R(26, top, width - 52, cardHeight);
    drawCard(graphics, card);
    const float x = 26 + 26;
    const float w = width - 52 - 52;
    ui::drawText(graphics, L"Logs e diagnóstico", *fonts_.heading, ui::color(kText),
                 R(x, top + 20, w - 500, 30));
    drawButton(graphics, R(x + w - 110, top + 18, 110, 34), L"Voltar", 1, true, ActBack);
    drawButton(graphics, R(x + w - 226, top + 18, 108, 34), L"Copiar", 1, true,
               ActCopyDiagnostics);
    drawButton(graphics, R(x + w - 342, top + 18, 108, 34), L"Pasta", 1, true, ActOpenLogsFolder);
    drawButton(graphics, R(x + w - 458, top + 18, 108, 34), L"Atualizar", 1, true,
               ActRefreshLogs);
    ui::drawText(graphics,
                 L"analysis\\launcher.log, analysis\\game-session.log e relatórios de travamento.",
                 fonts_.smallText, ui::color(kFaint), R(x, top + 54, w, 16));
}

int App::Impl::hitTest(POINT point) const {
    for (auto it = hits_.rbegin(); it != hits_.rend(); ++it) {
        if (point.x >= it->rect.X && point.x <= it->rect.X + it->rect.Width &&
            point.y >= it->rect.Y && point.y <= it->rect.Y + it->rect.Height) {
            return it->id;
        }
    }
    return 0;
}

void App::Impl::onMouseMove(POINT point) {
    const int id = hitTest(point);
    if (id != hoverId_) {
        hoverId_ = id;
        SetCursor(LoadCursorW(nullptr, id ? IDC_HAND : IDC_ARROW));
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void App::Impl::onMouseLeave() {
    if (hoverId_ != 0) {
        hoverId_ = 0;
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void App::Impl::onMouseDown(POINT point) {
    pressedId_ = hitTest(point);
    if (pressedId_) SetCapture(hwnd_);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void App::Impl::onMouseUp(POINT point) {
    const int released = hitTest(point);
    const int pressed = pressedId_;
    pressedId_ = 0;
    if (GetCapture() == hwnd_) ReleaseCapture();
    if (pressed && pressed == released) onAction(pressed / 16, pressed % 16);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void App::Impl::setPage(int page) {
    page_ = page;
    if (page_ == PageLogs) {
        if (logEdit_) ShowWindow(logEdit_, SW_SHOW);
        refreshLogEdit();
        layoutLogEdit();
    } else if (logEdit_) {
        ShowWindow(logEdit_, SW_HIDE);
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void App::Impl::setNotice(const std::wstring& text, NoticeKind kind) {
    notice_ = text;
    noticeKind_ = kind;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void App::Impl::setUpdateModal(UpdateModalState state) {
    updateModal_ = state;
    hoverId_ = 0;
    pressedId_ = 0;
    if (GetCapture() == hwnd_) ReleaseCapture();
    KillTimer(hwnd_, kUpdateSpinnerTimer);
    if (state == UpdateModalChecking) {
        updateSpinnerAngle_ = 0;
        SetTimer(hwnd_, kUpdateSpinnerTimer, 60, nullptr);
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void App::Impl::refreshPackage() {
    if (uninstalling_) return;
    report_ = inspectPackageForLaunch(layout_);
    if (report_.ready) {
        setNotice(L"Pacote pronto: o jogo está pronto para abrir.", NoticeSuccess);
        if (page_ == PageHome) setPage(PageReady);
    } else {
        std::wstring message = L"O pacote ainda está incompleto.";
        for (const CheckItem& item : report_.items) {
            if (item.status == CheckStatus::Error) {
                message = item.label + L": " + item.detail;
                break;
            }
        }
        setNotice(message, NoticeError);
        if (page_ == PageReady) setPage(PageHome);
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
}

bool App::Impl::canInstall() const {
    return !isoPath_.empty() && !installing_ && !uninstalling_;
}

bool App::Impl::validateSelectedIso() {
    isoValid_ = false;
    isoDetail_.clear();
    isoStatus_.clear();
    if (isoPath_.empty()) return false;
    if (!fileExists(isoPath_)) {
        isoStatus_ = L"A imagem selecionada não existe mais.";
        if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
        return false;
    }
    IsoImage image;
    std::wstring error;
    if (!image.open(isoPath_, error)) {
        isoStatus_ = firstLine(error);
        if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
        return false;
    }
    IsoValidation validation;
    if (!validateIso(image, validation)) {
        isoStatus_ = validation.errors.empty() ? L"Imagem não reconhecida."
                                               : validation.errors.front();
        if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
        return false;
    }
    isoValid_ = true;
    isoDetail_ = validation.summary();
    isoStatus_ = L"ISO validada: " + validation.serial +
                 (validation.version.empty() ? L"" : L" (versão " + validation.version + L")");
    logInfo(L"ISO selecionada e validada: " + isoPath_ + L" | " + validation.serial);
    InvalidateRect(hwnd_, nullptr, FALSE);
    return true;
}

void App::Impl::startInstall() {
    if (installing_ || uninstalling_) return;
    if (isoPath_.empty() || (!isoValid_ && !validateSelectedIso())) {
        setNotice(L"Selecione e valide uma imagem de disco NTSC-U antes de instalar.",
                  NoticeWarning);
        return;
    }
    std::wstring error;
    if (!layout_.ensureUserDirectories(error)) {
        setNotice(error, NoticeError);
        return;
    }
    if (!saveSettings()) return;

    installing_ = true;
    installFinished_ = false;
    installSucceeded_ = false;
    overallPercent_ = 0;
    installLog_.clear();
    for (int i = 0; i < 5; ++i) {
        stepStates_[i] = StepState::Pending;
        stepMessages_[i].clear();
    }
    installCancel_ = std::make_shared<CancelToken>();
    setPage(PageInstalling);
    setNotice(L"", NoticeInfo);

    const Layout layout = layout_;
    Settings settings = settings_;
    const fs::path iso = isoPath_;
    const HWND hwnd = hwnd_;
    auto cancel = installCancel_;
    try {
        installThread_ = std::thread([layout, settings, iso, hwnd, cancel]() mutable {
            InstallOptions options;
            options.isoPath = iso;
            auto post = [hwnd](std::unique_ptr<InstallEvent> event) {
                PostMessageW(hwnd, WM_APP_INSTALL_EVENT, 0,
                             reinterpret_cast<LPARAM>(event.release()));
            };
            InstallResult result;
            try {
                result = performInstall(
                    layout, settings, options, *cancel,
                    [&](const InstallEvent& event) {
                        post(std::make_unique<InstallEvent>(event));
                    });
            } catch (const std::exception& exception) {
                result.ok = false;
                result.message = L"Falha inesperada na instalação: " +
                                 toWide(exception.what());
            } catch (...) {
                result.ok = false;
                result.message = L"Falha inesperada na instalação.";
            }
            PostMessageW(hwnd, WM_APP_INSTALL_DONE, 0,
                         reinterpret_cast<LPARAM>(new InstallResult(std::move(result))));
        });
    } catch (const std::exception& exception) {
        installing_ = false;
        installFinished_ = true;
        setNotice(L"Não foi possível iniciar a instalação: " + toWide(exception.what()),
                  NoticeError);
    }
}

void App::Impl::cancelInstall() {
    if (installCancel_) installCancel_->cancel();
}

void App::Impl::startUninstall() {
    if (uninstalling_ || installing_ || gameRunning_) return;

    std::wstring processError;
    externalGameRunning_ = runtimeProcessRunning(layout_, processError);
    runtimeCheckError_ = processError;
    if (externalGameRunning_) {
        setNotice(processError.empty() ? L"Feche o jogo antes de desinstalar." : processError,
                  NoticeWarning);
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }

    const std::wstring confirmation =
        L"Desinstalar os dados e os assets gerados do jogo?\n\n"
        L"Serão removidos:\n" +
        layout_.gameDir.wstring() + L"\n" + layout_.assetsDir.wstring() +
        L"\n\nSerão mantidos: launcher, runtime, saves, configuração e logs.\n"
        L"Para jogar novamente, será necessário selecionar e instalar a ISO.";
    if (MessageBoxW(hwnd_, confirmation.c_str(), L"Desinstalar jogo",
                    MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) {
        return;
    }

    uninstalling_ = true;
    setNotice(L"Desinstalando dados do jogo. Os saves e o runtime serão mantidos.", NoticeInfo);
    InvalidateRect(hwnd_, nullptr, FALSE);

    const Layout layout = layout_;
    const HWND hwnd = hwnd_;
    try {
        uninstallThread_ = std::thread([layout, hwnd]() {
            auto outcome = std::make_unique<UninstallOutcome>();
            try {
                outcome->ok = uninstallGame(layout, outcome->error);
            } catch (const std::exception& exception) {
                outcome->error = L"Falha inesperada na desinstalação: " +
                                 toWide(exception.what());
            } catch (...) {
                outcome->error = L"Falha inesperada na desinstalação.";
            }
            PostMessageW(hwnd, WM_APP_UNINSTALL_DONE, 0,
                         reinterpret_cast<LPARAM>(outcome.release()));
        });
    } catch (const std::exception& exception) {
        uninstalling_ = false;
        setNotice(L"Não foi possível iniciar a desinstalação: " + toWide(exception.what()),
                  NoticeError);
    }
}

void App::Impl::handleUninstallDone(std::unique_ptr<UninstallOutcome> outcome) {
    if (uninstallThread_.joinable()) uninstallThread_.join();
    uninstalling_ = false;
    externalGameRunning_ = false;
    runtimeCheckError_.clear();
    isoPath_.clear();
    isoValid_ = false;
    isoStatus_.clear();
    isoDetail_.clear();
    refreshPackage();
    setPage(PageHome);

    if (outcome->ok) {
        setNotice(L"Jogo desinstalado. Saves, configuração, logs e runtime foram mantidos.",
                  NoticeSuccess);
        logInfo(L"Desinstalação concluída; saves, configuração, logs e runtime preservados.");
        MessageBoxW(hwnd_,
                    L"Os dados do jogo foram removidos.\n\n"
                    L"Saves, configuração, logs e runtime foram mantidos.\n"
                    L"Para jogar novamente, selecione e instale a ISO.",
                    kWindowTitle, MB_OK | MB_ICONINFORMATION);
    } else {
        setNotice(L"Não foi possível concluir a desinstalação: " + firstLine(outcome->error),
                  NoticeError);
        logError(L"Falha na desinstalação: " + outcome->error);
        MessageBoxW(hwnd_,
                    (L"Não foi possível concluir a desinstalação:\n" + outcome->error +
                     L"\n\nAlguns dados podem ter sido removidos. Saves, configuração, logs e "
                     L"runtime não fazem parte da remoção.")
                        .c_str(),
                    kWindowTitle, MB_OK | MB_ICONERROR);
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void App::Impl::handleInstallEvent(std::unique_ptr<InstallEvent> event) {
    const int index = static_cast<int>(event->step);
    if (index >= 0 && index < 5) {
        stepStates_[index] = event->state;
        if (!event->message.empty()) stepMessages_[index] = event->message;
    }
    overallPercent_ = event->overallPercent;
    if (!event->logLine.empty()) {
        installLog_.push_back(event->logLine);
        while (installLog_.size() > 200) installLog_.pop_front();
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void App::Impl::handleInstallDone(std::unique_ptr<InstallResult> result) {
    if (installThread_.joinable()) installThread_.join();
    installing_ = false;
    installFinished_ = true;
    installSucceeded_ = result->ok;
    if (result->ok) {
        overallPercent_ = 100;
        settings_.lastIsoPath = isoPath_;
        if (!saveSettings()) return;
        refreshPackage();
        if (!report_.ready) {
            setPage(PageHome);
            setNotice(L"A instalação terminou, mas a verificação final encontrou arquivos ausentes.",
                      NoticeError);
            return;
        }
        setPage(PageReady);
        setNotice(L"Instalação concluída. O jogo está pronto para abrir.", NoticeSuccess);
    } else {
        report_ = inspectPackageForLaunch(layout_);
        if (result->cancelled) {
            setNotice(L"Instalação cancelada. Os arquivos já extraídos foram mantidos.",
                      NoticeWarning);
        } else {
            setNotice(firstLine(result->message), NoticeError);
            MessageBoxW(hwnd_, result->message.c_str(), L"Instalação não concluída",
                        MB_OK | MB_ICONERROR);
        }
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
}

bool App::Impl::saveSettings() {
    std::wstring error;
    config_.settings() = settings_;
    if (!config_.save(layout_.configPath, error)) {
        setNotice(L"Não foi possível salvar as configurações: " + error, NoticeError);
        return false;
    }
    settings_ = config_.settings();
    return true;
}

void App::Impl::startGame() {
    if (gameRunning_ || uninstalling_) return;
    report_ = inspectPackageForLaunch(layout_);
    if (!report_.ready) {
        setNotice(L"O pacote precisa ser concluído antes de abrir o jogo.", NoticeError);
        refreshPackage();
        setPage(PageHome);
        return;
    }

    std::wstring error;
    layout_.ensureUserDirectories(error);
    std::error_code ec;
    fs::create_directories(layout_.gameLog().parent_path(), ec);

    applyGameEnvironment(layout_, settings_, diagnosticsMode_ || settings_.hangDiagnostics);

    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE log = CreateFileW(layout_.gameLog().c_str(), GENERIC_WRITE,
                             FILE_SHARE_READ | FILE_SHARE_WRITE, &security, CREATE_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL, nullptr);
    if (log == INVALID_HANDLE_VALUE) {
        log = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &security,
                          OPEN_EXISTING, 0, nullptr);
    }
    HANDLE input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                               &security, OPEN_EXISTING, 0, nullptr);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = log;
    startup.hStdError = log;
    startup.hStdInput = input;
    PROCESS_INFORMATION process{};
    const std::wstring dataArgument = relativeDataArgument(layout_, layout_.gameElf());
    const std::wstring command =
        quoteExecutableCommand(layout_.runtimeExe(), {dataArgument});
    logInfo(L"Iniciando jogo: " + command);
    const BOOL started =
        CreateProcessW(layout_.runtimeExe().c_str(), const_cast<LPWSTR>(command.c_str()),
                       nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                       layout_.dataRoot.wstring().c_str(), &startup, &process);
    const DWORD startError = GetLastError();
    if (log != INVALID_HANDLE_VALUE) CloseHandle(log);
    if (input != INVALID_HANDLE_VALUE) CloseHandle(input);
    if (!started) {
        const std::wstring message = L"Não foi possível abrir o jogo. Código Windows: " +
                                     std::to_wstring(startError) + L".\n" +
                                     windowsErrorMessage(startError);
        logError(message);
        MessageBoxW(hwnd_, message.c_str(), kWindowTitle, MB_OK | MB_ICONERROR);
        return;
    }
    CloseHandle(process.hThread);
    gameRunning_ = true;
    gamePid_ = process.dwProcessId;
    logInfo(L"Jogo iniciado, PID " + std::to_wstring(gamePid_));
    setNotice(L"Jogo aberto. Feche a janela do jogo para voltar ao launcher.", NoticeInfo);
    InvalidateRect(hwnd_, nullptr, FALSE);

    const HWND target = hwnd_;
    const DWORD pid = process.dwProcessId;
    HANDLE processHandle = process.hProcess;
    std::thread([target, pid, processHandle]() {
        nameGameWindow(pid);
        WaitForSingleObject(processHandle, INFINITE);
        DWORD exitCode = 0;
        GetExitCodeProcess(processHandle, &exitCode);
        CloseHandle(processHandle);
        PostMessageW(target, WM_APP_GAME_EXITED, exitCode, 0);
    }).detach();
}

void App::Impl::bringGameToFront() {
    struct Finder {
        DWORD pid = 0;
        HWND window = nullptr;
    } finder{gamePid_};
    EnumWindows(
        [](HWND window, LPARAM data) -> BOOL {
            auto* finder = reinterpret_cast<Finder*>(data);
            DWORD pid = 0;
            GetWindowThreadProcessId(window, &pid);
            if (pid == finder->pid && IsWindowVisible(window)) {
                finder->window = window;
                return FALSE;
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&finder));
    if (finder.window) {
        ShowWindow(finder.window, SW_RESTORE);
        SetForegroundWindow(finder.window);
    }
}

void App::Impl::handleGameExited(DWORD exitCode) {
    gameRunning_ = false;
    gamePid_ = 0;
    externalGameRunning_ = false;
    runtimeCheckError_.clear();
    refreshPackage();
    if (exitCode == 0 || exitCode == 0xFFFFFFFFu) {
        setNotice(L"O jogo foi fechado.", NoticeInfo);
    } else {
        setNotice(L"O jogo encerrou com código " + std::to_wstring(exitCode) +
                      L". Veja os logs em analysis\\.",
                  NoticeWarning);
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void App::Impl::startUpdateCheck(bool manual) {
    if (updateChecking_) {
        if (manual) {
            setPage(PageUpdates);
            setUpdateModal(UpdateModalChecking);
        }
        return;
    }
    updateChecking_ = true;
    updateError_.clear();
    updateStatus_ = L"Consultando a versão mais recente no GitHub...";
    if (manual) {
        setPage(PageUpdates);
        setUpdateModal(UpdateModalChecking);
    }
    InvalidateRect(hwnd_, nullptr, FALSE);

    const HWND target = hwnd_;
    std::thread([target, manual]() {
        auto outcome = std::make_unique<UpdateOutcome>();
        std::wstring error;
        outcome->manual = manual;
        outcome->ok = fetchLatestRelease(outcome->release, error);
        outcome->error = error;
        if (outcome->ok) {
            outcome->hasUpdate =
                compareVersions(DISNEY_LAUNCHER_VERSION_STRING, outcome->release.tag) < 0;
        }
        PostMessageW(target, WM_APP_UPDATE_DONE, 0,
                     reinterpret_cast<LPARAM>(outcome.release()));
    }).detach();
}

void App::Impl::handleUpdateDone(std::unique_ptr<UpdateOutcome> outcome) {
    updateChecking_ = false;
    bool noUpdates = false;
    if (!outcome->ok) {
        if (outcome->error.find(L"Ainda não há versões publicadas") != std::wstring::npos) {
            updateStatus_ = L"Ainda não há versões publicadas no GitHub. "
                            L"Esta é a build local " DISNEY_LAUNCHER_VERSION_STRING L".";
            noUpdates = true;
        } else {
            updateStatus_ = L"Não foi possível verificar atualizações: " + outcome->error;
            logWarning(updateStatus_);
        }
    } else {
        release_ = outcome->release;
        updateHasNew_ = outcome->hasUpdate;
        noUpdates = !updateHasNew_;
        if (updateHasNew_) {
            updateStatus_ = L"Nova versão disponível: " + release_.tag + L".";
        } else {
            updateStatus_ = L"Você está usando a versão mais recente (" +
                            release_.tag + L").";
        }
    }
    setUpdateModal(outcome->manual && noUpdates ? UpdateModalNoUpdates : UpdateModalNone);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void App::Impl::startDownload() {
    if (downloading_) return;
    const std::wstring installerName = installerNameForTag(release_.tag);
    if (installerName.empty()) {
        updateError_ = L"A tag da atualização não produz um nome de instalador válido.";
        setNotice(updateError_, NoticeError);
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }
    const ReleaseAsset* asset = nullptr;
    for (const ReleaseAsset& candidate : release_.assets) {
        if (candidate.name != installerName) continue;
        if (asset) {
            updateError_ = L"O release contém mais de um instalador oficial com o mesmo nome.";
            setNotice(updateError_, NoticeError);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        asset = &candidate;
    }
    if (!asset) {
        updateError_ = L"O release não contém o instalador oficial esperado.";
        setNotice(updateError_, NoticeError);
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }
    if (asset->size == 0 || asset->size > 512ULL * 1024 * 1024) {
        updateError_ = L"O tamanho do instalador informado pelo release é inválido.";
        setNotice(updateError_, NoticeError);
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }
    const fs::path destination = fs::temp_directory_path() / installerName;
    downloading_ = true;
    downloadPercent_ = 0;
    updateError_.clear();
    downloadCancel_ = std::make_shared<CancelToken>();
    setNotice(L"Baixando o instalador da nova versão...", NoticeInfo);

    const HWND target = hwnd_;
    const std::wstring url = asset->downloadUrl;
    const std::wstring releaseTag = release_.tag;
    const uint64_t releaseSize = asset->size;
    auto cancel = downloadCancel_;
    std::thread([target, url, destination, cancel, releaseTag, installerName, releaseSize]() {
        auto outcome = std::make_unique<DownloadOutcome>();
        auto lastPercent = std::make_shared<std::atomic<int>>(-1);
        std::wstring error;
        outcome->ok = downloadReleaseAsset(
            url, destination, releaseTag, installerName, releaseSize, *cancel,
            [&](uint64_t done, uint64_t total) {
                const int percent = total ? static_cast<int>(done * 100 / total) : 0;
                const int previous = lastPercent->exchange(percent);
                if (percent != previous) {
                    PostMessageW(target, WM_APP_DOWNLOAD_PROGRESS, percent, 0);
                }
            },
            error);
        outcome->error = error;
        outcome->path = destination;
        PostMessageW(target, WM_APP_DOWNLOAD_DONE, 0,
                     reinterpret_cast<LPARAM>(outcome.release()));
    }).detach();
}

void App::Impl::handleDownloadDone(std::unique_ptr<DownloadOutcome> outcome) {
    downloading_ = false;
    downloadCancel_.reset();
    if (!outcome->ok) {
        updateError_ = outcome->error;
        setNotice(L"Falha no download: " + firstLine(outcome->error), NoticeError);
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }
    std::wstring signatureError;
    if (!verifyInstallerSignature(outcome->path, signatureError)) {
        updateError_ = signatureError;
        setNotice(L"Instalador recusado: " + firstLine(signatureError), NoticeError);
        logError(updateError_);
        DeleteFileW(outcome->path.c_str());
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }
    downloadedInstaller_ = outcome->path.wstring();
    setNotice(L"Download concluído e assinatura validada. O instalador foi salvo em " +
                  outcome->path.wstring(),
              NoticeSuccess);
    InvalidateRect(hwnd_, nullptr, FALSE);
    const int answer = MessageBoxW(
        hwnd_,
        (L"O instalador foi baixado e sua assinatura foi validada pelo Windows.\n\n" +
         outcome->path.wstring() +
         L"\n\nDeseja executá-lo agora? O launcher será fechado para a atualização.")
            .c_str(),
        kWindowTitle, MB_YESNO | MB_ICONQUESTION);
    if (answer == IDYES) {
        if (!verifyInstallerSignature(outcome->path, signatureError)) {
            updateError_ = signatureError;
            downloadedInstaller_.clear();
            setNotice(L"Instalador recusado: " + firstLine(signatureError), NoticeError);
            logError(updateError_);
            DeleteFileW(outcome->path.c_str());
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        if (openPath(outcome->path)) DestroyWindow(hwnd_);
    }
}

bool App::Impl::openPath(const fs::path& path) {
    const HINSTANCE result = ShellExecuteW(hwnd_, L"open", path.c_str(), nullptr, nullptr,
                                           SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) <= 32) {
        MessageBoxW(hwnd_, (L"Não foi possível abrir:\n" + path.wstring()).c_str(),
                    kWindowTitle, MB_OK | MB_ICONERROR);
        return false;
    }
    return true;
}

void App::Impl::openControls() {
    for (const fs::path& candidate :
         {layout_.exeDir / L"docs" / L"controles.md", layout_.dataRoot / L"docs" / L"controles.md"}) {
        if (fileExists(candidate)) {
            openPath(candidate);
            return;
        }
    }
    MessageBoxW(
        hwnd_,
        L"Teclado:\n"
        L"  Direção: WASD ou setas\n"
        L"  Confirmar: Espaço ou X\n"
        L"  Voltar: C\n"
        L"  Start / Select: Enter / Tab\n"
        L"  L1 / R1: Q / E\n"
        L"  L2 / R2: Shift esquerdo / direito\n\n"
        L"Controle Xbox: mapeamento por posição dos botões; o teclado continua ativo.\n"
        L"Consulte docs/controles.md no projeto para a tabela completa.",
        kWindowTitle, MB_OK | MB_ICONINFORMATION);
}

void App::Impl::refreshLogEdit() {
    if (!logEdit_) return;
    std::wstring text;
    text += L"=== LAUNCHER  ·  " + layout_.launcherLog().wstring() + L" ===\r\n";
    const std::wstring launcherText = readLogTail(layout_.launcherLog(), 128 * 1024, 500);
    text += launcherText.empty() ? L"(sem registros)\r\n" : launcherText;
    text += L"\r\n\r\n=== JOGO  ·  " + layout_.gameLog().wstring() + L" ===\r\n";
    const std::wstring gameText = readLogTail(layout_.gameLog(), 192 * 1024, 800);
    text += gameText.empty() ? L"(o log é criado ao iniciar o jogo)\r\n" : gameText;
    SetWindowTextW(logEdit_, text.c_str());
    const int length = GetWindowTextLengthW(logEdit_);
    SendMessageW(logEdit_, EM_SETSEL, length, length);
    SendMessageW(logEdit_, EM_SCROLLCARET, 0, 0);
}

void App::Impl::layoutLogEdit() {
    if (!logEdit_ || !hwnd_) return;
    RECT client{};
    GetClientRect(hwnd_, &client);
    const float width = (client.right - client.left) / scale_;
    const float height = (client.bottom - client.top) / scale_;
    const float top = notice_.empty() ? 94.0f : 140.0f;
    const float cardHeight = height - top - 24;
    const int editX = static_cast<int>(S(26 + 22));
    const int editY = static_cast<int>(S(top + 82));
    const int editW = static_cast<int>(S(width - 52 - 44));
    const int editH = static_cast<int>(S(cardHeight - 106));
    SetWindowPos(logEdit_, nullptr, editX, editY, std::max(10, editW), std::max(10, editH),
                 SWP_NOZORDER | SWP_NOACTIVATE);
}

std::wstring App::Impl::diagnosticsText() const {
    std::ostringstream out;
    out << "Disney's Extreme Skate Adventure - diagnóstico do launcher\r\n";
    out << "Launcher " << DISNEY_LAUNCHER_VERSION_STRING_UTF8 << "\r\n";
    out << "Gerado em " << toUtf8(nowIso8601()) << "\r\n";
    out << "Modo: " << (layout_.portable ? "portátil" : "instalado") << "\r\n";
    out << "Dados: " << toUtf8(layout_.dataRoot.wstring()) << "\r\n";
    out << "Jogo: " << toUtf8(layout_.gameDir.wstring()) << "\r\n";
    out << "Runtime: " << toUtf8(layout_.runtimeExe().wstring()) << "\r\n";
    out << "Pacote pronto: " << (report_.ready ? "sim" : "não") << "\r\n";
    out << "Erros: " << report_.errorCount << "  Avisos: " << report_.warningCount << "\r\n";
    for (const CheckItem& item : report_.items) {
        out << "- [" << (item.status == CheckStatus::Ok        ? "OK"
                         : item.status == CheckStatus::Warning ? "AVISO"
                                                               : "ERRO")
            << "] " << toUtf8(item.label) << ": " << toUtf8(item.detail) << "\r\n";
    }
    out << "\r\nÚltimas linhas do launcher:\r\n";
    for (const std::wstring& line : Logger::instance().tail(60)) {
        out << toUtf8(line) << "\r\n";
    }
    return toWide(out.str());
}

void App::Impl::copyDiagnostics() {
    const std::wstring text = diagnosticsText();
    if (!OpenClipboard(hwnd_)) return;
    EmptyClipboard();
    const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (memory) {
        void* destination = GlobalLock(memory);
        if (destination) {
            std::memcpy(destination, text.c_str(), bytes);
            GlobalUnlock(memory);
            SetClipboardData(CF_UNICODETEXT, memory);
        } else {
            GlobalFree(memory);
        }
    }
    CloseClipboard();
    setNotice(L"Diagnóstico copiado para a área de transferência.", NoticeSuccess);
}

void App::Impl::onAction(int action, int value) {
    if (uninstalling_) return;
    switch (action) {
    case ActSelectIso: {
        wchar_t file[32768]{};
        if (!isoPath_.empty()) {
            wcsncpy_s(file, isoPath_.c_str(), _TRUNCATE);
        }
        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = hwnd_;
        dialog.lpstrFilter =
            L"Imagem ISO de PlayStation 2 (*.iso)\0*.iso\0Todos os arquivos (*.*)\0*.*\0";
        dialog.lpstrFile = file;
        dialog.nMaxFile = static_cast<DWORD>(std::size(file));
        dialog.lpstrTitle = L"Selecione a imagem do disco original (NTSC-U)";
        dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY |
                       OFN_EXPLORER | OFN_ENABLESIZING | OFN_DONTADDTORECENT;
        if (GetOpenFileNameW(&dialog)) {
            isoPath_ = file;
            if (!validateSelectedIso()) {
                setNotice(L"A imagem selecionada não é uma ISO NTSC-U válida de SLUS_206.07.",
                          NoticeError);
                return;
            }
            std::wstring error;
            if (!layout_.ensureUserDirectories(error)) {
                isoValid_ = false;
                setNotice(L"Não foi possível preparar os dados do usuário: " + firstLine(error),
                          NoticeError);
                return;
            }
            settings_.lastIsoPath = isoPath_;
            if (!saveSettings()) return;
            setNotice(L"ISO validada e caminho salvo. Clique em Instalar jogo para começar.",
                      NoticeSuccess);
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        break;
    }
    case ActInstall:
        startInstall();
        break;
    case ActRefreshPackage:
        refreshPackage();
        break;
    case ActCancelInstall:
        if (installing_) {
            cancelInstall();
        } else {
            setPage(PageHome);
        }
        break;
    case ActRetryInstall:
        installFinished_ = false;
        startInstall();
        break;
    case ActBackHome:
        installFinished_ = false;
        setPage(report_.ready ? PageReady : PageHome);
        setNotice(L"", NoticeInfo);
        break;
    case ActPlay:
        startGame();
        break;
    case ActBringGame:
        bringGameToFront();
        break;
    case ActSettings:
        draft_ = settings_;
        runtimeCheckError_.clear();
        externalGameRunning_ = gameRunning_ || runtimeProcessRunning(layout_, runtimeCheckError_);
        setPage(PageSettings);
        break;
    case ActUninstall:
        startUninstall();
        break;
    case ActUpdates:
        setPage(PageUpdates);
        if (release_.tag.empty()) startUpdateCheck(true);
        break;
    case ActLogs:
        setPage(PageLogs);
        break;
    case ActOpenGameFolder:
        openPath(layout_.gameDir);
        break;
    case ActOpenSaves:
        openPath(layout_.savesDir);
        break;
    case ActOpenLogsFolder: {
        std::error_code ec;
        fs::create_directories(layout_.logsDir, ec);
        openPath(layout_.logsDir);
        break;
    }
    case ActBack:
        setPage(report_.ready ? PageReady : PageHome);
        break;
    case ActSaveSettings:
        settings_ = draft_;
        if (!saveSettings()) return;
        setNotice(L"Configurações salvas. Elas valem na próxima vez que o jogo abrir.",
                  NoticeSuccess);
        setPage(report_.ready ? PageReady : PageHome);
        break;
    case ActCancelSettings:
        draft_ = settings_;
        setPage(report_.ready ? PageReady : PageHome);
        break;
    case ActScale1: draft_.renderScale = 1; break;
    case ActScale2: draft_.renderScale = 2; break;
    case ActScale3: draft_.renderScale = 3; break;
    case ActGpu: draft_.gpuRenderer = true; break;
    case ActSoftware: draft_.gpuRenderer = false; break;
    case ActVblankOn: draft_.nativeVblank = true; break;
    case ActVblankOff: draft_.nativeVblank = false; break;
    case ActDiagOn: draft_.hangDiagnostics = true; break;
    case ActDiagOff: draft_.hangDiagnostics = false; break;
    case ActUpdatesOn: draft_.checkUpdatesOnStart = true; break;
    case ActUpdatesOff: draft_.checkUpdatesOnStart = false; break;
    case ActCheckUpdates:
        startUpdateCheck(true);
        break;
    case ActOpenRelease:
        if (!release_.htmlUrl.empty()) openPath(release_.htmlUrl);
        break;
    case ActDownloadInstaller:
        startDownload();
        break;
    case ActOpenControls:
        openControls();
        break;
    case ActRefreshLogs:
        refreshLogEdit();
        break;
    case ActCopyDiagnostics:
        copyDiagnostics();
        break;
    case ActCloseUpdateModal:
        setUpdateModal(UpdateModalNone);
        break;
    case ActClose:
        PostMessageW(hwnd_, WM_CLOSE, 0, 0);
        break;
    default:
        break;
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
}

HWND App::window() const { return impl_->hwnd_; }

const wchar_t* App::className() { return kWindowClass; }

App& App::instance() {
    static App app;
    return app;
}

App::App() = default;
App::~App() = default;

bool App::initialize(HINSTANCE instance, bool diagnosticsMode) {
    if (!impl_) impl_ = std::make_unique<Impl>();
    return impl_->initialize(instance, diagnosticsMode);
}

int App::run() { return impl_->run(); }

void App::shutdown() {
    if (impl_) impl_->destroyGraphics();
}

} // namespace ds
