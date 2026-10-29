#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Runtime/WebViewHost.hpp>

#include <windows.h>
#include <WebView2.h>
#include <wrl/client.h>

#include <atomic>
#include <deque>
#include <mutex>
#include <thread>

namespace ludork::runtime::webview {

struct WindowsWebViewSessionImpl
    : std::enable_shared_from_this<WindowsWebViewSessionImpl> {
    struct Command {
        std::uint64_t id;
        std::wstring url;
    };

    HWND parent;
    HANDLE wakeEvent;
    HANDLE completionEvent;
    std::mutex queueMutex;
    std::deque<Command> commands;
    std::atomic<bool> stopping = false;
    std::thread thread;
    std::uint64_t id = 0;
    std::uint64_t generation = 0;
    std::uint64_t navigationId = 0;
    bool initializing = false;
    HWND window = nullptr;
    HWND status = nullptr;
    HWND closeButton = nullptr;
    HWND installButton = nullptr;
    RECT lastFrame{};
    std::wstring url;
    Microsoft::WRL::ComPtr<ICoreWebView2Environment> environment;
    Microsoft::WRL::ComPtr<ICoreWebView2Controller> controller;
    Microsoft::WRL::ComPtr<ICoreWebView2> webView;

    explicit WindowsWebViewSessionImpl(HWND windowHandle);
    ~WindowsWebViewSessionImpl();
    bool post(Command command);
    void run();
    void stop();
    void processCommands();
    void open(std::uint64_t sessionId, std::wstring address);
    void initializeWebView();
    void configureWebView();
    void close(bool notify);
    void resize();
    void showError(const wchar_t* message, bool missingRuntime = false);
    static LRESULT CALLBACK windowProcedure(HWND window, UINT message,
                                            WPARAM word, LPARAM parameter);

private:
    static constexpr DWORD MessagePollMilliseconds = 33;
    static constexpr int InitialWindowSize = 100;
    static constexpr int HorizontalPadding = 12;
    static constexpr int CloseButtonTop = 6;
    static constexpr int CloseButtonWidth = 80;
    static constexpr int CloseButtonHeight = 32;
    static constexpr int StatusTop = 12;
    static constexpr int StatusPreferredWidth = 320;
    static constexpr int StatusHeight = 24;
    static constexpr int StatusRightInset =
        CloseButtonWidth + HorizontalPadding * 3;
    static constexpr int InstallButtonTop = 60;
    static constexpr int InstallButtonPreferredWidth = 320;
    static constexpr int InstallButtonHeight = 36;
    static constexpr int ContentTop = 44;
};

class WindowsWebViewHostImpl final : public Host {
public:
    explicit WindowsWebViewHostImpl(sf::WindowHandle window);
    ~WindowsWebViewHostImpl() override;
    bool open(std::uint64_t id, const std::string& url) override;
    void close(std::uint64_t id) override;

private:
    std::shared_ptr<WindowsWebViewSessionImpl> session_;
};

}  // namespace ludork::runtime::webview
