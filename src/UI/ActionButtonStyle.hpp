#pragma once

#include <string>

#include <TGUI/AllWidgets.hpp>

namespace ActionButtonStyle {

inline void applySelectableState(const tgui::Button::Ptr& button,
                                 const std::string& baseLabel,
                                 bool selected,
                                 bool enabled,
                                 bool prefixSelected = true) {
    if (!button) {
        return;
    }

    button->setText(selected && prefixSelected ? "> " + baseLabel : baseLabel);
    button->setEnabled(enabled);

    const tgui::Color backgroundColor = selected
        ? (enabled ? tgui::Color(124, 96, 28) : tgui::Color(96, 74, 22))
        : (enabled ? tgui::Color(255, 255, 255) : tgui::Color(220, 220, 220));
    const tgui::Color textColor = selected
        ? tgui::Color::White
        : (enabled ? tgui::Color::Black : tgui::Color(100, 100, 100));

    auto renderer = button->getRenderer();
    renderer->setBackgroundColor(backgroundColor);
    renderer->setTextColor(textColor);
    renderer->setBackgroundColorHover(backgroundColor);
    renderer->setTextColorHover(textColor);
    renderer->setBackgroundColorDisabled(backgroundColor);
    renderer->setTextColorDisabled(textColor);
}

} // namespace ActionButtonStyle