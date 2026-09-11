#include "WindowsWebViewHostImpl.hpp"
#include "WebViewPlatform.hpp"

#include <shellapi.h>

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <functional>
#include <utility>

namespace {

constexpr wchar_t windowClassName[] = L"Ludork.Runtime.WebView";
constexpr wchar_t installAddress[] =
    L"https://developer.microsoft.com/microsoft-edge/webview2/";
constexpr UINT closeButtonId = 1001;
constexpr UINT installButtonId = 1002;

std::wstring nativeText(const std::string& value) {
    const int length =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                            static_cast<int>(value.size()), nullptr, 0);
    if (length <= 0) {
        return {};
    }
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                        static_cast<int>(value.size()), result.data(), length);
    return result;
}

bool allowedAddress(const wchar_t* address) {
    if (address == nullptr) {
        return false;
    }
    std::wstring value(address);
    const std::size_t end = value.find(L':');
    if (end == std::wstring::npos) {
        return false;
    }
    value.resize(end);
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return value == L"http" || value == L"https";
}

std::filesystem::path dataDirectory() {
    std::wstring base(32768, L'\0');
    const DWORD length = GetEnvironmentVariableW(
        L"LOCALAPPDATA", base.data(), static_cast<DWORD>(base.size()));
    if (length == 0 || length >= base.size()) {
        return {};
    }
    base.resize(length);
    std::wstring executable(32768, L'\0');
    const DWORD executableLength = GetModuleFileNameW(
        nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    if (executableLength == 0 || executableLength >= executable.size()) {
        return {};
    }
    executable.resize(executableLength);
    return std::filesystem::path(base) / L"Ludork" / L"WebView2" /
           std::to_wstring(std::hash<std::wstring>{}(executable));
}

}  // namespace

