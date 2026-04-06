#include "UI/BuildToolPanel.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Buildings/BuildingType.hpp"
#include "Config/GameConfig.hpp"

void BuildToolPanel::init(tgui::Gui& gui) {
    m_panel = tgui::Panel::create({200, 300});
    m_panel->setPosition({"&.width - 210", "50"});
    m_panel->getRenderer()->setBackgroundColor(tgui::Color(50, 50, 50, 220));
    m_panel->getRenderer()->setBorderColor(tgui::Color::White);
    m_panel->getRenderer()->setBorders({1});
    gui.add(m_panel, "BuildToolPanel");

    auto titleLabel = tgui::Label::create("Build");
    titleLabel->setPosition({10, 5});
    titleLabel->setTextSize(16);
    titleLabel->getRenderer()->setTextColor(tgui::Color::White);
    m_panel->add(titleLabel);

    struct BuildOption { std::string name; BuildingType type; };
    std::vector<BuildOption> options = {
        {"Barracks",   BuildingType::Barracks},
        {"Wood Wall",  BuildingType::WoodWall},
        {"Stone Wall", BuildingType::StoneWall},
        {"Bridge",     BuildingType::Bridge},
        {"Arena",      BuildingType::Arena}
    };

    float y = 35.0f;
    for (const auto& opt : options) {
        auto btn = tgui::Button::create(opt.name);
        btn->setPosition({10, y});
        btn->setSize({180, 30});
        int typeInt = static_cast<int>(opt.type);
        btn->onPress([this, typeInt]() {
            if (m_onSelectBuildType) m_onSelectBuildType(typeInt);
        });
        m_panel->add(btn);
        y += 40.0f;
    }

    m_panel->setVisible(false);
}

void BuildToolPanel::show(const Kingdom& kingdom, const GameConfig& config) {
    if (m_panel) m_panel->setVisible(true);
}

void BuildToolPanel::hide() { if (m_panel) m_panel->setVisible(false); }

void BuildToolPanel::setOnSelectBuildType(std::function<void(int)> callback) {
    m_onSelectBuildType = std::move(callback);
}
