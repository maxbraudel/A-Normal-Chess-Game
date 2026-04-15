#include "UI/HUD.hpp"
#include "Assets/AssetManager.hpp"
#include "UI/HUDLayout.hpp"

namespace {

std::string metricText(std::size_t index, int value) {
    return inGameMetricLabels()[index] + ": " + std::to_string(value);
}

void applyMultiplayerStatusTone(const tgui::Label::Ptr& label, MultiplayerStatusTone tone) {
    if (!label) {
        return;
    }

    auto renderer = label->getRenderer();
    switch (tone) {
        case MultiplayerStatusTone::Connected:
            renderer->setTextColor(tgui::Color(178, 255, 196));
            renderer->setBackgroundColor(tgui::Color(20, 82, 44, 220));
            renderer->setBorderColor(tgui::Color(98, 189, 118));
            break;

        case MultiplayerStatusTone::Waiting:
            renderer->setTextColor(tgui::Color(255, 235, 178));
            renderer->setBackgroundColor(tgui::Color(110, 74, 10, 220));
            renderer->setBorderColor(tgui::Color(218, 175, 74));
            break;

        case MultiplayerStatusTone::Error:
            renderer->setTextColor(tgui::Color(255, 205, 205));
            renderer->setBackgroundColor(tgui::Color(118, 24, 24, 225));
            renderer->setBorderColor(tgui::Color(218, 92, 92));
            break;

        case MultiplayerStatusTone::Neutral:
        default:
            renderer->setTextColor(tgui::Color(220, 235, 255));
            renderer->setBackgroundColor(tgui::Color(18, 38, 72, 215));
            renderer->setBorderColor(tgui::Color(110, 148, 210));
            break;
    }
}

} // namespace

void HUD::init(tgui::Gui& gui, const AssetManager& assets) {
    (void) assets;

    m_metricsPanel = tgui::Panel::create(HUDLayout::metricsPanelSize());
    m_metricsPanel->setPosition(HUDLayout::anchorPositionForSize(HUDAnchor::TopLeft,
                                                                 HUDLayout::metricsPanelSize().x,
                                                                 HUDLayout::metricsPanelSize().y));
    HUDLayout::makeTransparentPanel(m_metricsPanel);
    gui.add(m_metricsPanel, "HUDMetricsPanel");

    m_statusPanel = tgui::Panel::create(HUDLayout::stackSize(1, HUDLayout::kStatusWidth));
    m_statusPanel->setPosition(HUDLayout::anchorPosition(HUDAnchor::TopCenter, 1, HUDLayout::kStatusWidth));
    HUDLayout::makeTransparentPanel(m_statusPanel);
    gui.add(m_statusPanel, "HUDStatusPanel");

    m_actionPanel = tgui::Panel::create(HUDLayout::stackSize(3, HUDLayout::kActionWidth));
    m_actionPanel->setPosition(HUDLayout::anchorPosition(HUDAnchor::TopRight, 3, HUDLayout::kActionWidth));
    HUDLayout::makeTransparentPanel(m_actionPanel);
    gui.add(m_actionPanel, "HUDActionPanel");

    m_networkPanel = tgui::Panel::create({HUDLayout::kNetworkStatusWidth, HUDLayout::kNetworkStatusHeight});
    m_networkPanel->setPosition(HUDLayout::anchorPositionForSize(
        HUDAnchor::TopCenter,
        HUDLayout::kNetworkStatusWidth,
        HUDLayout::kNetworkStatusHeight,
        HUDLayout::kEdgeMargin,
        HUDLayout::kEdgeMargin + HUDLayout::kTopComponentHeight + HUDLayout::kComponentGap));
    HUDLayout::makeTransparentPanel(m_networkPanel);
    gui.add(m_networkPanel, "HUDNetworkPanel");

    for (std::size_t index = 0; index < m_metricLabels.size(); ++index) {
        m_metricLabels[index] = tgui::Label::create(metricText(index, 0));
        HUDLayout::styleHudIndicator(m_metricLabels[index], HUDLayout::metricColors()[index],
                                     HUDLayout::metricWidths()[index], HUDLayout::kTopComponentHeight, 13);
        HUDLayout::placeMetricChild(m_metricLabels[index], index);
        m_metricsPanel->add(m_metricLabels[index]);
    }

    m_statusLabel = tgui::Label::create("Turn 1 | White Kingdom : Idle");
    HUDLayout::styleStatusIndicator(m_statusLabel);
    HUDLayout::placeStackChild(m_statusLabel, 0, HUDLayout::kStatusWidth);
    m_statusPanel->add(m_statusLabel);

    m_networkStatusLabel = tgui::Label::create("");
    HUDLayout::styleHudIndicator(
        m_networkStatusLabel,
        tgui::Color(220, 235, 255),
        HUDLayout::kNetworkStatusWidth,
        HUDLayout::kNetworkStatusHeight,
        13);
    m_networkStatusLabel->setPosition({0, 0});
    applyMultiplayerStatusTone(m_networkStatusLabel, MultiplayerStatusTone::Neutral);
    m_networkPanel->add(m_networkStatusLabel);

    m_menuButton = tgui::Button::create("Menu");
    HUDLayout::styleHudButton(m_menuButton);
    HUDLayout::placeStackChild(m_menuButton, 0, HUDLayout::kActionWidth);
    m_menuButton->onPress([this]() {
        if (m_onMenu) {
            m_onMenu();
        }
    });
    m_actionPanel->add(m_menuButton);

    m_endTurnButton = tgui::Button::create("End Turn");
    HUDLayout::styleHudButton(m_endTurnButton);
    HUDLayout::placeStackChild(m_endTurnButton, 1, HUDLayout::kActionWidth);
    m_endTurnButton->onPress([this]() {
        if (m_onEndTurn) {
            m_onEndTurn();
        }
    });
    m_actionPanel->add(m_endTurnButton);

    m_resetTurnButton = tgui::Button::create("Reset Turn");
    HUDLayout::styleHudButton(m_resetTurnButton);
    HUDLayout::placeStackChild(m_resetTurnButton, 2, HUDLayout::kActionWidth);
    m_resetTurnButton->onPress([this]() {
        if (m_onResetTurn) {
            m_onResetTurn();
        }
    });
    m_actionPanel->add(m_resetTurnButton);

    m_metricsPanel->setVisible(false);
    m_statusPanel->setVisible(false);
    m_actionPanel->setVisible(false);
    m_networkPanel->setVisible(false);
}

