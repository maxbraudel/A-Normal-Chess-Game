#include "UI/MapObjectPanel.hpp"

#include "Objects/MapObject.hpp"
#include "UI/HUDLayout.hpp"

namespace {

std::string objectStatusText(const MapObject& object) {
    switch (object.type) {
        case MapObjectType::Chest:
            return "Status: Closed chest";
    }

    return "Status: Unknown";
}

std::string objectHintText(const MapObject& object) {
    switch (object.type) {
        case MapObjectType::Chest:
            return "Move a piece onto this chest to reveal and collect its hidden reward.";
    }

    return "No interaction hint available.";
}

} // namespace

void MapObjectPanel::init(const tgui::Panel::Ptr& parent) {
    m_panel = tgui::Panel::create({"&.width", "&.height"});
    HUDLayout::styleEmbeddedPanel(m_panel);
    parent->add(m_panel);

    m_titleLabel = tgui::Label::create("");
    HUDLayout::placeSidebarPanelTitle(m_titleLabel);
    m_panel->add(m_titleLabel);

    m_typeLabel = tgui::Label::create("Type: Chest");
    HUDLayout::placeSidebarPanelBodyLabel(m_typeLabel, 0);
    m_panel->add(m_typeLabel);

    m_positionLabel = tgui::Label::create("Cell: 0, 0");
    HUDLayout::placeSidebarPanelBodyLabel(m_positionLabel, 1);
    m_panel->add(m_positionLabel);

    m_statusLabel = tgui::Label::create("Status: Closed chest");
    HUDLayout::placeSidebarPanelBodyLabel(m_statusLabel, 2);
    m_panel->add(m_statusLabel);

    m_hintLabel = tgui::Label::create("");
    m_hintLabel->setPosition({HUDLayout::kSidebarPanelInset, 146});
    m_hintLabel->setSize({HUDLayout::kSidebarPanelBodyWidth, 96});
    HUDLayout::styleSidebarHint(m_hintLabel);
    m_panel->add(m_hintLabel);

    m_panel->setVisible(false);
}

void MapObjectPanel::show(const MapObject& object, const std::string& title) {
    if (!m_panel) {
        return;
    }

    m_panel->moveToFront();
    if (m_titleLabel) {
        m_titleLabel->setText(title);
    }
    m_typeLabel->setText(std::string{"Type: "} + mapObjectTypeLabel(object.type));
    m_positionLabel->setText(
        "Cell: " + std::to_string(object.position.x) + ", " + std::to_string(object.position.y));
    m_statusLabel->setText(objectStatusText(object));
    m_hintLabel->setText(objectHintText(object));
    m_panel->setVisible(true);
}

void MapObjectPanel::hide() {
    if (m_panel) {
        m_panel->setVisible(false);
    }
}