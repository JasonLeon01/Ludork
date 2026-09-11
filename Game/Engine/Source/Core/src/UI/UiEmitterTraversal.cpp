#include <UI/UiEmitterTraversal.hpp>

#include <Emitters/EmitterScheduler.hpp>
#include <UI/EmitterView.hpp>
#include <UI/ListView.hpp>

namespace ludork::engine {

void collectUiEmitters(const std::shared_ptr<ControlBase>& root,
                       EmitterScheduler& scheduler) {
    if (root == nullptr || !root->getVisible()) {
        return;
    }
    if (ListView* list = dynamic_cast<ListView*>(root.get())) {
        list->applyPositions();
    }
    if (EmitterView* view = dynamic_cast<EmitterView*>(root.get());
        view != nullptr && !view->isDisposed()) {
        scheduler.registerEmitter(view->getEmitter(), view->prepareEmitter());
    }
    for (const std::shared_ptr<ControlBase>& child : root->getChildren()) {
        collectUiEmitters(child, scheduler);
    }
}

}  // namespace ludork::engine