void HUD::show() {
    m_visible = true;
    if (m_metricsPanel) m_metricsPanel->setVisible(true);
    if (m_statusPanel) m_statusPanel->setVisible(true);
    if (m_actionPanel) m_actionPanel->setVisible(true);
    if (m_networkPanel && m_networkStatusLabel && !m_networkStatusLabel->getText().empty()) {
        m_networkPanel->setVisible(true);
    }
}

void HUD::hide() {
    m_visible = false;
    if (m_metricsPanel) m_metricsPanel->setVisible(false);
    if (m_statusPanel) m_statusPanel->setVisible(false);
    if (m_actionPanel) m_actionPanel->setVisible(false);
    if (m_networkPanel) m_networkPanel->setVisible(false);
}

void HUD::update(const InGameViewModel& model) {
    if (m_metricLabels[0]) m_metricLabels[0]->setText(metricText(0, model.activeGold));
    if (m_metricLabels[1]) m_metricLabels[1]->setText(metricText(1, model.activeOccupiedCells));
    if (m_metricLabels[2]) m_metricLabels[2]->setText(metricText(2, model.activeTroops));
    if (m_metricLabels[3]) m_metricLabels[3]->setText(metricText(3, model.activeIncome));

    if (m_statusLabel) {
        m_statusLabel->setText("T" + std::to_string(model.turnNumber) + " | "
                               + model.activeTurnLabel + " : " + model.statusLabel);
    }
    if (m_endTurnButton) m_endTurnButton->setEnabled(model.allowCommands);
    if (m_resetTurnButton) m_resetTurnButton->setEnabled(model.allowCommands);
}

void HUD::setMultiplayerStatus(const std::string& text, MultiplayerStatusTone tone) {
    if (!m_networkStatusLabel || !m_networkPanel) {
        return;
    }

    m_networkStatusLabel->setText(text);
    applyMultiplayerStatusTone(m_networkStatusLabel, tone);
    m_networkPanel->setVisible(m_visible && !text.empty());
}

void HUD::clearMultiplayerStatus() {
    if (m_networkStatusLabel) {
        m_networkStatusLabel->setText("");
    }
    if (m_networkPanel) {
        m_networkPanel->setVisible(false);
    }
}

void HUD::setOnMenu(std::function<void()> callback) { m_onMenu = std::move(callback); }
void HUD::setOnResetTurn(std::function<void()> callback) { m_onResetTurn = std::move(callback); }
void HUD::setOnEndTurn(std::function<void()> callback) { m_onEndTurn = std::move(callback); }
