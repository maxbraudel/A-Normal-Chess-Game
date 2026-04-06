#pragma once

#include <TGUI/Backend/SFML-Graphics.hpp>
#include <TGUI/AllWidgets.hpp>
#include <string>

enum class HUDAnchor {
    TopLeft,
    TopCenter,
    TopRight,
    MiddleLeft,
    MiddleRight,
    BottomLeft,
    BottomCenter,
    BottomRight
};

namespace HUDLayout {

inline constexpr float kComponentWidth = 140.f;
inline constexpr float kComponentHeight = 36.f;
inline constexpr float kComponentGap = 8.f;

inline tgui::Vector2f stackSize(int count) {
    if (count <= 0) {
        return {0.f, 0.f};
    }
    return {
        count * kComponentWidth + (count - 1) * kComponentGap,
        kComponentHeight
    };
}

inline tgui::Layout2d anchorPosition(HUDAnchor anchor, int count) {
    const auto size = stackSize(count);
    const std::string width = std::to_string(size.x);
    const std::string height = std::to_string(size.y);
    const auto layout = [](const std::string& x, const std::string& y) {
        return tgui::Layout2d{tgui::Layout{x}, tgui::Layout{y}};
    };

    switch (anchor) {
        case HUDAnchor::TopLeft:
            return layout("0", "0");
        case HUDAnchor::TopCenter:
            return layout("(&.width - " + width + ") / 2", "0");
        case HUDAnchor::TopRight:
            return layout("&.width - " + width, "0");
        case HUDAnchor::MiddleLeft:
            return layout("0", "(&.height - " + height + ") / 2");
        case HUDAnchor::MiddleRight:
            return layout("&.width - " + width, "(&.height - " + height + ") / 2");
        case HUDAnchor::BottomLeft:
            return layout("0", "&.height - " + height);
        case HUDAnchor::BottomCenter:
            return layout("(&.width - " + width + ") / 2", "&.height - " + height);
        case HUDAnchor::BottomRight:
            return layout("&.width - " + width, "&.height - " + height);
    }

    return layout("0", "0");
}

inline void placeStackChild(const tgui::Widget::Ptr& widget, int index) {
    widget->setPosition({index * (kComponentWidth + kComponentGap), 0});
    widget->setSize({kComponentWidth, kComponentHeight});
}

inline void styleHudButton(const tgui::Button::Ptr& button) {
    button->setSize({kComponentWidth, kComponentHeight});
    button->setTextSize(16);
}

inline void styleHudIndicator(const tgui::Label::Ptr& label, const tgui::Color& textColor) {
    label->setAutoSize(false);
    label->setSize({kComponentWidth, kComponentHeight});
    label->setTextSize(16);
    label->setHorizontalAlignment(tgui::HorizontalAlignment::Center);
    label->setVerticalAlignment(tgui::VerticalAlignment::Center);
    label->getRenderer()->setTextColor(textColor);
}

inline void makeTransparentPanel(const tgui::Panel::Ptr& panel) {
    panel->getRenderer()->setBackgroundColor(tgui::Color(0, 0, 0, 0));
    panel->getRenderer()->setBorders(0);
}

} // namespace HUDLayout