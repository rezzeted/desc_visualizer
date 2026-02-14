#pragma once

#include <diagram_model/types.hpp>
#include <diagram_model/class_diagram.hpp>
#include <diagram_placement/physics_layout.hpp>
#include <diagram_render/nested_hit_button.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct ImVec2;

namespace canvas {

class DiagramCanvas {
public:
    DiagramCanvas();
    ~DiagramCanvas();

    void set_diagram(const diagram_model::Diagram* diagram);
    const diagram_model::Diagram* diagram() const;

    void set_class_diagram(const diagram_model::ClassDiagram* class_diagram);
    const diagram_model::ClassDiagram* class_diagram() const;
    std::unordered_map<std::string, bool>& class_expanded() { return class_expanded_; }
    const std::unordered_map<std::string, bool>& class_expanded() const { return class_expanded_; }
    bool set_class_block_expanded(const std::string& class_id, bool expanded);

    std::unordered_map<std::string, bool>& nested_expanded() { return nested_expanded_; }
    const std::unordered_map<std::string, bool>& nested_expanded() const { return nested_expanded_; }

    void focus_on_class(const std::string& class_id);

    void set_grid_step(float step) { grid_step_ = step; }
    float grid_step() const { return grid_step_; }

    void pan(float dx, float dy);
    void zoom_at(float screen_x, float screen_y, float zoom_delta);
    void zoom_at_center(float zoom_delta);

    void screen_to_world(float screen_x, float screen_y, double& world_x, double& world_y) const;
    void world_to_screen(double world_x, double world_y, float& screen_x, float& screen_y) const;

    void set_view_center(float screen_region_width, float screen_region_height);
    void set_offset(float ox, float oy) { offset_x_ = ox; offset_y_ = oy; }
    void set_zoom(float z) { zoom_ = z; }
    float offset_x() const { return offset_x_; }
    float offset_y() const { return offset_y_; }
    float zoom() const { return zoom_; }
    std::size_t current_overlap_count() const { return active_overlap_pairs_.size(); }
    bool is_layout_settled() const { return physics_layout_.is_settled(); }

    bool update_and_draw(float region_width, float region_height);

private:
    const diagram_model::Diagram* diagram_ = nullptr;
    const diagram_model::ClassDiagram* class_diagram_ = nullptr;
    std::unordered_map<std::string, bool> class_expanded_;
    std::unordered_map<std::string, bool> nested_expanded_;
    std::vector<diagram_render::NestedHitButton> nested_hit_buttons_;
    std::vector<diagram_render::NavHitButton> nav_hit_buttons_;
    std::vector<diagram_render::ClassHoverRegion> hover_regions_;
    std::string hovered_class_id_;
    diagram_placement::PhysicsLayout physics_layout_;
    float offset_x_ = 0;
    float offset_y_ = 0;
    float zoom_ = 1.0f;
    float grid_step_ = 40.0f;
    float last_region_width_ = 0;
    float last_region_height_ = 0;
    bool dragging_ = false;
    float drag_start_x_ = 0;
    float drag_start_y_ = 0;
    float drag_start_offset_x_ = 0;
    float drag_start_offset_y_ = 0;
    bool dragging_block_ = false;
    std::string dragged_block_id_;
    double dragged_block_offset_x_ = 0.0;
    double dragged_block_offset_y_ = 0.0;
    std::unordered_set<std::string> active_overlap_pairs_;
    bool settle_error_reported_ = false;

    void draw_grid(ImVec2 region_min, ImVec2 region_max);
    void handle_input(float region_width, float region_height);
    bool try_toggle_class_expanded(float screen_x, float screen_y);
    void log_visual_overlaps(const diagram_placement::PlacedClassDiagram& displayed);
};

} // namespace canvas
