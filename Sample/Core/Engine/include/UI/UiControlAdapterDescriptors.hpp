#pragma once

#include <LudorkRuntimeBinding/Annotations.hpp>

#include <array>
#include <span>
#include <string_view>

enum class UiChildPolicy {
    None,
    Single,
    Multiple,
};

enum class UiControlSlotType {
    None,
    Canvas,
    List,
};

struct UiControlPropertyDescriptor {
    std::string_view id;
    std::string_view type;
    std::string_view defaultJson;
    bool required;
    bool editorOnly;
    bool adapterProperty;
};

struct UiControlAdapterDescriptor {
    std::string_view controlId;
    std::string_view adapter;
    std::string_view displayName;
    std::string_view category;
    UiChildPolicy childPolicy;
    UiControlSlotType slotType;
    std::span<const UiControlPropertyDescriptor> properties;
};

template <typename Tag>
struct UiControlAdapterTraits;

#define UI_CONTROL_COMMON_PROPERTY(ID, TYPE, REQUIRED, DEFAULT_JSON) \
    UiControlPropertyDescriptor {                                    \
        ID, TYPE, DEFAULT_JSON, REQUIRED, false, false               \
    }

#define UI_CONTROL_PROPERTY(ID, TYPE, REQUIRED, DEFAULT_JSON) \
    UiControlPropertyDescriptor {                             \
        ID, TYPE, DEFAULT_JSON, REQUIRED, false, true         \
    }

#define UI_CONTROL_EDITOR_PROPERTY(ID, TYPE, REQUIRED, DEFAULT_JSON) \
    UiControlPropertyDescriptor {                                    \
        ID, TYPE, DEFAULT_JSON, REQUIRED, true, false                \
    }

#define UI_CONTROL_COMMON_PROPERTIES                                   \
    UI_CONTROL_COMMON_PROPERTY("visible", "bool", false, "true"),      \
        UI_CONTROL_COMMON_PROPERTY("rotation", "float", false, "0.0"), \
        UI_CONTROL_COMMON_PROPERTY("scale", "sf.Vector2f", false,      \
                                   "[1.0,1.0]"),                       \
        UI_CONTROL_COMMON_PROPERTY("origin", "sf.Vector2f", false,     \
                                   "[0.0,0.0]")

#define UI_CONTROL_PLAIN_TEXT_PROPERTIES(DEFAULT_SIZE)                         \
    UI_CONTROL_PROPERTY("textConfig", "string", false, "\"\""),                \
        UI_CONTROL_PROPERTY("font", "string", false, "\"\""),                  \
        UI_CONTROL_PROPERTY("characterSize", "int", false, DEFAULT_SIZE),      \
        UI_CONTROL_PROPERTY("bold", "bool", false, "false"),                   \
        UI_CONTROL_PROPERTY("italic", "bool", false, "false"),                 \
        UI_CONTROL_PROPERTY("underlined", "bool", false, "false"),             \
        UI_CONTROL_PROPERTY("strikeThrough", "bool", false, "false"),          \
        UI_CONTROL_PROPERTY("slantAngle", "float", false, "0.0"),              \
        UI_CONTROL_PROPERTY("fillColor", "sf.Color", false,                    \
                            "[255,255,255,255]"),                              \
        UI_CONTROL_PROPERTY("letterSpacing", "float", false, "1.0"),           \
        UI_CONTROL_PROPERTY("lineSpacing", "float", false, "1.0"),             \
        UI_CONTROL_PROPERTY("lineAlignment", "sf.Text.LineAlignment", false,   \
                            "\"default\""),                                    \
        UI_CONTROL_PROPERTY("outlineColor", "sf.Color", false, "[0,0,0,255]"), \
        UI_CONTROL_PROPERTY("outlineThickness", "float", false, "0.0"),        \
        UI_CONTROL_PROPERTY("glowEnabled", "bool", false, "false"),            \
        UI_CONTROL_PROPERTY("glowColor", "sf.Color", false,                    \
                            "[255,255,255,0]"),                                \
        UI_CONTROL_PROPERTY("glowRadius", "float", false, "0.0"),              \
        UI_CONTROL_PROPERTY("glowIntensity", "float", false, "0.0"),           \
        UI_CONTROL_PROPERTY("gradientEnabled", "bool", false, "false"),        \
        UI_CONTROL_PROPERTY("gradientDirection",                               \
                            "Engine.TextGradientDirection", false,             \
                            "\"vertical\""),                                   \
        UI_CONTROL_PROPERTY("gradientCurve", "string", false, "\"\"")

