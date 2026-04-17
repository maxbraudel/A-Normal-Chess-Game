#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

#include <cmath>

#include "Core/MacOSTrackpadAdapter.hpp"

#include "Core/LiveResizeRenderWindow.hpp"
#include "Render/Camera.hpp"

namespace {

constexpr float kTrackpadDeltaEpsilon = 0.001f;
constexpr float kTrackpadInitialPanDelta = 1.0f;

// IMP for wantsScrollEventsForSwipeTrackingOnAxis: — always returns NO
// so macOS skips the swipe-between-pages disambiguation delay and delivers
// NSEventPhaseBegan immediately when fingers start moving.
static BOOL noSwipeTracking(id, SEL, NSInteger) { return NO; }

bool hasPhase(NSEventPhase phase, NSEventPhase flag) {
    return (phase & flag) != 0;
}

float resolveTrackpadPanDelta(CGFloat preciseDelta,
                              CGFloat legacyDelta,
                              bool isGestureStart) {
    float delta = static_cast<float>(preciseDelta);
    if (std::abs(delta) < kTrackpadDeltaEpsilon) {
        delta = static_cast<float>(legacyDelta);
    }

    if (isGestureStart && std::abs(delta) >= kTrackpadDeltaEpsilon
        && std::abs(delta) < kTrackpadInitialPanDelta) {
        delta = std::copysign(kTrackpadInitialPanDelta, delta);
    }

    return delta;
}

float trackpadZoomFactor(CGFloat magnification) {
    return std::exp(-static_cast<float>(magnification));
}

} // namespace

struct MacOSTrackpadAdapter::Impl {
    id eventMonitor = nil;
    NSWindow* window = nil;
    Camera* camera = nullptr;
    bool scrollGestureActive = false;
    bool scrollGestureAwaitingDelta = false;
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

    // Disable the macOS swipe-between-pages gesture disambiguation on the SFML
    // content view. Without this, the system delays NSEventPhaseBegan by
    // 50-150 ms while deciding if a two-finger scroll is a page swipe,
    // which is the "startup lag" felt at the beginning of every pan.
    NSView* contentView = m_impl->window.contentView;
    if (contentView != nil) {
        SEL sel = @selector(wantsScrollEventsForSwipeTrackingOnAxis:);
        if (![contentView respondsToSelector:sel]) {
            class_addMethod(
                object_getClass(contentView),
                sel,
                reinterpret_cast<IMP>(noSwipeTracking),
                "B@:q");  // BOOL (id, SEL, NSInteger) — q = long/NSInteger on arm64
        }
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

        const NSEventPhase phase = event.phase;
        const NSEventPhase momentumPhase = event.momentumPhase;
        if (hasPhase(phase, NSEventPhaseMayBegin)) {
            impl->scrollGestureAwaitingDelta = true;
            return event;
        }

        const bool phaseActive = hasPhase(phase, NSEventPhaseBegan)
            || hasPhase(phase, NSEventPhaseChanged);
        const bool momentumActive = hasPhase(momentumPhase, NSEventPhaseBegan)
            || hasPhase(momentumPhase, NSEventPhaseChanged);
        const bool phaseEnded = hasPhase(phase, NSEventPhaseEnded)
            || hasPhase(phase, NSEventPhaseCancelled);
        const bool momentumEnded = hasPhase(momentumPhase, NSEventPhaseEnded)
            || hasPhase(momentumPhase, NSEventPhaseCancelled);

        if (!momentumActive && phaseActive && !impl->scrollGestureActive) {
            impl->scrollGestureAwaitingDelta = true;
        }

        const bool isGestureStart = impl->scrollGestureAwaitingDelta && !momentumActive;

        const float directionScale = event.isDirectionInvertedFromDevice ? -1.0f : 1.0f;
        const float zoomLevel = impl->camera->getZoomLevel();
        const float deltaX = resolveTrackpadPanDelta(event.scrollingDeltaX,
                                                     event.deltaX,
                                                     isGestureStart)
            * directionScale * zoomLevel;
        const float deltaY = resolveTrackpadPanDelta(event.scrollingDeltaY,
                                                     event.deltaY,
                                                     isGestureStart)
            * directionScale * zoomLevel;

        if (std::abs(deltaX) < kTrackpadDeltaEpsilon && std::abs(deltaY) < kTrackpadDeltaEpsilon) {
            if (phaseEnded || momentumEnded
                || (phase == NSEventPhaseNone && momentumPhase == NSEventPhaseNone)) {
                impl->scrollGestureActive = false;
                impl->scrollGestureAwaitingDelta = false;
            }
            return (impl->scrollGestureActive || impl->scrollGestureAwaitingDelta) ? nil : event;
        }

        impl->scrollGestureActive = phaseActive || momentumActive;
        impl->scrollGestureAwaitingDelta = false;
        impl->camera->pan({deltaX, deltaY});

        if (phaseEnded || momentumEnded
            || (phase == NSEventPhaseNone && momentumPhase == NSEventPhaseNone)) {
            impl->scrollGestureActive = false;
            impl->scrollGestureAwaitingDelta = false;
        }

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