namespace ludork::runtime::webview {

WindowsWebViewSessionImpl::WindowsWebViewSessionImpl(HWND windowHandle)
    : parent(windowHandle),
      wakeEvent(CreateEventW(nullptr, FALSE, FALSE, nullptr)) {}

WindowsWebViewSessionImpl::~WindowsWebViewSessionImpl() {
    if (wakeEvent != nullptr) {
        CloseHandle(wakeEvent);
    }
}

bool WindowsWebViewSessionImpl::post(Command command) {
    const std::lock_guard lock(queueMutex);
    if (stopping || wakeEvent == nullptr) {
        return false;
    }
    commands.push_back(std::move(command));
    SetEvent(wakeEvent);
    return true;
}

void WindowsWebViewSessionImpl::stop() {
    stopping = true;
    SetEvent(wakeEvent);
    if (thread.joinable()) {
        HANDLE worker = thread.native_handle();
        while (WaitForSingleObject(worker, 0) == WAIT_TIMEOUT) {
            const DWORD result = MsgWaitForMultipleObjectsEx(
                1, &worker, INFINITE, QS_SENDMESSAGE, MWMO_INPUTAVAILABLE);
            if (result != WAIT_OBJECT_0 + 1) {
                break;
            }
            MSG message{};
            PeekMessageW(&message, nullptr, 0, 0, PM_NOREMOVE);
        }
        thread.join();
    }
}

void WindowsWebViewSessionImpl::run() {
    const HRESULT initialized =
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = windowProcedure;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = windowClassName;
    RegisterClassExW(&windowClass);
    while (!stopping) {
        const DWORD result = MsgWaitForMultipleObjectsEx(
            1, &wakeEvent, 33, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        if (result == WAIT_FAILED) {
            break;
        }
        processCommands();
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                stopping = true;
                break;
            }
            if ((message.message == WM_KEYDOWN ||
                 message.message == WM_SYSKEYDOWN) &&
                message.wParam == VK_ESCAPE && window != nullptr) {
                close(true);
                continue;
            }
            if (window == nullptr || !IsDialogMessageW(window, &message)) {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
        }
        if (window != nullptr) {
            if (!IsWindow(parent)) {
                close(true);
            } else {
                resize();
            }
        }
    }
    close(true);
    if (SUCCEEDED(initialized)) {
        CoUninitialize();
    }
}

void WindowsWebViewSessionImpl::processCommands() {
    std::deque<Command> pending;
    {
        const std::lock_guard lock(queueMutex);
        pending.swap(commands);
    }
    for (Command& command : pending) {
        if (command.url.empty()) {
            if (command.id == 0 || command.id == id) {
                close(false);
            }
        } else if (!stopping) {
            open(command.id, std::move(command.url));
        }
    }
}

void WindowsWebViewSessionImpl::open(std::uint64_t sessionId,
                                     std::wstring address) {
    if (!IsWindow(parent)) {
        notifyClosed(sessionId);
        return;
    }
    id = sessionId;
    url = std::move(address);
    if (window == nullptr) {
        window = CreateWindowExW(WS_EX_TOOLWINDOW, windowClassName, L"WebView",
                                 WS_POPUP | WS_CLIPCHILDREN, 0, 0, 100, 100,
                                 GetAncestor(parent, GA_ROOT), nullptr,
                                 GetModuleHandleW(nullptr), this);
        if (window == nullptr) {
            close(true);
            return;
        }
        closeButton = CreateWindowExW(
            0, L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0,
            80, 32, window,
            reinterpret_cast<HMENU>(static_cast<UINT_PTR>(closeButtonId)),
            GetModuleHandleW(nullptr), nullptr);
        status = CreateWindowExW(
            0, L"STATIC", L"Loading…", WS_CHILD | WS_VISIBLE | SS_LEFT, 12, 12,
            320, 24, window, nullptr, GetModuleHandleW(nullptr), nullptr);
        installButton = CreateWindowExW(
            0, L"BUTTON", L"Install Microsoft WebView2 Runtime",
            WS_CHILD | WS_TABSTOP, 12, 60, 320, 36, window,
            reinterpret_cast<HMENU>(static_cast<UINT_PTR>(installButtonId)),
            GetModuleHandleW(nullptr), nullptr);
        const HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        SendMessageW(closeButton, WM_SETFONT, reinterpret_cast<WPARAM>(font),
                     TRUE);
        SendMessageW(status, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        SendMessageW(installButton, WM_SETFONT, reinterpret_cast<WPARAM>(font),
                     TRUE);
        resize();
        ShowWindow(window, SW_SHOW);
        SetForegroundWindow(window);
        SetFocus(closeButton);
    }
    if (webView != nullptr) {
        if (FAILED(webView->Navigate(url.c_str()))) {
            showError(L"Unable to open this web address.");
        }
    } else if (!initializing) {
        initializeWebView();
    }
}

void WindowsWebViewSessionImpl::initializeWebView() {
    LPWSTR version = nullptr;
    const HRESULT available =
        GetAvailableCoreWebView2BrowserVersionString(nullptr, &version);
    CoTaskMemFree(version);
    if (FAILED(available)) {
        showError(
            L"Microsoft WebView2 Runtime is missing. Install it, then reopen "
            L"this page.",
            true);
        return;
    }
    const std::filesystem::path directory = dataDirectory();
    std::error_code error;
    if (directory.empty()) {
        showError(L"The WebView user data directory is unavailable.");
        return;
    }
    std::filesystem::create_directories(directory, error);
    if (error) {
        showError(L"Unable to create the WebView user data directory.");
        return;
    }
    initializing = true;
    const std::uint64_t expected = generation;
    const std::weak_ptr<WindowsWebViewSessionImpl> weak = weak_from_this();
    const HRESULT created = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, directory.c_str(), nullptr,
        Microsoft::WRL::Callback<
            ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [weak, expected](HRESULT result,
                             ICoreWebView2Environment* environment) -> HRESULT {
                const auto state = weak.lock();
                if (state == nullptr || state->generation != expected ||
                    state->window == nullptr) {
                    return S_OK;
                }
                if (FAILED(result) || environment == nullptr) {
                    state->initializing = false;
                    state->showError(
                        L"Unable to initialize Microsoft WebView2.");
                    return S_OK;
                }
                state->environment = environment;
                const HRESULT controllerResult =
                    environment->CreateCoreWebView2Controller(
                        state->window,
                        Microsoft::WRL::Callback<
                            ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                            [weak, expected](HRESULT result,
                                             ICoreWebView2Controller*
                                                 controller) -> HRESULT {
                                const auto state = weak.lock();
                                if (state == nullptr ||
                                    state->generation != expected ||
                                    state->window == nullptr) {
                                    if (controller != nullptr) {
                                        controller->Close();
                                    }
                                    return S_OK;
                                }
                                state->initializing = false;
                                if (FAILED(result) || controller == nullptr) {
                                    state->showError(
                                        L"Unable to create Microsoft "
                                        L"WebView2.");
                                    return S_OK;
                                }
                                state->controller = controller;
                                controller->get_CoreWebView2(&state->webView);
                                if (state->webView == nullptr) {
                                    state->showError(
                                        L"Microsoft WebView2 is unavailable.");
                                    return S_OK;
                                }
                                state->configureWebView();
                                state->resize();
                                controller->put_IsVisible(TRUE);
                                controller->MoveFocus(
                                    COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
                                if (FAILED(state->webView->Navigate(
                                        state->url.c_str()))) {
                                    state->showError(
                                        L"Unable to open this web address.");
                                }
                                return S_OK;
                            })
                            .Get());
                if (FAILED(controllerResult)) {
                    state->initializing = false;
                    state->showError(L"Unable to create Microsoft WebView2.");
                }
                return S_OK;
            })
            .Get());
    if (FAILED(created)) {
        initializing = false;
        showError(L"Unable to initialize Microsoft WebView2.");
    }
}

