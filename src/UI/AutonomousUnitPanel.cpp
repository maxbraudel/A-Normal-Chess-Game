#include "UI/AutonomousUnitPanel.hpp"

#include "Autonomous/AutonomousUnit.hpp"
#include "UI/HUDLayout.hpp"
#include "Units/PieceType.hpp"

namespace {

std::string pieceTypeName(PieceType type) {
    switch (type) {
        case PieceType::Pawn:
            return "Pawn";
        case PieceType::Knight:
            return "Knight";
        case PieceType::Bishop:
            return "Bishop";
        case PieceType::Rook:
            return "Rook";
        case PieceType::Queen:
            return "Queen";
        case PieceType::King:
            return "King";
    }

    return "Unknown";
}

std::string ownerText(const AutonomousUnit& unit) {
    switch (unit.type) {
        case AutonomousUnitType::InfernalPiece:
            return "Owner: Evil Kingdom";
    }

    return "Owner: Unknown";
}

std::string phaseText(const AutonomousUnit& unit) {
    if (unit.type == AutonomousUnitType::InfernalPiece) {
        return std::string{"Phase: "} + infernalPhaseDisplayName(unit.infernal.phase);
    }

    return "Phase: N/A";
}

std::string manifestedPieceText(const AutonomousUnit& unit) {
    if (unit.type == AutonomousUnitType::InfernalPiece) {
        return "Manifested As: " + pieceTypeName(unit.infernal.manifestedPieceType);
    }

    return "Manifested As: N/A";
}

std::string hintText(const AutonomousUnit& unit) {
    if (unit.type != AutonomousUnitType::InfernalPiece) {
        return "Inspect this unit to understand its current position and behavior.";
    }

    switch (unit.infernal.phase) {
        case InfernalPhase::Hunting:
            return "This infernal piece is actively pursuing its prey.";
        case InfernalPhase::Returning:
            return "This infernal piece is disengaging after completing its hunt.";
        case InfernalPhase::Searching:
            return "This infernal lost direct contact and is searching for a new route back to prey.";
    }

    return "Inspect this unit to understand its current position and behavior.";
}

} // namespace

void AutonomousUnitPanel::init(const tgui::Panel::Ptr& parent) {
    m_panel = tgui::Panel::create({"&.width", "&.height"});
    HUDLayout::styleEmbeddedPanel(m_panel);
    parent->add(m_panel);

    m_titleLabel = tgui::Label::create("");
    HUDLayout::placeSidebarPanelTitle(m_titleLabel);
    m_panel->add(m_titleLabel);

    m_ownerLabel = tgui::Label::create("Owner: Evil Kingdom");
    HUDLayout::placeSidebarPanelBodyLabel(m_ownerLabel, 0);
    m_panel->add(m_ownerLabel);

    m_positionLabel = tgui::Label::create("Cell: 0, 0");
    HUDLayout::placeSidebarPanelBodyLabel(m_positionLabel, 1);
    m_panel->add(m_positionLabel);

    m_typeLabel = tgui::Label::create("Type: Infernal Piece");
    HUDLayout::placeSidebarPanelBodyLabel(m_typeLabel, 2);
    m_panel->add(m_typeLabel);

    m_manifestedTypeLabel = tgui::Label::create("Manifested As: Queen");
    HUDLayout::placeSidebarPanelBodyLabel(m_manifestedTypeLabel, 3);
    m_panel->add(m_manifestedTypeLabel);

    m_phaseLabel = tgui::Label::create("Phase: Hunting");
    HUDLayout::placeSidebarPanelBodyLabel(m_phaseLabel, 4);
    m_panel->add(m_phaseLabel);

    m_hintLabel = tgui::Label::create("");
    m_hintLabel->setPosition({HUDLayout::kSidebarPanelInset, 210});
    m_hintLabel->setSize({HUDLayout::kSidebarPanelBodyWidth, 110});
    HUDLayout::styleSidebarHint(m_hintLabel);
    m_panel->add(m_hintLabel);

    m_panel->setVisible(false);
}

void AutonomousUnitPanel::show(const AutonomousUnit& unit, const std::string& title) {
    if (!m_panel) {
        return;
    }

    m_panel->moveToFront();
    if (m_titleLabel) {
        m_titleLabel->setText(title);
    }
    m_ownerLabel->setText(ownerText(unit));
    m_typeLabel->setText(std::string{"Type: "} + autonomousUnitTypeDisplayName(unit.type));
    m_positionLabel->setText(
        "Cell: " + std::to_string(unit.position.x) + ", " + std::to_string(unit.position.y));
    m_manifestedTypeLabel->setText(manifestedPieceText(unit));
    m_phaseLabel->setText(phaseText(unit));
    m_hintLabel->setText(hintText(unit));
    m_panel->setVisible(true);
}

void AutonomousUnitPanel::hide() {
    if (m_panel) {
        m_panel->setVisible(false);
    }
}