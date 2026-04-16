#pragma once

#include <TGUI/AllWidgets.hpp>

namespace FocusStyle {

inline void neutralizeButtonFocus(const tgui::Button::Ptr& button) {
    if (!button) {
        return;
    }

    auto renderer = button->getRenderer();
    renderer->setTextColorFocused(renderer->getTextColor());
    renderer->setTextColorDownFocused(renderer->getTextColorDown());
    renderer->setBackgroundColorFocused(renderer->getBackgroundColor());
    renderer->setBackgroundColorDownFocused(renderer->getBackgroundColorDown());
    renderer->setBorderColorFocused(renderer->getBorderColor());
    renderer->setBorderColorDownFocused(renderer->getBorderColorDown());
}

inline void neutralizeEditBoxFocus(const tgui::EditBox::Ptr& editBox) {
    if (!editBox) {
        return;
    }

    auto renderer = editBox->getRenderer();
    renderer->setTextColorFocused(renderer->getTextColor());
    renderer->setBackgroundColorFocused(renderer->getBackgroundColor());
    renderer->setBorderColorFocused(renderer->getBorderColor());
}

inline void neutralizeCheckBoxFocus(const tgui::CheckBox::Ptr& checkBox) {
    if (!checkBox) {
        return;
    }

    auto renderer = checkBox->getRenderer();
    renderer->setBorderColorFocused(renderer->getBorderColor());
    renderer->setBorderColorCheckedFocused(renderer->getBorderColorChecked());
}

} // namespace FocusStyle