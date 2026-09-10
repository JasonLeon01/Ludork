#include <Utils/EventBus.hpp>

#include <algorithm>
#include <iostream>
#include <stdexcept>

std::mutex EventBus::blueprintEventMutex_;
EventBus::BlueprintEventValidator EventBus::blueprintEventValidator_;
EventBus::BlueprintEventInvoker EventBus::blueprintEventInvoker_;

std::size_t EventBus::subscribe(const std::string& event, Handler handler,
                                int priority) {
    std::scoped_lock lock(mutex_);
    return subscribeLocked(event, std::move(handler), priority, {});
}

std::size_t EventBus::once(const std::string& event, Handler handler,
                           int priority) {
    std::shared_ptr<std::size_t> token = std::make_shared<std::size_t>(0);
    *token = subscribe(
        event,
        [this, token,
         handler = std::move(handler)](const RuntimeValue& payload) {
            struct UnsubscribeGuard {
                EventBus* bus;
                std::size_t token;
                ~UnsubscribeGuard() {
                    bus->unsubscribe(token);
                }
            } guard{this, *token};
            handler(payload);
        },
        priority);
    return *token;
}

std::size_t EventBus::subscribeObjectHandler(const std::string& event,
                                             RuntimeIdentityPtr object,
                                             Handler handler, int priority) {
    std::scoped_lock lock(mutex_);
    unsubscribeObjectHandler(event, object);
    return subscribeLocked(event, std::move(handler), priority,
                           std::move(object));
}

std::size_t EventBus::subscribeBlueprintEvent(const std::string& event,
                                              RuntimeIdentityPtr object,
                                              const std::string& eventName,
                                              int priority) {
    validateBlueprintEvent(object, eventName);
    RuntimeIdentityPtr callbackObject = object;
    return subscribeObjectHandler(
        event, std::move(object),
        [object = std::move(callbackObject), eventName](const RuntimeValue&) {
            BlueprintEventInvoker invoker;
            {
                std::scoped_lock lock(blueprintEventMutex_);
                invoker = blueprintEventInvoker_;
            }
            if (invoker) {
                invoker(object, eventName);
            }
        },
        priority);
}

std::size_t EventBus::onceBlueprintEvent(const std::string& event,
                                         RuntimeIdentityPtr object,
                                         const std::string& eventName,
                                         int priority) {
    validateBlueprintEvent(object, eventName);
    return once(
        event,
        [object = std::move(object), eventName](const RuntimeValue&) {
            BlueprintEventInvoker invoker;
            {
                std::scoped_lock lock(blueprintEventMutex_);
                invoker = blueprintEventInvoker_;
            }
            if (invoker) {
                invoker(object, eventName);
            }
        },
        priority);
}

bool EventBus::unsubscribe(std::size_t token) {
    std::scoped_lock lock(mutex_);
    for (auto iterator = handlers_.begin(); iterator != handlers_.end();) {
        std::vector<Subscription>& subscriptions = iterator->second;
        const auto match =
            std::find_if(subscriptions.begin(), subscriptions.end(),
                         [token](const Subscription& subscription) {
                             return subscription.token == token;
                         });
        if (match != subscriptions.end()) {
            subscriptions.erase(match);
            if (subscriptions.empty()) {
                handlers_.erase(iterator);
            }
            return true;
        }
        ++iterator;
    }
    return false;
}

bool EventBus::unsubscribeEvent(const std::string& event) {
    std::scoped_lock lock(mutex_);
    return handlers_.erase(event) != 0;
}

bool EventBus::unsubscribeObjectHandler(const std::string& event,
                                        const RuntimeIdentityPtr& object) {
    std::scoped_lock lock(mutex_);
    const auto entry = handlers_.find(event);
    if (entry == handlers_.end()) {
        return false;
    }
    std::vector<Subscription>& subscriptions = entry->second;
    const auto match =
        std::find_if(subscriptions.begin(), subscriptions.end(),
                     [&object](const Subscription& subscription) {
                         return identitiesEqual(subscription.object, object);
                     });
    if (match == subscriptions.end()) {
        return false;
    }
    subscriptions.erase(match);
    if (subscriptions.empty()) {
        handlers_.erase(entry);
    }
    return true;
}

void EventBus::clear(const std::optional<std::string>& event) {
    std::scoped_lock lock(mutex_);
    if (event.has_value()) {
        handlers_.erase(*event);
    } else {
        handlers_.clear();
    }
}

void EventBus::publish(const std::string& event, const RuntimeValue& payload) {
    std::vector<Subscription> snapshot;
    {
        std::scoped_lock lock(mutex_);
        const auto entry = handlers_.find(event);
        if (entry != handlers_.end()) {
            snapshot = entry->second;
        }
    }
    for (const Subscription& subscription : snapshot) {
        try {
            subscription.handler(payload);
        } catch (const std::exception& error) {
            std::cerr << "EventBus: handler error for '" << event
                      << "': " << error.what() << '\n';
        }
    }
}

void EventBus::post(const std::string& event, const RuntimeValue& payload) {
    std::scoped_lock lock(mutex_);
    queue_.push_back({event, payload});
}

