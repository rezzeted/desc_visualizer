#pragma once

#include <diagram_model/types.hpp>
#include <memory>

struct ImVec2;

namespace canvas {

class DiagramCanvas {
public:
    DiagramCanvas();
    ~DiagramCanvas();

    void set_diagram(const diagram_model::Diagram* diagram);
    const diagram_model::Diagram* diagram() const;

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

    bool update_and_draw(float region_width, float region_height);

private:
    const diagram_model::Diagram* diagram_ = nullptr;
    float offset_x_ = 0;
    float offset_y_ = 0;
    float zoom_ = 1.0f;
    float grid_step_ = 40.0f;
    bool dragging_ = false;
    float drag_start_x_ = 0;
    float drag_start_y_ = 0;
    float drag_start_offset_x_ = 0;
    float drag_start_offset_y_ = 0;

    void draw_grid(ImVec2 region_min, ImVec2 region_max);
    void handle_input(float region_width, float region_height);
};

} // namespace canvas
