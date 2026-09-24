#pragma once

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>
#include <Runtime/Components/Component.hpp>

class Actor;

BIND_CLASS(table_init = true)
class LUDORK_ENGINE_API BillboardComponent : public Component {
public:
    LUDORK_CAST_DERIVED(BillboardComponent, Component)

    BIND_CLASS(copyable = true, table_init = true)
    struct BillboardItem {
        BIND_PROPERTY(meta(DropBox = {"text", "image"}))
        std::string kind = "text";

        BIND_PROPERTY(meta(Rely = {source = "kind", op = "==", value = "text"}))
        std::string text = "";

        BIND_PROPERTY(meta(Rely = {source = "kind", op = "==", value = "text"}))
        unsigned int fontSize = 12;

        BIND_PROPERTY(default = {255, 255, 255, 255},
                      meta(Rely = {source = "kind", op = "==", value = "text"}))
        sf::Color color = sf::Color::White;

        BIND_PROPERTY(meta(Rely = {source = "kind", op = "==", value = "image"},
                           PathVars = "/Game/Assets",
                           PathFilter = "*.png *.jpg *.jpeg *.bmp"))
        std::string image = "";

        bool operator==(const BillboardItem& other) const = default;
    };

    BIND_INIT()
    BillboardComponent();
    BillboardComponent(const BillboardComponent& other);
    BillboardComponent& operator=(const BillboardComponent& other);
    ~BillboardComponent() override;

    BIND_METHOD()
    RuntimeValue::Array onAttach(const RuntimeIdentityPtr& owner) override;

    BIND_PROPERTY(default = {})
    std::vector<BillboardItem> items;

    BIND_PROPERTY()
    float showRange = 128.0f;

    void update(Actor& owner, float deltaTime, bool presentationVisible,
                bool inRange);
    void draw(Actor& owner, sf::RenderTarget& target, sf::RenderStates states);
    void release() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::weak_ptr<Actor> owner_;
};
