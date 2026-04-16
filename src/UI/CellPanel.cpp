#include "UI/CellPanel.hpp"

#include "Board/Cell.hpp"
#include "Board/CellTraversal.hpp"
#include "Board/CellType.hpp"
#include "UI/HUDLayout.hpp"

namespace {

std::string cellTypeLabel(CellType type) {
    switch (type) {
        case CellType::Void: return "Void";
        case CellType::Grass: return "Grass";
        case CellType::Dirt: return "Dirt";
        case CellType::Water: return "Water";
    }

    return "Unknown";
}

} // namespace

void CellPanel::init(const tgui::Panel::Ptr& parent) {
    m_panel = tgui::Panel::create({"&.width", "&.height"});
    HUDLayout::styleEmbeddedPanel(m_panel);
    parent->add(m_panel);

    auto titleLabel = tgui::Label::create("Cell");
    titleLabel->setPosition({10, 10});
    HUDLayout::styleSidebarTitle(titleLabel);
    m_panel->add(titleLabel);

    m_positionLabel = tgui::Label::create("Cell: 0, 0");
    m_positionLabel->setPosition({10, 54});
    m_positionLabel->setSize({316, 22});
    HUDLayout::styleSidebarBody(m_positionLabel);
    m_panel->add(m_positionLabel);

    m_terrainLabel = tgui::Label::create("Terrain: Grass");
    m_terrainLabel->setPosition({10, 84});
    m_terrainLabel->setSize({316, 22});
    HUDLayout::styleSidebarBody(m_terrainLabel);
    m_panel->add(m_terrainLabel);

    m_zoneLabel = tgui::Label::create("Traversable: Yes");
    m_zoneLabel->setPosition({10, 114});
    m_zoneLabel->setSize({316, 22});
    HUDLayout::styleSidebarBody(m_zoneLabel);
    m_panel->add(m_zoneLabel);

    m_statusLabel = tgui::Label::create("Status: Empty");
    m_statusLabel->setPosition({10, 144});
    m_statusLabel->setSize({316, 22});
    HUDLayout::styleSidebarBody(m_statusLabel);
    m_panel->add(m_statusLabel);

    m_panel->setVisible(false);
}

void CellPanel::show(const Cell& cell) {
    if (!m_panel) {
        return;
    }

    m_panel->moveToFront();

    std::string status = "Empty";
    if (cell.piece) {
        status = "Occupied by piece";
    } else if (cell.building) {
        status = "Occupied by building";
    } else if (cell.type == CellType::Water) {
        status = "Blocked by water";
    }

    m_positionLabel->setText("Cell: " + std::to_string(cell.position.x) + ", " + std::to_string(cell.position.y));
    m_terrainLabel->setText("Terrain: " + cellTypeLabel(cell.type));
    m_zoneLabel->setText(std::string("Traversable: ") + (isCellTerrainTraversable(cell) ? "Yes" : "No"));
    m_statusLabel->setText("Status: " + status);
    m_panel->setVisible(true);
}

void CellPanel::hide() {
    if (m_panel) {
        m_panel->setVisible(false);
    }
}