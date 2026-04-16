#pragma once

#include <string>

#include <TGUI/AllWidgets.hpp>

#include "UI/FocusStyle.hpp"

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
    const tgui::Color borderColor(0, 0, 0);

    auto renderer = button->getRenderer();
    renderer->setBorders({1});
    renderer->setBorderColor(borderColor);
    renderer->setBackgroundColor(backgroundColor);
    renderer->setTextColor(textColor);
    renderer->setBorderColorHover(borderColor);
    renderer->setBackgroundColorHover(backgroundColor);
    renderer->setTextColorHover(textColor);
    renderer->setBorderColorDisabled(borderColor);
    renderer->setBackgroundColorDisabled(backgroundColor);
    renderer->setTextColorDisabled(textColor);
    renderer->setBorderColorFocused(borderColor);
    renderer->setBackgroundColorFocused(backgroundColor);
    renderer->setTextColorFocused(textColor);
    renderer->setBorderColorDownFocused(borderColor);
    renderer->setBackgroundColorDownFocused(backgroundColor);
    renderer->setTextColorDownFocused(textColor);

    FocusStyle::neutralizeButtonFocus(button);
}

} // namespace ActionButtonStyle