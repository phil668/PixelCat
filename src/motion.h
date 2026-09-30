#pragma once
#include <algorithm>
#include <cmath>
struct Pose { double rise; double scaleY; };
inline Pose pose(double t, double tapped, bool paused) {
    if (paused) return {0, 1};
    double elapsed = t - tapped;
    if (elapsed >= 0 && elapsed < 0.65)
        return {12 * std::sin(elapsed / 0.65 * 3.141592653589793), 1};
    return {0, 1 + 0.006 * std::sin(t * 1.6)};
}
inline double clampOrigin(double value, double low, double high, double size) {
    return std::max(low, std::min(value, std::max(low, high - size)));
}

constexpr int SpecialCount = 6;
enum class Activity { Idle, Walk, Sleep, Special };
struct Behavior {
    bool roaming = true, autoSleep = true, sleeping = false;
    int direction = -1;
    int special = -1;
    double specialRemaining = 0;
    double clock = 0, sinceInteraction = 0, phase = 0;
    Activity activity() const {
        if (special >= 0) return Activity::Special;
        if (sleeping) return Activity::Sleep;
        return roaming && phase >= 8 && phase < 13 ? Activity::Walk : Activity::Idle;
    }
    void interact() { sleeping = false; sinceInteraction = 0; phase = 0; special = -1; specialRemaining = 0; }
    void nap() { interact(); sleeping = true; }
    void playSpecial(int index) { if(index < 0 || index >= SpecialCount) return; interact(); special = index; specialRemaining = 8; }
    int randomSpecial(unsigned roll) const {
        if (special < 0 || special >= SpecialCount || SpecialCount == 1) return (int)(roll % SpecialCount);
        return (int)((special + 1u + roll % (SpecialCount - 1)) % SpecialCount);
    }
    int frame() const { return sleeping ? 2 + (int(clock / 3) % 2) : (int(clock * 6) % 2); }
    double advance(double dt, double x, double low, double high, double width, bool paused) {
        if (paused) return x;
        dt = std::max(0.0, std::min(dt, 2.0));
        clock += dt;
        if (special >= 0) {
            specialRemaining -= dt;
            if (specialRemaining <= 0) interact();
            return x;
        }
        if (sleeping) return x;
        sinceInteraction += dt;
        if (autoSleep && sinceInteraction >= 120) { nap(); return x; }
        phase = std::fmod(phase + dt, 18.0);
        if (activity() != Activity::Walk) return x;
        double next = x + direction * 30 * (width / 192.0) * dt;
        double right = std::max(low, high - width);
        if (next <= low) { next = low; direction = 1; }
        if (next >= right) { next = right; direction = -1; }
        return next;
    }
};
