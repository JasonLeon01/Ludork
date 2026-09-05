#pragma once

#include <CoreMinimal.hpp>

#include <EngineRuntimeApi.hpp>

#include <deque>
#include <mutex>

using EventHandler = std::function<void(const RuntimeValue&)>;

BIND_CLASS()
class EventBus {
public:
    using Handler = EventHandler;
    using BlueprintEventValidator =
        std::function<void(const RuntimeIdentityPtr&, const std::string&)>;
    using BlueprintEventInvoker =
        std::function<void(const RuntimeIdentityPtr&, const std::string&)>;

    BIND_INIT()
    EventBus() = default;

    BIND_METHOD(defaults = {0})
    std::size_t subscribe(const std::string& event, Handler handler,
                          int priority = 0);

    BIND_METHOD(defaults = {0})
    std::size_t once(const std::string& event, Handler handler,
                     int priority = 0);

    BIND_METHOD(defaults = {0})
    std::size_t subscribeObjectHandler(const std::string& event,
                                       RuntimeIdentityPtr object,
                                       Handler handler, int priority = 0);

    BIND_METHOD(defaults = {0})
    std::size_t subscribeBlueprintEvent(const std::string& event,
                                        RuntimeIdentityPtr object,
                                        const std::string& eventName,
                                        int priority = 0);

    BIND_METHOD(defaults = {0})
    std::size_t onceBlueprintEvent(const std::string& event,
                                   RuntimeIdentityPtr object,
                                   const std::string& eventName,
                                   int priority = 0);

    BIND_METHOD()
    bool unsubscribe(std::size_t token);

    BIND_METHOD()
    bool unsubscribeEvent(const std::string& event);

    BIND_METHOD()
    bool unsubscribeObjectHandler(const std::string& event,
                                  const RuntimeIdentityPtr& object);

    BIND_METHOD()
    void clear(const std::optional<std::string>& event = std::nullopt);

    BIND_METHOD(defaults = {nil})
    void publish(const std::string& event,
                 const RuntimeValue& payload = RuntimeValue());

    BIND_METHOD(defaults = {nil})
    void post(const std::string& event,
              const RuntimeValue& payload = RuntimeValue());

    BIND_METHOD()
    std::size_t flush(std::optional<std::size_t> limit = std::nullopt);

    void reset() noexcept;

    static void setBlueprintEventValidator(BlueprintEventValidator validator);
    static void setBlueprintEventInvoker(BlueprintEventInvoker invoker);
    void triggerBlueprintEvent(const RuntimeIdentityPtr& object,
                               const std::string& eventName);

private:
    struct Subscription {
        std::size_t token;
        int priority;
        Handler handler;
        RuntimeIdentityPtr object;
    };

    struct PostedEvent {
        std::string event;
        RuntimeValue payload;
    };

    std::size_t subscribeLocked(const std::string& event, Handler handler,
                                int priority, RuntimeIdentityPtr object);
    static bool identitiesEqual(const RuntimeIdentityPtr& left,
                                const RuntimeIdentityPtr& right);
    static void validateBlueprintEvent(const RuntimeIdentityPtr& object,
                                       const std::string& eventName);

    mutable std::recursive_mutex mutex_;
    std::unordered_map<std::string, std::vector<Subscription>> handlers_;
    std::deque<PostedEvent> queue_;
    std::size_t nextToken_ = 1;
    static std::mutex blueprintEventMutex_;
    static BlueprintEventValidator blueprintEventValidator_;
    static BlueprintEventInvoker blueprintEventInvoker_;
};

BIND_MODULE_PROPERTY(name = "_default_bus", readonly = true, metadata = false)
extern LUDORK_ENGINE_API EventBus* const DefaultEventBus;

BIND_INJECT(global = "_LUDORK_BLUEPRINT_EVENT_VALIDATOR")
void setBlueprintEventValidator(EventBus::BlueprintEventValidator validator);

BIND_INJECT(global = "_LUDORK_BLUEPRINT_EVENT_INVOKER")
void setBlueprintEventInvoker(EventBus::BlueprintEventInvoker invoker);

BIND_FUNCTION()
EventBus& eventBus();

BIND_FUNCTION(name = "subscribe", defaults = {0})
std::size_t subscribeEvent(const std::string& event, EventHandler handler,
                           int priority = 0);

BIND_FUNCTION(name = "once", defaults = {0})
std::size_t onceEvent(const std::string& event, EventHandler handler,
                      int priority = 0);

BIND_FUNCTION(name = "subscribeObjectHandler", defaults = {0})
std::size_t subscribeObjectEvent(const std::string& event,
                                 RuntimeIdentityPtr object,
                                 EventHandler handler, int priority = 0);

BIND_FUNCTION(name = "subscribeBlueprintEvent", defaults = {0})
std::size_t subscribeBlueprintEvent(const std::string& event,
                                    RuntimeIdentityPtr object,
                                    const std::string& eventName,
                                    int priority = 0);

BIND_FUNCTION(name = "onceBlueprintEvent", defaults = {0})
std::size_t onceBlueprintEvent(const std::string& event,
                               RuntimeIdentityPtr object,
                               const std::string& eventName, int priority = 0);

BIND_FUNCTION(name = "unsubscribe")
bool unsubscribeEvent(std::size_t token);

BIND_FUNCTION(name = "unsubscribeEvent")
bool unsubscribeEvent(const std::string& event);

BIND_FUNCTION(name = "unsubscribeObjectHandler")
bool unsubscribeObjectEvent(const std::string& event,
                            const RuntimeIdentityPtr& object);

BIND_FUNCTION(name = "publish", defaults = {nil})
void publishEvent(const std::string& event,
                  const RuntimeValue& payload = RuntimeValue());

BIND_FUNCTION(name = "post", defaults = {nil})
void postEvent(const std::string& event,
               const RuntimeValue& payload = RuntimeValue());

BIND_FUNCTION(name = "flush", defaults = {nil})
LUDORK_ENGINE_API std::size_t flushEvents(
    std::optional<std::size_t> limit = std::nullopt);

BIND_FUNCTION(name = "clear", defaults = {nil})
void clearEvents(const std::optional<std::string>& event = std::nullopt);

BIND_FUNCTION(name = "triggerBlueprintEvent")
void triggerBlueprintEvent(const RuntimeIdentityPtr& object,
                           const std::string& eventName);

LUDORK_ENGINE_API void shutdownEventBus() noexcept;