#define LUDORK_UI_CONTROL_DEFINITIONS                                          \
    BIND_UI_CONTROL(                                                           \
        CanvasUiControlAdapterTag, "Engine.Canvas", "Engine.Canvas", "Canvas", \
        "Layout", UiChildPolicy::Multiple, UiControlSlotType::Canvas,          \
        UI_CONTROL_PROPERTY("size", "sf.Vector2u", false, "[100,100]"))        \
    BIND_UI_CONTROL(                                                           \
        ScrollBoxUiControlAdapterTag, "Engine.ScrollBox", "Engine.ScrollBox",  \
        "Scroll Box", "Layout", UiChildPolicy::Multiple,                       \
        UiControlSlotType::List,                                               \
        UI_CONTROL_PROPERTY("size", "sf.Vector2f", false, "[100.0,100.0]"),    \
        UI_CONTROL_PROPERTY("windowSkin", "string", false, "\"\""))            \
    BIND_UI_CONTROL(                                                           \
        ListViewUiControlAdapterTag, "Engine.ListView", "Engine.ListView",     \
        "List View", "Layout", UiChildPolicy::Multiple,                        \
        UiControlSlotType::List,                                               \
        UI_CONTROL_PROPERTY("size", "sf.Vector2f", false, "[100.0,100.0]"),    \
        UI_CONTROL_PROPERTY("defaultItemHeight", "int", false, "32"),          \
        UI_CONTROL_PROPERTY("fixItemHeight", "bool", false, "false"),          \
        UI_CONTROL_PROPERTY("columns", "int", false, "1"))                     \
    BIND_UI_CONTROL(                                                           \
        WindowUiControlAdapterTag, "Engine.Window", "Engine.Window", "Window", \
        "Visual", UiChildPolicy::None, UiControlSlotType::None,                \
        UI_CONTROL_PROPERTY("size", "sf.Vector2u", false, "[160,96]"),         \
        UI_CONTROL_PROPERTY("windowSkin", "string", false, "\"\""),            \
        UI_CONTROL_PROPERTY("repeated", "bool", false, "false"))               \
    BIND_UI_CONTROL(                                                           \
        RectUiControlAdapterTag, "Engine.Rect", "Engine.Rect", "Rect",         \
        "Visual", UiChildPolicy::None, UiControlSlotType::None,                \
        UI_CONTROL_PROPERTY("size", "sf.Vector2f", false, "[160.0,96.0]"),     \
        UI_CONTROL_PROPERTY("windowSkin", "string", false, "\"\""),            \
        UI_CONTROL_PROPERTY("opacityCurve", "string", false, "\"\""))          \
    BIND_UI_CONTROL(                                                           \
        SolidRectUiControlAdapterTag, "Engine.SolidRect", "Engine.SolidRect",  \
        "Solid Rect", "Visual", UiChildPolicy::None, UiControlSlotType::None,  \
        UI_CONTROL_PROPERTY("size", "sf.Vector2f", false, "[100.0,32.0]"),     \
        UI_CONTROL_PROPERTY("fillColor", "sf.Color", false,                    \
                            "[255,255,255,255]"),                              \
        UI_CONTROL_PROPERTY("outlineColor", "sf.Color", false, "[0,0,0,0]"),   \
        UI_CONTROL_PROPERTY("outlineThickness", "float", false, "0.0"))        \
    BIND_UI_CONTROL(                                                           \
        ProgressBarUiControlAdapterTag, "Engine.ProgressBar",                  \
        "Engine.ProgressBar", "Progress Bar", "Visual", UiChildPolicy::None,   \
        UiControlSlotType::None,                                               \
        UI_CONTROL_PROPERTY("size", "sf.Vector2f", false, "[100.0,12.0]"),     \
        UI_CONTROL_PROPERTY("progress", "float", false, "0.0"),                \
        UI_CONTROL_PROPERTY("backgroundColor", "sf.Color", false,              \
                            "[255,255,255,64]"),                               \
        UI_CONTROL_PROPERTY("fillColor", "sf.Color", false,                    \
                            "[255,255,255,255]"))                              \
    BIND_UI_CONTROL(                                                           \
        ImageUiControlAdapterTag, "Engine.Image", "Engine.Image", "Image",     \
        "Visual", UiChildPolicy::None, UiControlSlotType::None,                \
        UI_CONTROL_PROPERTY("texture", "string", false, "\"\""),               \
        UI_CONTROL_PROPERTY("textureRect", "sf.IntRect", false, "null"),       \
        UI_CONTROL_PROPERTY("colour", "sf.Color", false, "[255,255,255,255]")) \
    BIND_UI_CONTROL(                                                           \
        ButtonUiControlAdapterTag, "Engine.Button", "Engine.Button", "Button", \
        "Input", UiChildPolicy::None, UiControlSlotType::None,                 \
        UI_CONTROL_PROPERTY("texture", "string", false, "\"\""),               \
        UI_CONTROL_PROPERTY("textureRect", "sf.IntRect", false, "null"),       \
        UI_CONTROL_PROPERTY("colour", "sf.Color", false, "[255,255,255,255]"), \
        UI_CONTROL_PROPERTY("hoverColour", "sf.Color", false,                  \
                            "[255,255,255,255]"),                              \
        UI_CONTROL_PROPERTY("pressedColour", "sf.Color", false,                \
                            "[255,255,255,255]"))                              \
    BIND_UI_CONTROL(                                                           \
        CheckBoxUiControlAdapterTag, "Engine.CheckBox", "Engine.CheckBox",     \
        "Check Box", "Input", UiChildPolicy::None, UiControlSlotType::None,    \
        UI_CONTROL_PROPERTY("size", "sf.Vector2f", false, "[32.0,32.0]"),      \
        UI_CONTROL_PROPERTY("checked", "bool", false, "false"),                \
        UI_CONTROL_PROPERTY("windowSkin", "string", false, "\"\""),            \
        UI_CONTROL_PLAIN_TEXT_PROPERTIES("20"))                                \
    BIND_UI_CONTROL(                                                           \
        SliderUiControlAdapterTag, "Engine.Slider", "Engine.Slider", "Slider", \
        "Input", UiChildPolicy::None, UiControlSlotType::None,                 \
        UI_CONTROL_PROPERTY("size", "sf.Vector2f", false, "[64.0,8.0]"),       \
        UI_CONTROL_PROPERTY("minValue", "int", false, "0"),                    \
        UI_CONTROL_PROPERTY("maxValue", "int", false, "100"),                  \
        UI_CONTROL_PROPERTY("value", "int", false, "0"),                       \
        UI_CONTROL_PROPERTY("lineTexture", "string", false,                    \
                            "\"Assets/System/SliderLine.png\""),               \
        UI_CONTROL_PROPERTY("handleTexture", "string", false,                  \
                            "\"Assets/System/SliderHandle.png\""))             \
    BIND_UI_CONTROL(                                                           \
        DropBoxUiControlAdapterTag, "Engine.DropBox", "Engine.DropBox",        \
        "Drop Box", "Input", UiChildPolicy::None, UiControlSlotType::None,     \
        UI_CONTROL_PROPERTY("size", "sf.Vector2f", false, "[200.0,32.0]"),     \
        UI_CONTROL_PROPERTY("windowSkin", "string", false, "\"\""),            \
        UI_CONTROL_PLAIN_TEXT_PROPERTIES("20"),                                \
        UI_CONTROL_EDITOR_PROPERTY("previewText", "string", false,             \
                                   "\"Option\""))                              \
    BIND_UI_CONTROL(                                                           \
        TabViewUiControlAdapterTag, "Engine.TabView", "Engine.TabView",        \
        "Tab View", "Input", UiChildPolicy::None, UiControlSlotType::None,     \
        UI_CONTROL_PROPERTY("size", "sf.Vector2f", false, "[100.0,32.0]"),     \
        UI_CONTROL_PROPERTY("windowSkin", "string", false, "\"\""),            \
        UI_CONTROL_PLAIN_TEXT_PROPERTIES("22"),                                \
        UI_CONTROL_PROPERTY("items", "string[]", false, "[\"#TAB\"]"))         \
    BIND_UI_CONTROL(                                                           \
        FunctionalImageUiControlAdapterTag, "Engine.FunctionalImage",          \
        "Engine.FunctionalImage", "Functional Image", "Input",                 \
        UiChildPolicy::None, UiControlSlotType::None,                          \
        UI_CONTROL_PROPERTY("texture", "string", false, "\"\""),               \
        UI_CONTROL_PROPERTY("textureRect", "sf.IntRect", false, "null"),       \
        UI_CONTROL_PROPERTY("colour", "sf.Color", false, "[255,255,255,255]")) \
    BIND_UI_CONTROL(                                                           \
        CharacterViewUiControlAdapterTag, "Engine.CharacterView",              \
        "Engine.CharacterView", "Character View", "Visual",                    \
        UiChildPolicy::None, UiControlSlotType::None,                          \
        UI_CONTROL_PROPERTY("size", "sf.Vector2f", false, "[32.0,32.0]"),      \
        UI_CONTROL_PROPERTY("texture", "string", false, "\"\""),               \
        UI_CONTROL_PROPERTY("textureRect", "sf.IntRect", false, "null"),       \
        UI_CONTROL_PROPERTY("characterScale", "sf.Vector2f", false,            \
                            "[1.0,1.0]"),                                      \
        UI_CONTROL_PROPERTY("animatable", "bool", false, "true"),              \
        UI_CONTROL_PROPERTY("switchInterval", "float", false, "0.2"),          \
        UI_CONTROL_PROPERTY("shader", "string", false, "\"\""),                \
        UI_CONTROL_PROPERTY("hue", "float", false, "0.0"),                     \
        UI_CONTROL_PROPERTY("colour", "sf.Color", false, "[255,255,255,255]")) \
    BIND_UI_CONTROL(                                                           \
        FunctionalPlainTextUiControlAdapterTag, "Engine.FunctionalPlainText",  \
        "Engine.FunctionalPlainText", "Functional Plain Text", "Input",        \
        UiChildPolicy::None, UiControlSlotType::None,                          \
        UI_CONTROL_PLAIN_TEXT_PROPERTIES("22"),                                \
        UI_CONTROL_PROPERTY("text", "string", false, "\"\""),                  \
        UI_CONTROL_EDITOR_PROPERTY("previewText", "string", false, "\"\""),    \
        UI_CONTROL_PROPERTY("colour", "sf.Color", false, "[255,255,255,255]")) \
    BIND_UI_CONTROL(                                                           \
        FunctionalRichTextUiControlAdapterTag, "Engine.FunctionalRichText",    \
        "Engine.FunctionalRichText", "Functional Rich Text", "Input",          \
        UiChildPolicy::None, UiControlSlotType::None,                          \
        UI_CONTROL_PROPERTY("textConfig", "string", false, "\"\""),            \
        UI_CONTROL_PROPERTY("text", "string", false, "\"\""),                  \
        UI_CONTROL_EDITOR_PROPERTY("previewText", "string", false, "\"\""),    \
        UI_CONTROL_PROPERTY("colour", "sf.Color", false, "[255,255,255,255]")) \
    BIND_UI_CONTROL(                                                           \
        PlainTextUiControlAdapterTag, "Engine.PlainText", "Engine.PlainText",  \
        "Plain Text", "Text", UiChildPolicy::None, UiControlSlotType::None,    \
        UI_CONTROL_PLAIN_TEXT_PROPERTIES("22"),                                \
        UI_CONTROL_PROPERTY("text", "string", false, "\"\""),                  \
        UI_CONTROL_EDITOR_PROPERTY("previewText", "string", false, "\"\""),    \
        UI_CONTROL_PROPERTY("colour", "sf.Color", false, "[255,255,255,255]")) \
    BIND_UI_CONTROL(                                                           \
        RichTextUiControlAdapterTag, "Engine.RichText", "Engine.RichText",     \
        "Rich Text", "Text", UiChildPolicy::None, UiControlSlotType::None,     \
        UI_CONTROL_PROPERTY("textConfig", "string", false, "\"\""),            \
        UI_CONTROL_PROPERTY("text", "string", false, "\"\""),                  \
        UI_CONTROL_EDITOR_PROPERTY("previewText", "string", false, "\"\""),    \
        UI_CONTROL_PROPERTY("colour", "sf.Color", false, "[255,255,255,255]"))

