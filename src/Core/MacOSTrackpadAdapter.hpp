#pragma once

class LiveResizeRenderWindow;
class Camera;

class MacOSTrackpadAdapter {
public:
    MacOSTrackpadAdapter();
    ~MacOSTrackpadAdapter();

    MacOSTrackpadAdapter(const MacOSTrackpadAdapter&) = delete;
    MacOSTrackpadAdapter& operator=(const MacOSTrackpadAdapter&) = delete;

    void install(LiveResizeRenderWindow& window, Camera& camera);
    void uninstall();

private:
    struct Impl;
    Impl* m_impl;
};