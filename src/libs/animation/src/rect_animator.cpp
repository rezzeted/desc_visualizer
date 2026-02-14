#include <animation/rect_animator.hpp>
#include <algorithm>
#include <cmath>

namespace animation {

RectAnimator::RectAnimator() = default;

void RectAnimator::set_target(const std::string& id, const diagram_placement::Rect& target_rect) {
    auto it = state_.find(id);
    if (it == state_.end()) {
        State s;
        s.current = target_rect;
        s.target = target_rect;
        state_[id] = s;
    } else {
        it->second.target = target_rect;
    }
}

static double ease_out(double t) {
    if (t >= 1.0) return 1.0;
    return 1.0 - (1.0 - t) * (1.0 - t);
}

void RectAnimator::tick(float dt) {
    if (dt <= 0.f) return;
    double step = static_cast<double>(dt) / static_cast<double>(duration_);
    step = std::min(1.0, step);
    step = ease_out(step);
    for (auto& [id, s] : state_) {
        s.current.x += (s.target.x - s.current.x) * step;
        s.current.y += (s.target.y - s.current.y) * step;
        s.current.width += (s.target.width - s.current.width) * step;
        s.current.height += (s.target.height - s.current.height) * step;
        const double eps = 0.5;
        if (std::abs(s.current.x - s.target.x) < eps && std::abs(s.current.y - s.target.y) < eps &&
            std::abs(s.current.width - s.target.width) < eps && std::abs(s.current.height - s.target.height) < eps)
            s.current = s.target;
    }
}

diagram_placement::Rect RectAnimator::get_current(const std::string& id) const {
    auto it = state_.find(id);
    if (it == state_.end()) return {};
    return it->second.current;
}

void RectAnimator::get_current_rects(std::unordered_map<std::string, diagram_placement::Rect>& out) const {
    out.clear();
    for (const auto& [id, s] : state_)
        out[id] = s.current;
}

} // namespace animation