void WindowsWebViewSessionImpl::configureWebView() {
    Microsoft::WRL::ComPtr<ICoreWebView2Settings> settings;
    if (SUCCEEDED(webView->get_Settings(&settings))) {
        settings->put_AreDevToolsEnabled(FALSE);
        settings->put_AreDefaultContextMenusEnabled(FALSE);
        settings->put_IsStatusBarEnabled(FALSE);
    }
    const std::weak_ptr<WindowsWebViewSessionImpl> weak = weak_from_this();
    const std::uint64_t expected = generation;
    EventRegistrationToken token{};
    controller->add_AcceleratorKeyPressed(
        Microsoft::WRL::Callback<
            ICoreWebView2AcceleratorKeyPressedEventHandler>(
            [weak, expected](
                ICoreWebView2Controller*,
                ICoreWebView2AcceleratorKeyPressedEventArgs* args) -> HRESULT {
                UINT key = 0;
                COREWEBVIEW2_KEY_EVENT_KIND kind{};
                args->get_VirtualKey(&key);
                args->get_KeyEventKind(&kind);
                if (key == VK_ESCAPE &&
                    (kind == COREWEBVIEW2_KEY_EVENT_KIND_KEY_DOWN ||
                     kind == COREWEBVIEW2_KEY_EVENT_KIND_SYSTEM_KEY_DOWN)) {
                    if (const auto state = weak.lock();
                        state != nullptr && state->generation == expected &&
                        state->window != nullptr) {
                        args->put_Handled(TRUE);
                        PostMessageW(state->window, WM_CLOSE, 0, 0);
                    }
                }
                return S_OK;
            })
            .Get(),
        &token);
    webView->add_NavigationStarting(
        Microsoft::WRL::Callback<ICoreWebView2NavigationStartingEventHandler>(
            [weak, expected](
                ICoreWebView2*,
                ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
                const auto state = weak.lock();
                if (state == nullptr || state->generation != expected) {
                    return args->put_Cancel(TRUE);
                }
                LPWSTR address = nullptr;
                args->get_Uri(&address);
                const bool allowed = allowedAddress(address);
                CoTaskMemFree(address);
                if (!allowed) {
                    args->put_Cancel(TRUE);
                } else {
                    args->get_NavigationId(&state->navigationId);
                    SetWindowTextW(state->status, L"Loading…");
                }
                return S_OK;
            })
            .Get(),
        &token);
    webView->add_NavigationCompleted(
        Microsoft::WRL::Callback<ICoreWebView2NavigationCompletedEventHandler>(
            [weak, expected](
                ICoreWebView2*,
                ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
                UINT64 navigationId = 0;
                args->get_NavigationId(&navigationId);
                BOOL success = FALSE;
                args->get_IsSuccess(&success);
                if (const auto state = weak.lock();
                    state != nullptr && state->generation == expected &&
                    state->navigationId == navigationId) {
                    SetWindowTextW(state->status,
                                   success ? L""
                                           : L"Unable to load this page. Close "
                                             L"and reopen to retry.");
                }
                return S_OK;
            })
            .Get(),
        &token);
    webView->add_NewWindowRequested(
        Microsoft::WRL::Callback<ICoreWebView2NewWindowRequestedEventHandler>(
            [weak, expected](
                ICoreWebView2*,
                ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT {
                args->put_Handled(TRUE);
                LPWSTR address = nullptr;
                args->get_Uri(&address);
                if (allowedAddress(address)) {
                    if (const auto state = weak.lock();
                        state != nullptr && state->generation == expected &&
                        state->webView != nullptr) {
                        state->webView->Navigate(address);
                    }
                }
                CoTaskMemFree(address);
                return S_OK;
            })
            .Get(),
        &token);
    webView->add_PermissionRequested(
        Microsoft::WRL::Callback<ICoreWebView2PermissionRequestedEventHandler>(
            [](ICoreWebView2*,
               ICoreWebView2PermissionRequestedEventArgs* args) -> HRESULT {
                return args->put_State(COREWEBVIEW2_PERMISSION_STATE_DENY);
            })
            .Get(),
        &token);
    webView->add_ProcessFailed(
        Microsoft::WRL::Callback<ICoreWebView2ProcessFailedEventHandler>(
            [weak, expected](ICoreWebView2*,
                             ICoreWebView2ProcessFailedEventArgs*) -> HRESULT {
                if (const auto state = weak.lock();
                    state != nullptr && state->generation == expected) {
                    state->showError(
                        L"Web content stopped. Close and reopen this page.");
                }
                return S_OK;
            })
            .Get(),
        &token);
    Microsoft::WRL::ComPtr<ICoreWebView2_4> downloads;
    if (SUCCEEDED(webView.As(&downloads))) {
        downloads->add_DownloadStarting(
            Microsoft::WRL::Callback<ICoreWebView2DownloadStartingEventHandler>(
                [](ICoreWebView2*,
                   ICoreWebView2DownloadStartingEventArgs* args) -> HRESULT {
                    args->put_Handled(TRUE);
                    return args->put_Cancel(TRUE);
                })
                .Get(),
            &token);
    }
    webView->add_WindowCloseRequested(
        Microsoft::WRL::Callback<ICoreWebView2WindowCloseRequestedEventHandler>(
            [weak, expected](ICoreWebView2*, IUnknown*) -> HRESULT {
                if (const auto state = weak.lock();
                    state != nullptr && state->generation == expected) {
                    PostMessageW(state->window, WM_CLOSE, 0, 0);
                }
                return S_OK;
            })
            .Get(),
        &token);
}

void WindowsWebViewSessionImpl::showError(const wchar_t* message,
                                          bool missingRuntime) {
    SetWindowTextW(status, message);
    ShowWindow(installButton, missingRuntime ? SW_SHOW : SW_HIDE);
}

void WindowsWebViewSessionImpl::resize() {
    if (window == nullptr) {
        return;
    }
    RECT client{};
    if (!GetClientRect(parent, &client)) {
        return;
    }
    POINT origin{client.left, client.top};
    ClientToScreen(parent, &origin);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    const RECT frame{origin.x, origin.y, origin.x + width, origin.y + height};
    if (!EqualRect(&frame, &lastFrame)) {
        SetWindowPos(window, nullptr, origin.x, origin.y, width, height,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        lastFrame = frame;
    }
    MoveWindow(closeButton, std::max(0, width - 92), 6, 80, 32, TRUE);
    MoveWindow(status, 12, 12, std::max(0, width - 116), 24, TRUE);
    MoveWindow(installButton, 12, 60, std::max(0, std::min(320, width - 24)),
               36, TRUE);
    if (controller != nullptr) {
        RECT content{0, 44, width, std::max(44, height)};
        controller->put_Bounds(content);
        controller->NotifyParentWindowPositionChanged();
    }
    const bool visible =
        IsWindowVisible(parent) && !IsIconic(GetAncestor(parent, GA_ROOT));
    if (static_cast<bool>(IsWindowVisible(window)) != visible) {
        ShowWindow(window, visible ? SW_SHOWNOACTIVATE : SW_HIDE);
    }
}

void WindowsWebViewSessionImpl::close(bool notify) {
    const std::uint64_t previous = std::exchange(id, 0);
    ++generation;
    initializing = false;
    if (controller != nullptr) {
        controller->Close();
    }
    webView.Reset();
    controller.Reset();
    environment.Reset();
    if (window != nullptr) {
        DestroyWindow(window);
    }
    window = nullptr;
    status = nullptr;
    closeButton = nullptr;
    installButton = nullptr;
    lastFrame = {};
    if (notify && previous != 0) {
        notifyClosed(previous);
    }
}

LRESULT CALLBACK WindowsWebViewSessionImpl::windowProcedure(HWND window,
                                                            UINT message,
                                                            WPARAM word,
                                                            LPARAM parameter) {
    WindowsWebViewSessionImpl* state =
        reinterpret_cast<WindowsWebViewSessionImpl*>(
            GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        state = static_cast<WindowsWebViewSessionImpl*>(
            reinterpret_cast<CREATESTRUCTW*>(parameter)->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(state));
    }
    if (state != nullptr) {
        if (message == WM_CLOSE ||
            (message == WM_COMMAND && LOWORD(word) == closeButtonId)) {
            state->close(true);
            return 0;
        }
        if (message == WM_COMMAND && LOWORD(word) == installButtonId) {
            ShellExecuteW(window, L"open", installAddress, nullptr, nullptr,
                          SW_SHOWNORMAL);
            return 0;
        }
    }
    return DefWindowProcW(window, message, word, parameter);
}

WindowsWebViewHostImpl::WindowsWebViewHostImpl(sf::WindowHandle window)
    : session_(std::make_shared<WindowsWebViewSessionImpl>(window)) {
    const auto state = session_;
    state->thread = std::thread([state]() {
        state->run();
    });
}

WindowsWebViewHostImpl::~WindowsWebViewHostImpl() {
    session_->stop();
}

bool WindowsWebViewHostImpl::open(std::uint64_t id, const std::string& url) {
    std::wstring address = nativeText(url);
    return !address.empty() && session_->post({id, std::move(address)});
}

void WindowsWebViewHostImpl::close(std::uint64_t id) {
    session_->post({id, {}});
}

std::shared_ptr<Host> createPlatformHost(sf::WindowHandle window) {
    return std::make_shared<WindowsWebViewHostImpl>(window);
}

}  // namespace ludork::runtime::webview
