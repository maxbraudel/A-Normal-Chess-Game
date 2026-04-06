#include "UI/BuildingPanel.hpp"
#include "Buildings/Building.hpp"
#include "Buildings/BuildingType.hpp"

static std::string buildingTypeName(BuildingType type) {
    switch (type) {
        case BuildingType::Church:    return "Church";
        case BuildingType::Mine:      return "Mine";
        case BuildingType::Farm:      return "Farm";
        case BuildingType::Barracks:  return "Barracks";
        case BuildingType::WoodWall:  return "Wood Wall";
        case BuildingType::StoneWall: return "Stone Wall";
        case BuildingType::Bridge:    return "Bridge";
        case BuildingType::Arena:     return "Arena";
        default: return "Unknown";
    }
}

void BuildingPanel::init(tgui::Gui& gui) {
    m_panel = tgui::Panel::create({200, 180});
    m_panel->setPosition({"&.width - 210", "50"});
    m_panel->getRenderer()->setBackgroundColor(tgui::Color(50, 50, 50, 220));
    m_panel->getRenderer()->setBorderColor(tgui::Color::White);
    m_panel->getRenderer()->setBorders({1});
    gui.add(m_panel, "BuildingPanel");

    m_typeLabel = tgui::Label::create("Type: ");
    m_typeLabel->setPosition({10, 10});
    m_typeLabel->setTextSize(14);
    m_typeLabel->getRenderer()->setTextColor(tgui::Color::White);
    m_panel->add(m_typeLabel);

    m_hpLabel = tgui::Label::create("HP: ");
    m_hpLabel->setPosition({10, 40});
    m_hpLabel->setTextSize(14);
    m_hpLabel->getRenderer()->setTextColor(tgui::Color::White);
    m_panel->add(m_hpLabel);

    m_statusLabel = tgui::Label::create("Status: ");
    m_statusLabel->setPosition({10, 70});
    m_statusLabel->setTextSize(14);
    m_statusLabel->getRenderer()->setTextColor(tgui::Color::White);
    m_panel->add(m_statusLabel);

    m_panel->setVisible(false);
}

void BuildingPanel::show(const Building& building) {
    if (!m_panel) return;
    m_typeLabel->setText("Type: " + buildingTypeName(building.type));

    // Calculate total HP
    int totalHP = 0;
    int maxHP = 0;
    for (int hp : building.cellHP) {
        totalHP += hp;
        maxHP += (building.type == BuildingType::StoneWall ? 3 : 1);
    }
    m_hpLabel->setText("HP: " + std::to_string(totalHP) + "/" + std::to_string(maxHP));
    m_statusLabel->setText(building.isDestroyed() ? "Status: Destroyed" :
                           building.isNeutral ? "Status: Neutral" : "Status: Active");
    m_panel->setVisible(true);
}

void BuildingPanel::hide() { if (m_panel) m_panel->setVisible(false); }
