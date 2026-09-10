#pragma once

#include <Input/TextInputHost.hpp>
#include <SFML/Window/WindowBase.hpp>
#import <AppKit/AppKit.h>

@interface LudorkTextInputClientImpl : NSView <NSTextInputClient> {
@public
    ludork::engine::text_input::SessionId sessionId;
    ludork::engine::text_input::State textState;
    ludork::engine::text_input::TextInputHost::Sink eventSink;
    sf::FloatRect caretRectangle;
    NSTextInputContext* textContext;
    NSView* surfaceView;
    sf::Vector2u surfaceSize;
    bool suppressEnter;
    bool suppressSpace;
}
- (std::optional<std::pair<std::size_t, std::size_t>>)selectionForReplacement:
    (NSRange)range;
- (NSEvent*)handleNativeEvent:(NSEvent*)event;
- (void)sendCommand:(ludork::engine::text_input::Command)command
             extend:(BOOL)extend;
@end

namespace ludork::global {

class MacTextInputHostImpl final
    : public ludork::engine::text_input::TextInputHost {
public:
    explicit MacTextInputHostImpl(sf::WindowBase& window);
    ~MacTextInputHostImpl() override;
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
    sf::WindowBase& window_;
    ludork::engine::text_input::SessionId id_ = 0;
    NSWindow* nativeWindow_ = nil;
    NSResponder* previousResponder_ = nil;
    LudorkTextInputClientImpl* client_ = nil;
    id eventMonitor_ = nil;
};

}  // namespace ludork::global
