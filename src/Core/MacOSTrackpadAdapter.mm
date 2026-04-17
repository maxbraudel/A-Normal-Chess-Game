#import <Cocoa/Cocoa.h>

#include <cmath>

#include "Core/MacOSTrackpadAdapter.hpp"

#include "Core/LiveResizeRenderWindow.hpp"
#include "Render/Camera.hpp"

namespace {

float trackpadZoomFactor(CGFloat magnification) {
    return std::exp(-static_cast<float>(magnification));
}

} // namespace

struct MacOSTrackpadAdapter::Impl {
    id eventMonitor = nil;
    NSWindow* window = nil;
    Camera* camera = nullptr;
};

MacOSTrackpadAdapter::MacOSTrackpadAdapter()
    : m_impl(new Impl{}) {}

MacOSTrackpadAdapter::~MacOSTrackpadAdapter() {
    uninstall();
    delete m_impl;
}

void MacOSTrackpadAdapter::install(LiveResizeRenderWindow& window, Camera& camera) {
    uninstall();

    m_impl->window = static_cast<NSWindow*>(window.getSystemHandle());
    m_impl->camera = &camera;
    if (m_impl->window == nil || m_impl->camera == nullptr) {
        return;
    }

    Impl* impl = m_impl;
    m_impl->eventMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:(NSEventMaskScrollWheel | NSEventMaskMagnify)
                                                                 handler:^NSEvent*(NSEvent* event) {
        if (impl == nullptr || impl->window == nil || impl->camera == nullptr) {
            return event;
        }

        if (event.window != impl->window || !impl->window.isKeyWindow) {
            return event;
        }

        if (event.type == NSEventTypeMagnify) {
            const float factor = trackpadZoomFactor(event.magnification);
            impl->camera->zoom(factor);
            return nil;
        }

        if (event.type != NSEventTypeScrollWheel || !event.hasPreciseScrollingDeltas) {
            return event;
        }

        const float directionScale = event.isDirectionInvertedFromDevice ? -1.0f : 1.0f;
        const float zoomLevel = impl->camera->getZoomLevel();
        const float deltaX = static_cast<float>(event.scrollingDeltaX) * directionScale * zoomLevel;
        const float deltaY = static_cast<float>(event.scrollingDeltaY) * directionScale * zoomLevel;
        if (std::abs(deltaX) <= 0.001f && std::abs(deltaY) <= 0.001f) {
            return nil;
        }

        impl->camera->pan({deltaX, deltaY});
        return nil;
    }];
}

void MacOSTrackpadAdapter::uninstall() {
    if (m_impl == nullptr) {
        return;
    }

    if (m_impl->eventMonitor != nil) {
        [NSEvent removeMonitor:m_impl->eventMonitor];
        m_impl->eventMonitor = nil;
    }

    m_impl->window = nil;
    m_impl->camera = nullptr;
}