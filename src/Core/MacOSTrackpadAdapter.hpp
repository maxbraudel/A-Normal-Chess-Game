#pragma once

class LiveResizeRenderWindow;
class Camera;

class MacOSTrackpadAdapter {
public:
#ifdef __APPLE__
    MacOSTrackpadAdapter();
    ~MacOSTrackpadAdapter();
#else
    MacOSTrackpadAdapter() = default;
    ~MacOSTrackpadAdapter() = default;
#endif

    MacOSTrackpadAdapter(const MacOSTrackpadAdapter&) = delete;
    MacOSTrackpadAdapter& operator=(const MacOSTrackpadAdapter&) = delete;

#ifdef __APPLE__
    void install(LiveResizeRenderWindow& window, Camera& camera);
    void uninstall();

private:
    struct Impl;
    Impl* m_impl;
#else
    void install(LiveResizeRenderWindow&, Camera&) {}
    void uninstall() {}
#endif
};