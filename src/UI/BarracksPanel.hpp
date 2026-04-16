#pragma once
#include <TGUI/Backend/SFML-Graphics.hpp>
#include <TGUI/AllWidgets.hpp>
#include <functional>

struct TurnCommand;

class Building;
class Kingdom;
class GameConfig;

class BarracksPanel {
public:
    void init(const tgui::Panel::Ptr& parent);
    void show(const Building& barracks, const Kingdom& kingdom, const GameConfig& config,
              bool allowProduce,
              bool allowCancelConstruction,
              const TurnCommand* pendingProduce = nullptr);
    void hide();

    void setOnProduce(std::function<void(int barracksId, int pieceType)> callback);
    void setOnCancelConstruction(std::function<void(int barracksId)> callback);

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
    tgui::Button::Ptr m_cancelConstructionBtn;
    std::function<void(int, int)> m_onProduce;
    std::function<void(int)> m_onCancelConstruction;
    int m_currentBarracksId = -1;
};
