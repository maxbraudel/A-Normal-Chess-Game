#pragma once
#include <TGUI/Backend/SFML-Graphics.hpp>
#include <TGUI/AllWidgets.hpp>

#include <optional>

#include "Systems/EconomySystem.hpp"

class Building;

class BuildingPanel {
public:
    void init(const tgui::Panel::Ptr& parent);
    void show(const Building& building, const std::optional<ResourceIncomeBreakdown>& resourceIncome = std::nullopt);
    void hide();

private:
    tgui::Panel::Ptr m_panel;
    tgui::Label::Ptr m_ownerLabel;
    tgui::Label::Ptr m_cellsLabel;
    tgui::Label::Ptr m_typeLabel;
    tgui::Label::Ptr m_hpLabel;
    tgui::Label::Ptr m_statusLabel;
    tgui::Label::Ptr m_resourceSectionLabel;
    tgui::Label::Ptr m_whiteIncomeLabel;
    tgui::Label::Ptr m_blackIncomeLabel;
};