#undef BIND_UI_CONTROL
#define BIND_UI_CONTROL(TAG, CONTROL_ID, ADAPTER, DISPLAY_NAME, CATEGORY, \
                        CHILD_POLICY, SLOT_TYPE, ...)                     \
    struct TAG {};                                                        \
    template <>                                                           \
    struct UiControlAdapterTraits<TAG> {                                  \
        inline static constexpr auto properties =                         \
            std::array{UI_CONTROL_COMMON_PROPERTIES, __VA_ARGS__};        \
        inline static constexpr UiControlAdapterDescriptor descriptor{    \
            CONTROL_ID,                                                   \
            ADAPTER,                                                      \
            DISPLAY_NAME,                                                 \
            CATEGORY,                                                     \
            CHILD_POLICY,                                                 \
            SLOT_TYPE,                                                    \
            std::span<const UiControlPropertyDescriptor>{properties},     \
        };                                                                \
    };

LUDORK_UI_CONTROL_DEFINITIONS

#undef BIND_UI_CONTROL
#define BIND_UI_CONTROL(TAG, CONTROL_ID, ADAPTER, DISPLAY_NAME, CATEGORY, \
                        CHILD_POLICY, SLOT_TYPE, ...)                     \
    UiControlAdapterTraits<TAG>::descriptor,

inline constexpr auto uiControlAdapterDescriptorTable =
    std::array{LUDORK_UI_CONTROL_DEFINITIONS};

#undef BIND_UI_CONTROL
#define BIND_UI_CONTROL(...)
#undef LUDORK_UI_CONTROL_DEFINITIONS
#undef UI_CONTROL_COMMON_PROPERTIES
#undef UI_CONTROL_PLAIN_TEXT_PROPERTIES
#undef UI_CONTROL_EDITOR_PROPERTY
#undef UI_CONTROL_PROPERTY
#undef UI_CONTROL_COMMON_PROPERTY
