#pragma once
#include <TGUI/Backend/SFML-Graphics.hpp>
#include <TGUI/AllWidgets.hpp>
#include <functional>

struct TurnCommand;

class Piece;
class GameConfig;

class PiecePanel {
public:
    void init(const tgui::Panel::Ptr& parent);
    void show(const Piece& piece,
              const GameConfig& config,
              bool allowUpgrade,
              bool allowDisband,
              const TurnCommand* pendingUpgrade = nullptr,
              const TurnCommand* pendingDisband = nullptr);
    void hide();

    void setOnUpgrade(std::function<void(int pieceId, int targetType)> callback);
    void setOnDisband(std::function<void(int pieceId)> callback);

private:
    tgui::Panel::Ptr m_panel;
    tgui::Label::Ptr m_ownerLabel;
    tgui::Label::Ptr m_positionLabel;
    tgui::Label::Ptr m_typeLabel;
    tgui::Label::Ptr m_xpLabel;
    tgui::Label::Ptr m_levelLabel;
    tgui::Label::Ptr m_moveCostLabel;
    tgui::Label::Ptr m_upkeepLabel;
    tgui::Button::Ptr m_primaryUpgradeBtn;
    tgui::Button::Ptr m_secondaryUpgradeBtn;
    tgui::Button::Ptr m_disbandButton;
    std::function<void(int, int)> m_onUpgrade;
    std::function<void(int)> m_onDisband;
    int m_currentPieceId = -1;
    int m_primaryUpgradeTarget = -1;
    int m_secondaryUpgradeTarget = -1;
};
