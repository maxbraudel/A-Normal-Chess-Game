#include "UI/HUD.hpp"
#include "Assets/AssetManager.hpp"
#include "UI/HUDLayout.hpp"

namespace {

std::string metricText(std::size_t index, int value) {
    return inGameMetricLabels()[index] + ": " + std::to_string(value);
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

    m_pauseButton = tgui::Button::create("Pause");
    HUDLayout::styleHudButton(m_pauseButton);
    HUDLayout::placeStackChild(m_pauseButton, 0, HUDLayout::kActionWidth);
    m_pauseButton->onPress([this]() {
        if (m_onPause) {
            m_onPause();
        }
    });
    m_actionPanel->add(m_pauseButton);

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
}

void HUD::show() {
    if (m_metricsPanel) m_metricsPanel->setVisible(true);
    if (m_statusPanel) m_statusPanel->setVisible(true);
    if (m_actionPanel) m_actionPanel->setVisible(true);
}

void HUD::hide() {
    if (m_metricsPanel) m_metricsPanel->setVisible(false);
    if (m_statusPanel) m_statusPanel->setVisible(false);
    if (m_actionPanel) m_actionPanel->setVisible(false);
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

void HUD::setOnPause(std::function<void()> callback) { m_onPause = std::move(callback); }
void HUD::setOnResetTurn(std::function<void()> callback) { m_onResetTurn = std::move(callback); }
void HUD::setOnEndTurn(std::function<void()> callback) { m_onEndTurn = std::move(callback); }
