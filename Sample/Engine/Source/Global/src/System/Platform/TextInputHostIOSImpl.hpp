#pragma once

#include "TextInputHostIOS.hpp"

#import <UIKit/UIKit.h>

#include <atomic>
#include <mutex>

namespace ludork::global {

class TextInputHostIOSImpl final
    : public ludork::engine::text_input::TextInputHost {
public:
    struct Session {
        ludork::engine::text_input::SessionId id;
        Sink sink;
        std::shared_ptr<std::atomic<bool>> windowLive;
        std::atomic<bool> completed = false;
        std::atomic<bool> cancelled = false;
        UIAlertController* dialog = nil;
        UIWindow* presentationWindow = nil;
        UIWindow* gameWindow = nil;
        NSObject* backgroundObserver = nil;
        NSObject* delegate = nil;

        Session(ludork::engine::text_input::SessionId value, Sink callback,
                std::shared_ptr<std::atomic<bool>> live);
        ~Session();
        void complete(bool accepted, bool notify);
    };

    explicit TextInputHostIOSImpl(sf::WindowHandle window);
    ~TextInputHostIOSImpl() override;
    bool isModal() const override;
    bool handlesKeyboard() const override;
    bool begin(ludork::engine::text_input::SessionId id,
               const ludork::engine::text_input::Request& request,
               Sink sink) override;
    void update(ludork::engine::text_input::SessionId id,
                const ludork::engine::text_input::State& state,
                const sf::FloatRect& caretRect) override;
    void end(ludork::engine::text_input::SessionId id) override;

private:
    sf::WindowHandle window_;
    std::shared_ptr<std::atomic<bool>> windowLive_;
    std::mutex mutex_;
    std::shared_ptr<Session> session_;
};

}  // namespace ludork::global

@interface LudorkTextInputDialogImpl : NSObject <UITextFieldDelegate> {
@public
    std::weak_ptr<ludork::global::TextInputHostIOSImpl::Session> session;
}
@end

@interface LudorkTextInputPresentationImpl : UIViewController
@end
