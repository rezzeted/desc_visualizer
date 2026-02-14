#pragma once

#include <diagram_placement/types.hpp>
#include <string>
#include <unordered_map>

namespace animation {

class RectAnimator {
public:
    RectAnimator();
    void set_duration(float seconds) { duration_ = seconds; }
    float duration() const { return duration_; }

    void set_target(const std::string& id, const diagram_placement::Rect& target_rect);
    void tick(float dt);
    diagram_placement::Rect get_current(const std::string& id) const;
    void get_current_rects(std::unordered_map<std::string, diagram_placement::Rect>& out) const;

private:
    struct State {
        diagram_placement::Rect current;
        diagram_placement::Rect target;
    };
    std::unordered_map<std::string, State> state_;
    float duration_ = 0.18f;
};

} // namespace animation
