#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Runtime/WebViewHost.hpp>

#include <windows.h>
#include <WebView2.h>
#include <wrl.h>

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
