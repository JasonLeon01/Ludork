#pragma once

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
    enum class TextKind {
        None,
        Plain,
        Rich,
    };

    std::string_view id;
    std::string_view displayName;
    std::string_view type;
    std::string_view defaultJson;
    bool required;
    bool editorOnly;
    bool adapterProperty;
    TextKind textKind = TextKind::None;
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

#define UI_CONTROL_COMMON_PROPERTY(ID, DISPLAY_NAME, TYPE, REQUIRED, \
                                   DEFAULT_JSON)                     \
    UiControlPropertyDescriptor {                                    \
        ID, DISPLAY_NAME, TYPE, DEFAULT_JSON, REQUIRED, false, false \
    }

#define UI_CONTROL_PROPERTY(ID, DISPLAY_NAME, TYPE, REQUIRED, DEFAULT_JSON) \
    UiControlPropertyDescriptor {                                           \
        ID, DISPLAY_NAME, TYPE, DEFAULT_JSON, REQUIRED, false, true         \
    }

#define UI_CONTROL_EDITOR_PROPERTY(ID, DISPLAY_NAME, TYPE, REQUIRED, \
                                   DEFAULT_JSON)                     \
    UiControlPropertyDescriptor {                                    \
        ID, DISPLAY_NAME, TYPE, DEFAULT_JSON, REQUIRED, true, false  \
    }

#define UI_CONTROL_TEXT_PROPERTY(KIND, ID, DISPLAY_NAME, TYPE, REQUIRED,  \
                                 DEFAULT_JSON)                            \
    UiControlPropertyDescriptor {                                         \
        ID, DISPLAY_NAME, TYPE, DEFAULT_JSON, REQUIRED, false, true, KIND \
    }

#define UI_CONTROL_COMMON_PROPERTIES                                         \
    UI_CONTROL_COMMON_PROPERTY("visible", "Visible", "bool", false, "true"), \
        UI_CONTROL_COMMON_PROPERTY("rotation", "Rotation", "float", false,   \
                                   "0.0"),                                   \
        UI_CONTROL_COMMON_PROPERTY("scale", "Scale", "sf.Vector2f", false,   \
                                   "[1.0,1.0]"),                             \
        UI_CONTROL_COMMON_PROPERTY("origin", "Origin", "sf.Vector2f", false, \
                                   "[0.0,0.0]")

#define UI_CONTROL_PLAIN_TEXT_PROPERTIES(DEFAULT_SIZE)                         \
    UI_CONTROL_TEXT_PROPERTY(UiControlPropertyDescriptor::TextKind::Plain,     \
                             "textConfig", "Text Config", "string", false,     \
                             "\"\""),                                          \
        UI_CONTROL_PROPERTY("font", "Font", "string", false, "\"\""),          \
        UI_CONTROL_PROPERTY("characterSize", "Character Size", "int", false,   \
                            DEFAULT_SIZE),                                     \
        UI_CONTROL_PROPERTY("bold", "Bold", "bool", false, "false"),           \
        UI_CONTROL_PROPERTY("italic", "Italic", "bool", false, "false"),       \
        UI_CONTROL_PROPERTY("underlined", "Underlined", "bool", false,         \
                            "false"),                                          \
        UI_CONTROL_PROPERTY("strikeThrough", "Strike Through", "bool", false,  \
                            "false"),                                          \
        UI_CONTROL_PROPERTY("slantAngle", "Slant Angle", "float", false,       \
                            "0.0"),                                            \
        UI_CONTROL_PROPERTY("fillColor", "Fill Color", "sf.Color", false,      \
                            "[255,255,255,255]"),                              \
        UI_CONTROL_PROPERTY("letterSpacing", "Letter Spacing", "float", false, \
                            "1.0"),                                            \
        UI_CONTROL_PROPERTY("lineSpacing", "Line Spacing", "float", false,     \
                            "1.0"),                                            \
        UI_CONTROL_PROPERTY("lineAlignment", "Line Alignment",                 \
                            "sf.Text.LineAlignment", false, "\"default\""),    \
        UI_CONTROL_PROPERTY("outlineColor", "Outline Color", "sf.Color",       \
                            false, "[0,0,0,255]"),                             \
        UI_CONTROL_PROPERTY("outlineThickness", "Outline Thickness", "float",  \
                            false, "0.0"),                                     \
        UI_CONTROL_PROPERTY("glowEnabled", "Glow Enabled", "bool", false,      \
                            "false"),                                          \
        UI_CONTROL_PROPERTY("glowColor", "Glow Color", "sf.Color", false,      \
                            "[255,255,255,0]"),                                \
        UI_CONTROL_PROPERTY("glowRadius", "Glow Radius", "float", false,       \
                            "0.0"),                                            \
        UI_CONTROL_PROPERTY("glowIntensity", "Glow Intensity", "float", false, \
                            "0.0"),                                            \
        UI_CONTROL_PROPERTY("gradientEnabled", "Gradient Enabled", "bool",     \
                            false, "false"),                                   \
        UI_CONTROL_PROPERTY("gradientDirection", "Gradient Direction",         \
                            "Engine.TextGradientDirection", false,             \
                            "\"vertical\""),                                   \
        UI_CONTROL_PROPERTY("gradientCurve", "Gradient Curve", "string",       \
                            false, "\"\"")

