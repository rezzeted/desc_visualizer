#pragma once

#include <diagram_model/class_diagram.hpp>
#include <diagram_placement/class_diagram_placement.hpp>
#include <diagram_placement/types.hpp>
#include <box2d/box2d.h>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace diagram_placement {

class PhysicsLayout {
public:
    PhysicsLayout();
    ~PhysicsLayout();

    void build(const diagram_model::ClassDiagram& diagram,
        const std::unordered_map<std::string, bool>& expanded,
        const std::unordered_map<std::string, Rect>* block_sizes);

    void step(float dt);
    PlacedClassDiagram get_placed() const;

    void update_block_size(const std::string& id, double w, double h, bool expanded);

    void begin_drag(const std::string& id);
    void drag_to(const std::string& id, double wx, double wy);
    void end_drag(const std::string& id);

    bool is_settled() const;

private:
    struct BodyState {
        b2BodyId body_id = b2_nullBodyId;
        b2ShapeId shape_id = b2_nullShapeId;
        Rect rect;
        double margin = 8.0;
        bool expanded = false;
    };

    struct ResizeAnim {
        std::string block_id;
        double from_w, from_h;
        double to_w, to_h;
        double anchor_x, anchor_y;
        float progress = 0.0f;
    };

    void destroy_world();
    void build_world(const std::unordered_map<std::string, Rect>* previous_positions);
    void collect_current_positions(std::unordered_map<std::string, Rect>& out) const;
    void request_settle();
    void warmup_settle(int steps);

    const diagram_model::ClassDiagram* diagram_ = nullptr;
    std::unordered_map<std::string, bool> expanded_;
    std::unordered_map<std::string, Rect> sizes_;
    std::unordered_map<std::string, BodyState> blocks_;
    std::unordered_map<std::string, std::size_t> order_;
    std::unordered_map<std::string, double> margins_;
    std::vector<ResizeAnim> active_anims_;

    b2WorldId world_id_ = b2_nullWorldId;
    std::string dragged_id_;
    int settle_steps_remaining_ = 0;

    static constexpr float kAnimSpeed = 4.0f;
};

} // namespace diagram_placement
