#pragma once
#include <TGUI/Backend/SFML-Graphics.hpp>
#include <TGUI/AllWidgets.hpp>

#include <functional>

#include <optional>
#include <string>

#include "Systems/EconomySystem.hpp"
#include "Systems/PublicBuildingOccupation.hpp"

class Building;

class BuildingPanel {
public:
    void init(const tgui::Panel::Ptr& parent);
    void show(const Building& building,
              bool allowCancelConstruction,
              const std::optional<ResourceIncomeBreakdown>& resourceIncome = std::nullopt,
              const std::optional<PublicBuildingOccupationState>& publicOccupation = std::nullopt,
              const std::string& title = "");
    void hide();

    void setOnCancelConstruction(std::function<void(int buildingId)> callback);

private:
    tgui::Panel::Ptr m_panel;
    tgui::Label::Ptr m_titleLabel;
    tgui::Label::Ptr m_ownerLabel;
    tgui::Label::Ptr m_cellsLabel;
    tgui::Label::Ptr m_typeLabel;
    tgui::Label::Ptr m_hpLabel;
    tgui::Label::Ptr m_statusLabel;
    tgui::Label::Ptr m_resourceSectionLabel;
    tgui::Label::Ptr m_whiteIncomeLabel;
    tgui::Label::Ptr m_blackIncomeLabel;
    tgui::Button::Ptr m_cancelConstructionBtn;
    std::function<void(int)> m_onCancelConstruction;
    int m_currentBuildingId = -1;
};
