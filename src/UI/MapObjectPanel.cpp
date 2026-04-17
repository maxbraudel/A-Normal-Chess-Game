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

    auto titleLabel = tgui::Label::create("Map Object");
    titleLabel->setPosition({10, 10});
    HUDLayout::styleSidebarTitle(titleLabel);
    m_panel->add(titleLabel);

    m_typeLabel = tgui::Label::create("Type: Chest");
    m_typeLabel->setPosition({10, 54});
    m_typeLabel->setSize({316, 22});
    HUDLayout::styleSidebarBody(m_typeLabel);
    m_panel->add(m_typeLabel);

    m_positionLabel = tgui::Label::create("Cell: 0, 0");
    m_positionLabel->setPosition({10, 84});
    m_positionLabel->setSize({316, 22});
    HUDLayout::styleSidebarBody(m_positionLabel);
    m_panel->add(m_positionLabel);

    m_statusLabel = tgui::Label::create("Status: Closed chest");
    m_statusLabel->setPosition({10, 114});
    m_statusLabel->setSize({316, 22});
    HUDLayout::styleSidebarBody(m_statusLabel);
    m_panel->add(m_statusLabel);

    m_hintLabel = tgui::Label::create("");
    m_hintLabel->setPosition({10, 150});
    m_hintLabel->setSize({316, 96});
    m_hintLabel->setAutoSize(false);
    m_hintLabel->setTextSize(14);
    m_hintLabel->getRenderer()->setTextColor(tgui::Color(220, 220, 220));
    m_panel->add(m_hintLabel);

    m_panel->setVisible(false);
}

void MapObjectPanel::show(const MapObject& object) {
    if (!m_panel) {
        return;
    }

    m_panel->moveToFront();
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