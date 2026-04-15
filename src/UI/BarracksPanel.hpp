#pragma once
#include <TGUI/Backend/SFML-Graphics.hpp>
#include <TGUI/AllWidgets.hpp>
#include <functional>

class Building;
class Kingdom;
class GameConfig;

class BarracksPanel {
public:
    void init(const tgui::Panel::Ptr& parent);
    void show(const Building& barracks, const Kingdom& kingdom, const GameConfig& config,
              bool allowProduce);
    void hide();

    void setOnProduce(std::function<void(int barracksId, int pieceType)> callback);

private:
    tgui::Panel::Ptr m_panel;
    tgui::Label::Ptr m_ownerLabel;
    tgui::Label::Ptr m_cellsLabel;
    tgui::Label::Ptr m_hpLabel;
    tgui::Label::Ptr m_statusLabel;
    tgui::Label::Ptr m_turnsLabel;
    tgui::Button::Ptr m_producePawnBtn;
    tgui::Button::Ptr m_produceKnightBtn;
    tgui::Button::Ptr m_produceBishopBtn;
    tgui::Button::Ptr m_produceRookBtn;
    std::function<void(int, int)> m_onProduce;
    int m_currentBarracksId = -1;
};
