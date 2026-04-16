#pragma once

#include <TGUI/Backend/SFML-Graphics.hpp>
#include <TGUI/AllWidgets.hpp>
#include <array>
#include <sstream>
#include <string>

#include "UI/FocusStyle.hpp"

enum class HUDAnchor {
    TopLeft,
    TopCenter,
    TopRight,
    MiddleLeft,
    MiddleRight,
    BottomLeft,
    BottomCenter,
    BottomRight
};

namespace HUDLayout {

inline constexpr float kEdgeMargin = 12.f;
inline constexpr float kMetricWidth = 108.f;
inline constexpr float kWideMetricWidth = 156.f;
inline constexpr float kActionWidth = 118.f;
inline constexpr float kStatusWidth = 360.f;
inline constexpr float kAlertWidth = 420.f;
inline constexpr float kTopComponentHeight = 40.f;
inline constexpr float kPointIndicatorWidth = 240.f;
inline constexpr float kPointIndicatorHeight = 34.f;
inline constexpr float kNetworkStatusWidth = 280.f;
inline constexpr float kNetworkStatusHeight = 34.f;
inline constexpr float kToolbarButtonWidth = 110.f;
inline constexpr float kToolbarHeight = 38.f;
inline constexpr float kComponentGap = 6.f;
inline constexpr float kLeftSidebarWidth = 360.f;
inline constexpr float kRightSidebarWidth = 600.f;
inline constexpr float kEventLogTurnColumnWidth = 56.f;
inline constexpr float kEventLogKingdomColumnWidth = 136.f;
inline constexpr float kSidebarInnerMargin = 12.f;
inline constexpr float kSidebarSectionGap = 12.f;

inline std::string asLayoutValue(float value) {
    std::ostringstream stream;
    stream << value;
    return stream.str();
}

inline void styleEmbeddedPanel(const tgui::Panel::Ptr& panel);

inline float sidebarWidth(HUDAnchor anchor) {
    return (anchor == HUDAnchor::MiddleRight) ? kRightSidebarWidth : kLeftSidebarWidth;
}

inline const std::array<tgui::Color, 4>& metricColors() {
    static const std::array<tgui::Color, 4> colors = {
        tgui::Color(255, 215, 0),
        tgui::Color(200, 230, 255),
        tgui::Color(235, 235, 235),
        tgui::Color(120, 230, 160)
    };

    return colors;
}

inline const std::array<float, 4>& metricWidths() {
    static const std::array<float, 4> widths = {
        kMetricWidth,
        kWideMetricWidth,
        kMetricWidth,
        kWideMetricWidth
    };

    return widths;
}

inline float totalMetricWidth() {
    float width = 0.f;
    const auto& widths = metricWidths();
    for (std::size_t index = 0; index < widths.size(); ++index) {
        width += widths[index];
        if (index + 1 < widths.size()) {
            width += kComponentGap;
        }
    }

    return width;
}

inline tgui::Vector2f metricsPanelSize() {
    return {totalMetricWidth(), kTopComponentHeight};
}

inline tgui::Vector2f pointPanelSize() {
    return {
        kPointIndicatorWidth,
        (kPointIndicatorHeight * 2.f) + kComponentGap
    };
}

inline float metricOffsetX(std::size_t index) {
    float x = 0.f;
    const auto& widths = metricWidths();
    for (std::size_t current = 0; current < index && current < widths.size(); ++current) {
        x += widths[current] + kComponentGap;
    }

    return x;
}

inline tgui::Vector2f stackSize(int count,
                                float componentWidth = kMetricWidth,
                                float componentGap = kComponentGap,
                                float componentHeight = kTopComponentHeight) {
    if (count <= 0) {
        return {0.f, 0.f};
    }
    return {
        count * componentWidth + (count - 1) * componentGap,
        componentHeight
    };
}

inline tgui::Layout2d anchorPosition(HUDAnchor anchor,
                                     int count,
                                     float componentWidth = kMetricWidth,
                                     float componentGap = kComponentGap,
                                     float componentHeight = kTopComponentHeight,
                                     float marginX = kEdgeMargin,
                                     float marginY = kEdgeMargin) {
    const auto size = stackSize(count, componentWidth, componentGap, componentHeight);
    const std::string width = asLayoutValue(size.x);
    const std::string height = asLayoutValue(size.y);
    const std::string xMargin = asLayoutValue(marginX);
    const std::string yMargin = asLayoutValue(marginY);
    const auto layout = [](const std::string& x, const std::string& y) {
        return tgui::Layout2d{tgui::Layout{x}, tgui::Layout{y}};
    };

    switch (anchor) {
        case HUDAnchor::TopLeft:
            return layout(xMargin, yMargin);
        case HUDAnchor::TopCenter:
            return layout("(&.width - " + width + ") / 2", yMargin);
        case HUDAnchor::TopRight:
            return layout("&.width - " + width + " - " + xMargin, yMargin);
        case HUDAnchor::MiddleLeft:
            return layout(xMargin, "(&.height - " + height + ") / 2");
        case HUDAnchor::MiddleRight:
            return layout("&.width - " + width + " - " + xMargin, "(&.height - " + height + ") / 2");
        case HUDAnchor::BottomLeft:
            return layout(xMargin, "&.height - " + height + " - " + yMargin);
        case HUDAnchor::BottomCenter:
            return layout("(&.width - " + width + ") / 2", "&.height - " + height + " - " + yMargin);
        case HUDAnchor::BottomRight:
            return layout("&.width - " + width + " - " + xMargin, "&.height - " + height + " - " + yMargin);
    }

    return layout("0", "0");
}

inline tgui::Layout2d anchorPositionForSize(HUDAnchor anchor,
                                            float width,
                                            float height,
                                            float marginX = kEdgeMargin,
                                            float marginY = kEdgeMargin) {
    const std::string layoutWidth = asLayoutValue(width);
    const std::string layoutHeight = asLayoutValue(height);
    const std::string xMargin = asLayoutValue(marginX);
    const std::string yMargin = asLayoutValue(marginY);
    const auto layout = [](const std::string& x, const std::string& y) {
        return tgui::Layout2d{tgui::Layout{x}, tgui::Layout{y}};
    };

    switch (anchor) {
        case HUDAnchor::TopLeft:
            return layout(xMargin, yMargin);
        case HUDAnchor::TopCenter:
            return layout("(&.width - " + layoutWidth + ") / 2", yMargin);
        case HUDAnchor::TopRight:
            return layout("&.width - " + layoutWidth + " - " + xMargin, yMargin);
        case HUDAnchor::MiddleLeft:
            return layout(xMargin, "(&.height - " + layoutHeight + ") / 2");
        case HUDAnchor::MiddleRight:
            return layout("&.width - " + layoutWidth + " - " + xMargin, "(&.height - " + layoutHeight + ") / 2");
        case HUDAnchor::BottomLeft:
            return layout(xMargin, "&.height - " + layoutHeight + " - " + yMargin);
        case HUDAnchor::BottomCenter:
            return layout("(&.width - " + layoutWidth + ") / 2", "&.height - " + layoutHeight + " - " + yMargin);
        case HUDAnchor::BottomRight:
            return layout("&.width - " + layoutWidth + " - " + xMargin, "&.height - " + layoutHeight + " - " + yMargin);
    }

    return layout("0", "0");
}

inline tgui::Layout sidebarHeight() {
    return tgui::Layout{"(&.height * 7) / 10"};
}

inline tgui::Layout2d sidebarSize(HUDAnchor anchor) {
    return {tgui::Layout{asLayoutValue(sidebarWidth(anchor))}, sidebarHeight()};
}

inline tgui::Layout2d sidebarPosition(HUDAnchor anchor) {
    const std::string width = asLayoutValue(sidebarWidth(anchor));
    const std::string margin = asLayoutValue(kEdgeMargin);
    const std::string height = "((&.height * 7) / 10)";
    const auto layout = [](const std::string& x, const std::string& y) {
        return tgui::Layout2d{tgui::Layout{x}, tgui::Layout{y}};
    };

    switch (anchor) {
        case HUDAnchor::MiddleLeft:
            return layout(margin, "(&.height - " + height + ") / 2");
        case HUDAnchor::MiddleRight:
            return layout("&.width - " + width + " - " + margin, "(&.height - " + height + ") / 2");
        default:
            return layout(margin, "(&.height - " + height + ") / 2");
    }
}

inline void placeStackChild(const tgui::Widget::Ptr& widget,
                            int index,
                            float componentWidth = kMetricWidth,
                            float componentGap = kComponentGap,
                            float componentHeight = kTopComponentHeight) {
    widget->setPosition({index * (componentWidth + componentGap), 0});
    widget->setSize({componentWidth, componentHeight});
}

inline void placeMetricChild(const tgui::Widget::Ptr& widget, std::size_t index) {
    widget->setPosition({metricOffsetX(index), 0});
    widget->setSize({metricWidths()[index], kTopComponentHeight});
}

inline void styleHudButton(const tgui::Button::Ptr& button,
                           float width = kActionWidth,
                           float height = kTopComponentHeight,
                           unsigned int textSize = 16) {
    button->setSize({width, height});
    button->setTextSize(textSize);
    FocusStyle::neutralizeButtonFocus(button);
}

inline void styleHudIndicator(const tgui::Label::Ptr& label,
                              const tgui::Color& textColor,
                              float width = kMetricWidth,
                              float height = kTopComponentHeight,
                              unsigned int textSize = 14) {
    label->setAutoSize(false);
    label->setSize({width, height});
    label->setTextSize(textSize);
    label->setHorizontalAlignment(tgui::HorizontalAlignment::Center);
    label->setVerticalAlignment(tgui::VerticalAlignment::Center);
    label->getRenderer()->setTextColor(textColor);
    label->getRenderer()->setBackgroundColor(tgui::Color(18, 18, 18, 185));
    label->getRenderer()->setBorders({1});
    label->getRenderer()->setBorderColor(tgui::Color(105, 105, 105));
    label->getRenderer()->setPadding({8, 6, 8, 6});
}

inline void styleStatusIndicator(const tgui::Label::Ptr& label) {
    styleHudIndicator(label, tgui::Color::White, kStatusWidth, kTopComponentHeight, 14);
}

inline void styleEmbeddedPanel(const tgui::Panel::Ptr& panel) {
    panel->getRenderer()->setBackgroundColor(tgui::Color(0, 0, 0, 0));
    panel->getRenderer()->setBorders(0);
}

inline void makeTransparentPanel(const tgui::Panel::Ptr& panel) {
    styleEmbeddedPanel(panel);
}

inline void styleSidebarFrame(const tgui::Panel::Ptr& panel) {
    panel->getRenderer()->setBackgroundColor(tgui::Color(26, 26, 26, 220));
    panel->getRenderer()->setBorders({1});
    panel->getRenderer()->setBorderColor(tgui::Color(155, 155, 155));
}

inline void styleSidebarSection(const tgui::Panel::Ptr& panel) {
    panel->getRenderer()->setBackgroundColor(tgui::Color(12, 12, 12, 175));
    panel->getRenderer()->setBorders({1});
    panel->getRenderer()->setBorderColor(tgui::Color(85, 85, 85));
}

inline void styleSidebarTitle(const tgui::Label::Ptr& label) {
    label->setTextSize(17);
    label->getRenderer()->setTextColor(tgui::Color::White);
}

inline void styleSidebarBody(const tgui::Label::Ptr& label, unsigned int textSize = 14) {
    label->setTextSize(textSize);
    label->setAutoSize(false);
    label->getRenderer()->setTextColor(tgui::Color(230, 230, 230));
}

} // namespace HUDLayout