std::size_t EventBus::flush(std::optional<std::size_t> limit) {
    std::size_t processed = 0;
    while (!limit.has_value() || processed < *limit) {
        PostedEvent event;
        {
            std::scoped_lock lock(mutex_);
            if (queue_.empty()) {
                break;
            }
            event = std::move(queue_.front());
            queue_.pop_front();
        }
        publish(event.event, event.payload);
        ++processed;
    }
    return processed;
}

void EventBus::reset() noexcept {
    std::scoped_lock lock(mutex_);
    handlers_.clear();
    queue_.clear();
    nextToken_ = 1;
}

void EventBus::setBlueprintEventValidator(BlueprintEventValidator validator) {
    std::scoped_lock lock(blueprintEventMutex_);
    blueprintEventValidator_ = std::move(validator);
}

void EventBus::setBlueprintEventInvoker(BlueprintEventInvoker invoker) {
    std::scoped_lock lock(blueprintEventMutex_);
    blueprintEventInvoker_ = std::move(invoker);
}

void EventBus::triggerBlueprintEvent(const RuntimeIdentityPtr& object,
                                     const std::string& eventName) {
    validateBlueprintEvent(object, eventName);
    BlueprintEventInvoker invoker;
    {
        std::scoped_lock lock(blueprintEventMutex_);
        invoker = blueprintEventInvoker_;
    }
    if (invoker) {
        invoker(object, eventName);
    }
}

std::size_t EventBus::subscribeLocked(const std::string& event, Handler handler,
                                      int priority, RuntimeIdentityPtr object) {
    const std::size_t token = nextToken_++;
    std::vector<Subscription>& subscriptions = handlers_[event];
    subscriptions.push_back(
        {token, priority, std::move(handler), std::move(object)});
    std::stable_sort(subscriptions.begin(), subscriptions.end(),
                     [](const Subscription& left, const Subscription& right) {
                         return left.priority > right.priority;
                     });
    return token;
}

bool EventBus::identitiesEqual(const RuntimeIdentityPtr& left,
                               const RuntimeIdentityPtr& right) {
    if (left == nullptr || right == nullptr) {
        return left == right;
    }
    return left->equals(*right);
}

void EventBus::validateBlueprintEvent(const RuntimeIdentityPtr& object,
                                      const std::string& eventName) {
    BlueprintEventValidator validator;
    {
        std::scoped_lock lock(blueprintEventMutex_);
        validator = blueprintEventValidator_;
    }
    if (!validator) {
        throw std::runtime_error("Blueprint event validator is not configured");
    }
    validator(object, eventName);
}

EventBus& eventBus() {
    static EventBus bus;
    return bus;
}

EventBus* const DefaultEventBus = &eventBus();

void setBlueprintEventValidator(EventBus::BlueprintEventValidator validator) {
    EventBus::setBlueprintEventValidator(std::move(validator));
}

void setBlueprintEventInvoker(EventBus::BlueprintEventInvoker invoker) {
    EventBus::setBlueprintEventInvoker(std::move(invoker));
}

std::size_t subscribeEvent(const std::string& event, EventHandler handler,
                           int priority) {
    return eventBus().subscribe(event, std::move(handler), priority);
}

std::size_t onceEvent(const std::string& event, EventHandler handler,
                      int priority) {
    return eventBus().once(event, std::move(handler), priority);
}

std::size_t subscribeObjectEvent(const std::string& event,
                                 RuntimeIdentityPtr object,
                                 EventHandler handler, int priority) {
    return eventBus().subscribeObjectHandler(event, std::move(object),
                                             std::move(handler), priority);
}

std::size_t subscribeBlueprintEvent(const std::string& event,
                                    RuntimeIdentityPtr object,
                                    const std::string& eventName,
                                    int priority) {
    return eventBus().subscribeBlueprintEvent(event, std::move(object),
                                              eventName, priority);
}

std::size_t onceBlueprintEvent(const std::string& event,
                               RuntimeIdentityPtr object,
                               const std::string& eventName, int priority) {
    return eventBus().onceBlueprintEvent(event, std::move(object), eventName,
                                         priority);
}

bool unsubscribeEvent(std::size_t token) {
    return eventBus().unsubscribe(token);
}

bool unsubscribeEvent(const std::string& event) {
    return eventBus().unsubscribeEvent(event);
}

bool unsubscribeObjectEvent(const std::string& event,
                            const RuntimeIdentityPtr& object) {
    return eventBus().unsubscribeObjectHandler(event, object);
}

void publishEvent(const std::string& event, const RuntimeValue& payload) {
    eventBus().publish(event, payload);
}

void postEvent(const std::string& event, const RuntimeValue& payload) {
    eventBus().post(event, payload);
}

std::size_t flushEvents(std::optional<std::size_t> limit) {
    return eventBus().flush(limit);
}

void clearEvents(const std::optional<std::string>& event) {
    eventBus().clear(event);
}

void triggerBlueprintEvent(const RuntimeIdentityPtr& object,
                           const std::string& eventName) {
    eventBus().triggerBlueprintEvent(object, eventName);
}

void shutdownEventBus() noexcept {
    eventBus().reset();
    EventBus::setBlueprintEventValidator({});
    EventBus::setBlueprintEventInvoker({});
}
