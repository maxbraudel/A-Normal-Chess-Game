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

    m_titleLabel = tgui::Label::create("");
    HUDLayout::placeSidebarPanelTitle(m_titleLabel);
    m_panel->add(m_titleLabel);

    m_positionLabel = tgui::Label::create("Cell: 0, 0");
    HUDLayout::placeSidebarPanelBodyLabel(m_positionLabel, 0);
    m_panel->add(m_positionLabel);

    m_terrainLabel = tgui::Label::create("Terrain: Grass");
    HUDLayout::placeSidebarPanelBodyLabel(m_terrainLabel, 1);
    m_panel->add(m_terrainLabel);

    m_zoneLabel = tgui::Label::create("Traversable: Yes");
    HUDLayout::placeSidebarPanelBodyLabel(m_zoneLabel, 2);
    m_panel->add(m_zoneLabel);

    m_statusLabel = tgui::Label::create("Status: Empty");
    HUDLayout::placeSidebarPanelBodyLabel(m_statusLabel, 3);
    m_panel->add(m_statusLabel);

    m_panel->setVisible(false);
}

void CellPanel::show(const Cell& cell, const std::string& title) {
    if (!m_panel) {
        return;
    }

    m_panel->moveToFront();
    if (m_titleLabel) {
        m_titleLabel->setText(title);
    }

    std::string status = "Empty";
    if (cell.piece) {
        status = "Occupied by piece";
    } else if (cell.autonomousUnit) {
        status = "Occupied by autonomous unit";
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