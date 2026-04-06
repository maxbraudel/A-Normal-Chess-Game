#pragma once
#include <TGUI/Backend/SFML-Graphics.hpp>
#include <TGUI/AllWidgets.hpp>
#include <functional>

class Piece;
class GameConfig;

class PiecePanel {
public:
    void init(tgui::Gui& gui);
    void show(const Piece& piece, const GameConfig& config, bool allowUpgrade);
    void hide();

    void setOnUpgrade(std::function<void(int pieceId)> callback);

private:
    tgui::Panel::Ptr m_panel;
    tgui::Label::Ptr m_typeLabel;
    tgui::Label::Ptr m_xpLabel;
    tgui::Label::Ptr m_levelLabel;
    tgui::Button::Ptr m_upgradeBtn;
    std::function<void(int)> m_onUpgrade;
    int m_currentPieceId = -1;
};