#define UI_CONTROL_RICH_TEXT_PROPERTIES                                     \
    UI_CONTROL_TEXT_PROPERTY(UiControlPropertyDescriptor::TextKind::Rich,   \
                             "textConfig", "Text Config", "string", false,  \
                             "\"\""),                                       \
        UI_CONTROL_PROPERTY("text", "Text", "string", false, "\"\""),       \
        UI_CONTROL_EDITOR_PROPERTY("previewText", "Preview Text", "string", \
                                   false, "\"\""),                          \
        UI_CONTROL_PROPERTY("colour", "Colour", "sf.Color", false,          \
                            "[255,255,255,255]")

#define LUDORK_UI_CONTROL_DEFINITIONS                                          \
    BIND_UI_CONTROL(CanvasUiControlAdapterTag, "Engine.Canvas",                \
                    "Engine.Canvas", "Canvas", "Layout",                       \
                    UiChildPolicy::Multiple, UiControlSlotType::Canvas,        \
                    UI_CONTROL_PROPERTY("size", "Size", "sf.Vector2u", false,  \
                                        "[100,100]"))                          \
    BIND_UI_CONTROL(ScrollBoxUiControlAdapterTag, "Engine.ScrollBox",          \
                    "Engine.ScrollBox", "Scroll Box", "Layout",                \
                    UiChildPolicy::Multiple, UiControlSlotType::List,          \
                    UI_CONTROL_PROPERTY("size", "Size", "sf.Vector2f", false,  \
                                        "[100.0,100.0]"),                      \
                    UI_CONTROL_PROPERTY("windowSkin", "Window Skin", "string", \
                                        false, "\"\""))                        \
    BIND_UI_CONTROL(                                                           \
        ListViewUiControlAdapterTag, "Engine.ListView", "Engine.ListView",     \
        "List View", "Layout", UiChildPolicy::Multiple,                        \
        UiControlSlotType::List,                                               \
        UI_CONTROL_PROPERTY("size", "Size", "sf.Vector2f", false,              \
                            "[100.0,100.0]"),                                  \
        UI_CONTROL_PROPERTY("defaultItemHeight", "Default Item Height", "int", \
                            false, "32"),                                      \
        UI_CONTROL_PROPERTY("fixItemHeight", "Fix Item Height", "bool", false, \
                            "false"),                                          \
        UI_CONTROL_PROPERTY("columns", "Columns", "int", false, "1"))          \
    BIND_UI_CONTROL(WrapBoxUiControlAdapterTag, "Engine.WrapBox",              \
                    "Engine.WrapBox", "Wrap Box", "Layout",                    \
                    UiChildPolicy::Single, UiControlSlotType::List,            \
                    UI_CONTROL_PROPERTY("size", "Size", "sf.Vector2f", false,  \
                                        "[100.0,100.0]"),                      \
                    UI_CONTROL_PROPERTY("count", "Count", "int", false, "1"),  \
                    UI_CONTROL_PROPERTY("spacing", "Spacing", "sf.Vector2f",   \
                                        false, "[0.0,0.0]"))                   \
    BIND_UI_CONTROL(                                                           \
        WindowUiControlAdapterTag, "Engine.Window", "Engine.Window", "Window", \
        "Visual", UiChildPolicy::None, UiControlSlotType::None,                \
        UI_CONTROL_PROPERTY("size", "Size", "sf.Vector2u", false, "[160,96]"), \
        UI_CONTROL_PROPERTY("windowSkin", "Window Skin", "string", false,      \
                            "\"\""),                                           \
        UI_CONTROL_PROPERTY("repeated", "Repeated", "bool", false, "false"))   \
    BIND_UI_CONTROL(RectUiControlAdapterTag, "Engine.Rect", "Engine.Rect",     \
                    "Rect", "Visual", UiChildPolicy::None,                     \
                    UiControlSlotType::None,                                   \
                    UI_CONTROL_PROPERTY("size", "Size", "sf.Vector2f", false,  \
                                        "[160.0,96.0]"),                       \
                    UI_CONTROL_PROPERTY("windowSkin", "Window Skin", "string", \
                                        false, "\"\""),                        \
                    UI_CONTROL_PROPERTY("opacityCurve", "Opacity Curve",       \
                                        "string", false, "\"\""))              \
    BIND_UI_CONTROL(                                                           \
        SolidRectUiControlAdapterTag, "Engine.SolidRect", "Engine.SolidRect",  \
        "Solid Rect", "Visual", UiChildPolicy::None, UiControlSlotType::None,  \
        UI_CONTROL_PROPERTY("size", "Size", "sf.Vector2f", false,              \
                            "[100.0,32.0]"),                                   \
        UI_CONTROL_PROPERTY("fillColor", "Fill Color", "sf.Color", false,      \
                            "[255,255,255,255]"),                              \
        UI_CONTROL_PROPERTY("outlineColor", "Outline Color", "sf.Color",       \
                            false, "[0,0,0,0]"),                               \
        UI_CONTROL_PROPERTY("outlineThickness", "Outline Thickness", "float",  \
                            false, "0.0"))                                     \
    BIND_UI_CONTROL(                                                           \
        ProgressBarUiControlAdapterTag, "Engine.ProgressBar",                  \
        "Engine.ProgressBar", "Progress Bar", "Visual", UiChildPolicy::None,   \
        UiControlSlotType::None,                                               \
        UI_CONTROL_PROPERTY("size", "Size", "sf.Vector2f", false,              \
                            "[100.0,12.0]"),                                   \
        UI_CONTROL_PROPERTY("progress", "Progress", "float", false, "0.0"),    \
        UI_CONTROL_PROPERTY("backgroundTexture", "Background Texture",         \
                            "string", false, "\"\""),                          \
        UI_CONTROL_PROPERTY("fillTexture", "Fill Texture", "string", false,    \
                            "\"\""),                                           \
        UI_CONTROL_PROPERTY("backgroundTextureRect",                           \
                            "Background Texture Rect", "sf.IntRect", false,    \
                            "null"),                                           \
        UI_CONTROL_PROPERTY("fillTextureRect", "Fill Texture Rect",            \
                            "sf.IntRect", false, "null"),                      \
        UI_CONTROL_PROPERTY("backgroundColor", "Background Color", "sf.Color", \
                            false, "[255,255,255,64]"),                        \
        UI_CONTROL_PROPERTY("fillColor", "Fill Color", "sf.Color", false,      \
                            "[255,255,255,255]"))                              \
    BIND_UI_CONTROL(                                                           \
        ImageUiControlAdapterTag, "Engine.Image", "Engine.Image", "Image",     \
        "Visual", UiChildPolicy::None, UiControlSlotType::None,                \
        UI_CONTROL_PROPERTY("drawAs", "Draw As", "Engine.ImageDrawAs", false,  \
                            "\"Image\""),                                      \
        UI_CONTROL_PROPERTY("texture", "Texture", "string", false, "\"\""),    \
        UI_CONTROL_PROPERTY("textureRect", "Texture Rect", "sf.IntRect",       \
                            false, "null"),                                    \
        UI_CONTROL_PROPERTY("colour", "Colour", "sf.Color", false,             \
                            "[255,255,255,255]"))                              \
    BIND_UI_CONTROL(                                                           \
        ButtonUiControlAdapterTag, "Engine.Button", "Engine.Button", "Button", \
        "Input", UiChildPolicy::None, UiControlSlotType::None,                 \
        UI_CONTROL_PROPERTY("texture", "Texture", "string", false, "\"\""),    \
        UI_CONTROL_PROPERTY("textureRect", "Texture Rect", "sf.IntRect",       \
                            false, "null"),                                    \
        UI_CONTROL_PROPERTY("colour", "Colour", "sf.Color", false,             \
                            "[255,255,255,255]"),                              \
        UI_CONTROL_PROPERTY("hoverColour", "Hover Colour", "sf.Color", false,  \
                            "[255,255,255,255]"),                              \
        UI_CONTROL_PROPERTY("pressedColour", "Pressed Colour", "sf.Color",     \
                            false, "[255,255,255,255]"),                       \
        UI_CONTROL_PROPERTY("gamepadButton", "Gamepad Button", "string",       \
                            false, "\"\""),                                    \
        UI_CONTROL_PROPERTY("gamepadLongPress", "Gamepad Long Press", "bool",  \
                            false, "false"))                                   \
    BIND_UI_CONTROL(                                                           \
        CheckBoxUiControlAdapterTag, "Engine.CheckBox", "Engine.CheckBox",     \
        "Check Box", "Input", UiChildPolicy::None, UiControlSlotType::None,    \
        UI_CONTROL_PROPERTY("size", "Size", "sf.Vector2f", false,              \
                            "[32.0,32.0]"),                                    \
        UI_CONTROL_PROPERTY("checked", "Checked", "bool", false, "false"),     \
        UI_CONTROL_PROPERTY("windowSkin", "Window Skin", "string", false,      \
                            "\"\""),                                           \
        UI_CONTROL_PLAIN_TEXT_PROPERTIES("20"))                                \
    BIND_UI_CONTROL(                                                           \
        SliderUiControlAdapterTag, "Engine.Slider", "Engine.Slider", "Slider", \
        "Input", UiChildPolicy::None, UiControlSlotType::None,                 \
        UI_CONTROL_PROPERTY("size", "Size", "sf.Vector2f", false,              \
                            "[64.0,8.0]"),                                     \
        UI_CONTROL_PROPERTY("minValue", "Min Value", "int", false, "0"),       \
        UI_CONTROL_PROPERTY("maxValue", "Max Value", "int", false, "100"),     \
        UI_CONTROL_PROPERTY("value", "Value", "int", false, "0"),              \
        UI_CONTROL_PROPERTY("lineTexture", "Line Texture", "string", false,    \
                            "\"/Game/Assets/System/SliderLine.png\""),         \
        UI_CONTROL_PROPERTY("handleTexture", "Handle Texture", "string",       \
                            false,                                             \
                            "\"/Game/Assets/System/SliderHandle.png\""))       \
    BIND_UI_CONTROL(DropBoxUiControlAdapterTag, "Engine.DropBox",              \
                    "Engine.DropBox", "Drop Box", "Input",                     \
                    UiChildPolicy::None, UiControlSlotType::None,              \
                    UI_CONTROL_PROPERTY("size", "Size", "sf.Vector2f", false,  \
                                        "[200.0,32.0]"),                       \
                    UI_CONTROL_PROPERTY("windowSkin", "Window Skin", "string", \
                                        false, "\"\""),                        \
                    UI_CONTROL_PLAIN_TEXT_PROPERTIES("20"),                    \
                    UI_CONTROL_EDITOR_PROPERTY("previewText", "Preview Text",  \
                                               "string", false, "\"Option\"")) \
    BIND_UI_CONTROL(                                                           \
        TextBoxUiControlAdapterTag, "Engine.TextBox", "Engine.TextBox",        \
        "Text Box", "Input", UiChildPolicy::None, UiControlSlotType::None,     \
        UI_CONTROL_PROPERTY("size", "Size", "sf.Vector2f", false,              \
                            "[240.0,40.0]"),                                   \
        UI_CONTROL_PROPERTY("windowSkin", "Window Skin", "string", false,      \
                            "\"\""),                                           \
        UI_CONTROL_PLAIN_TEXT_PROPERTIES("22"),                                \
        UI_CONTROL_PROPERTY("text", "Text", "string", false, "\"\""),          \
        UI_CONTROL_EDITOR_PROPERTY("previewText", "Preview Text", "string",    \
                                   false, "\"\""))                             \
    BIND_UI_CONTROL(TabViewUiControlAdapterTag, "Engine.TabView",              \
                    "Engine.TabView", "Tab View", "Input",                     \
                    UiChildPolicy::None, UiControlSlotType::None,              \
                    UI_CONTROL_PROPERTY("size", "Size", "sf.Vector2f", false,  \
                                        "[100.0,32.0]"),                       \
                    UI_CONTROL_PROPERTY("windowSkin", "Window Skin", "string", \
                                        false, "\"\""),                        \
                    UI_CONTROL_PLAIN_TEXT_PROPERTIES("22"),                    \
                    UI_CONTROL_PROPERTY("items", "Items", "string[]", false,   \
                                        "[\"#TAB\"]"))                         \
    BIND_UI_CONTROL(GamepadHintBarUiControlAdapterTag,                         \
                    "Engine.GamepadHintBar", "Engine.GamepadHintBar",          \
                    "Gamepad Hint Bar", "Input", UiChildPolicy::None,          \
                    UiControlSlotType::None,                                   \
                    UI_CONTROL_PROPERTY("size", "Size", "sf.Vector2f", false,  \
                                        "[200.0,16.0]"),                       \
                    UI_CONTROL_PLAIN_TEXT_PROPERTIES("12"))                    \
    BIND_UI_CONTROL(                                                           \
        FunctionalImageUiControlAdapterTag, "Engine.FunctionalImage",          \
        "Engine.FunctionalImage", "Functional Image", "Input",                 \
        UiChildPolicy::None, UiControlSlotType::None,                          \
        UI_CONTROL_PROPERTY("drawAs", "Draw As", "Engine.ImageDrawAs", false,  \
                            "\"Image\""),                                      \
        UI_CONTROL_PROPERTY("texture", "Texture", "string", false, "\"\""),    \
        UI_CONTROL_PROPERTY("textureRect", "Texture Rect", "sf.IntRect",       \
                            false, "null"),                                    \
        UI_CONTROL_PROPERTY("colour", "Colour", "sf.Color", false,             \
                            "[255,255,255,255]"))                              \
    BIND_UI_CONTROL(                                                           \
        CharacterViewUiControlAdapterTag, "Engine.CharacterView",              \
        "Engine.CharacterView", "Character View", "Visual",                    \
        UiChildPolicy::None, UiControlSlotType::None,                          \
        UI_CONTROL_PROPERTY("size", "Size", "sf.Vector2f", false,              \
                            "[32.0,32.0]"),                                    \
        UI_CONTROL_PROPERTY("texture", "Texture", "string", false, "\"\""),    \
        UI_CONTROL_PROPERTY("textureRect", "Texture Rect", "sf.IntRect",       \
                            false, "null"),                                    \
        UI_CONTROL_PROPERTY("characterScale", "Character Scale",               \
                            "sf.Vector2f", false, "[1.0,1.0]"),                \
        UI_CONTROL_PROPERTY("animatable", "Animatable", "bool", false,         \
                            "true"),                                           \
        UI_CONTROL_PROPERTY("switchInterval", "Switch Interval", "float",      \
                            false, "0.2"),                                     \
        UI_CONTROL_PROPERTY("shader", "Shader", "string", false, "\"\""),      \
        UI_CONTROL_PROPERTY("hue", "Hue", "float", false, "0.0"),              \
        UI_CONTROL_PROPERTY("colour", "Colour", "sf.Color", false,             \
                            "[255,255,255,255]"))                              \
    BIND_UI_CONTROL(                                                           \
        EmitterViewUiControlAdapterTag, "Engine.EmitterView",                  \
        "Engine.EmitterView", "Emitter View", "Visual", UiChildPolicy::None,   \
        UiControlSlotType::None,                                               \
        UI_CONTROL_PROPERTY("particle", "Particle", "string", false, "\"\""),  \
        UI_CONTROL_PROPERTY("size", "Size", "sf.Vector2f", false,              \
                            "[100.0,100.0]"),                                  \
        UI_CONTROL_PROPERTY("anchor", "Anchor", "sf.Vector2f", false,          \
                            "[0.5,0.5]"),                                      \
        UI_CONTROL_PROPERTY("autoPlay", "Auto Play", "bool", false, "true"))   \
    BIND_UI_CONTROL(                                                           \
        FunctionalPlainTextUiControlAdapterTag, "Engine.FunctionalPlainText",  \
        "Engine.FunctionalPlainText", "Functional Plain Text", "Input",        \
        UiChildPolicy::None, UiControlSlotType::None,                          \
        UI_CONTROL_PLAIN_TEXT_PROPERTIES("22"),                                \
        UI_CONTROL_PROPERTY("text", "Text", "string", false, "\"\""),          \
        UI_CONTROL_EDITOR_PROPERTY("previewText", "Preview Text", "string",    \
                                   false, "\"\""),                             \
        UI_CONTROL_PROPERTY("colour", "Colour", "sf.Color", false,             \
                            "[255,255,255,255]"))                              \
    BIND_UI_CONTROL(FunctionalRichTextUiControlAdapterTag,                     \
                    "Engine.FunctionalRichText", "Engine.FunctionalRichText",  \
                    "Functional Rich Text", "Input", UiChildPolicy::None,      \
                    UiControlSlotType::None, UI_CONTROL_RICH_TEXT_PROPERTIES)  \
    BIND_UI_CONTROL(                                                           \
        PlainTextUiControlAdapterTag, "Engine.PlainText", "Engine.PlainText",  \
        "Plain Text", "Text", UiChildPolicy::None, UiControlSlotType::None,    \
        UI_CONTROL_PLAIN_TEXT_PROPERTIES("22"),                                \
        UI_CONTROL_PROPERTY("text", "Text", "string", false, "\"\""),          \
        UI_CONTROL_EDITOR_PROPERTY("previewText", "Preview Text", "string",    \
                                   false, "\"\""),                             \
        UI_CONTROL_PROPERTY("colour", "Colour", "sf.Color", false,             \
                            "[255,255,255,255]"))                              \
    BIND_UI_CONTROL(RichTextUiControlAdapterTag, "Engine.RichText",            \
                    "Engine.RichText", "Rich Text", "Text",                    \
                    UiChildPolicy::None, UiControlSlotType::None,              \
                    UI_CONTROL_RICH_TEXT_PROPERTIES)

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

inline constexpr auto uiControlCommonPropertyDescriptors =
    std::array{UI_CONTROL_COMMON_PROPERTIES};

#undef BIND_UI_CONTROL
#define BIND_UI_CONTROL(...)
#undef LUDORK_UI_CONTROL_DEFINITIONS
#undef UI_CONTROL_COMMON_PROPERTIES
#undef UI_CONTROL_PLAIN_TEXT_PROPERTIES
#undef UI_CONTROL_RICH_TEXT_PROPERTIES
#undef UI_CONTROL_EDITOR_PROPERTY
#undef UI_CONTROL_PROPERTY
#undef UI_CONTROL_COMMON_PROPERTY
#undef UI_CONTROL_TEXT_PROPERTY
