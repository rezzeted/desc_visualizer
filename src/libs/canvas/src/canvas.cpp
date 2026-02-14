#include <canvas/canvas.hpp>
#include <diagram_placement/placer.hpp>
#include <diagram_placement/class_diagram_placement.hpp>
#include <diagram_render/renderer.hpp>
#include "imgui.h"
#include <cmath>

namespace {

const float class_button_size = 20.0f;
const float class_padding = 8.0f;
const float class_header_height = 28.0f;

} // namespace

namespace canvas {

DiagramCanvas::DiagramCanvas() = default;

DiagramCanvas::~DiagramCanvas() = default;

void DiagramCanvas::set_diagram(const diagram_model::Diagram* diagram) {
    diagram_ = diagram;
}

const diagram_model::Diagram* DiagramCanvas::diagram() const {
    return diagram_;
}

void DiagramCanvas::set_class_diagram(const diagram_model::ClassDiagram* class_diagram) {
    class_diagram_ = class_diagram;
}

const diagram_model::ClassDiagram* DiagramCanvas::class_diagram() const {
    return class_diagram_;
}

void DiagramCanvas::pan(float dx, float dy) {
    offset_x_ += dx;
    offset_y_ += dy;
}

void DiagramCanvas::zoom_at(float screen_x, float screen_y, float zoom_delta) {
    float new_zoom = zoom_ * zoom_delta;
    if (new_zoom < 0.1f) new_zoom = 0.1f;
    if (new_zoom > 10.0f) new_zoom = 10.0f;
    float factor = new_zoom / zoom_;
    offset_x_ = screen_x - (screen_x - offset_x_) * factor;
    offset_y_ = screen_y - (screen_y - offset_y_) * factor;
    zoom_ = new_zoom;
}

void DiagramCanvas::zoom_at_center(float zoom_delta) {
    zoom_ *= zoom_delta;
    if (zoom_ < 0.1f) zoom_ = 0.1f;
    if (zoom_ > 10.0f) zoom_ = 10.0f;
}

void DiagramCanvas::screen_to_world(float screen_x, float screen_y, double& world_x, double& world_y) const {
    world_x = (screen_x - offset_x_) / zoom_;
    world_y = (screen_y - offset_y_) / zoom_;
}

void DiagramCanvas::world_to_screen(double world_x, double world_y, float& screen_x, float& screen_y) const {
    screen_x = (float)world_x * zoom_ + offset_x_;
    screen_y = (float)world_y * zoom_ + offset_y_;
}

void DiagramCanvas::set_view_center(float screen_region_width, float screen_region_height) {
    offset_x_ = screen_region_width * 0.5f;
    offset_y_ = screen_region_height * 0.5f;
}

void DiagramCanvas::draw_grid(ImVec2 region_min, ImVec2 region_max) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (!dl) return;

    const unsigned int grid_color = IM_COL32(60, 60, 65, 255);
    const float grid_thickness = 1.0f;

    double left_world, top_world, right_world, bottom_world;
    screen_to_world(region_min.x, region_min.y, left_world, top_world);
    screen_to_world(region_max.x, region_max.y, right_world, bottom_world);

    float step = grid_step_ * zoom_;
    if (step < 8.0f) step = 8.0f;

    double start_x = std::floor(left_world / grid_step_) * grid_step_;
    double start_y = std::floor(top_world / grid_step_) * grid_step_;

    for (double wx = start_x; wx <= right_world + grid_step_; wx += grid_step_) {
        float sx1, sy1, sx2, sy2;
        world_to_screen(wx, top_world - 1000, sx1, sy1);
        world_to_screen(wx, bottom_world + 1000, sx2, sy2);
        dl->AddLine(ImVec2(sx1, sy1), ImVec2(sx2, sy2), grid_color, grid_thickness);
    }
    for (double wy = start_y; wy <= bottom_world + grid_step_; wy += grid_step_) {
        float sx1, sy1, sx2, sy2;
        world_to_screen(left_world - 1000, wy, sx1, sy1);
        world_to_screen(right_world + 1000, wy, sx2, sy2);
        dl->AddLine(ImVec2(sx1, sy1), ImVec2(sx2, sy2), grid_color, grid_thickness);
    }
}

bool DiagramCanvas::try_toggle_class_expanded(float screen_x, float screen_y) {
    if (!class_diagram_) return false;
    diagram_placement::PlacedClassDiagram placed = diagram_placement::place_class_diagram(*class_diagram_, class_expanded_);
    for (const auto& block : placed.blocks)
        class_rect_animator_.set_target(block.class_id, block.rect);
    double wx, wy;
    screen_to_world(screen_x, screen_y, wx, wy);
    for (const auto& block : placed.blocks) {
        diagram_placement::Rect cur = class_rect_animator_.get_current(block.class_id);
        double btn_x = cur.x + cur.width - class_padding - class_button_size;
        double btn_y = cur.y + (class_header_height - class_button_size) * 0.5;
        if (wx >= btn_x && wx <= btn_x + class_button_size && wy >= btn_y && wy <= btn_y + class_button_size) {
            bool& exp = class_expanded_[block.class_id];
            exp = !exp;
            return true;
        }
    }
    return false;
}

void DiagramCanvas::handle_input(float region_width, float region_height) {
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 mouse = io.MousePos;
    ImVec2 win_min = ImGui::GetWindowPos();
    ImVec2 win_max = ImVec2(win_min.x + region_width, win_min.y + region_height);

    bool in_region = mouse.x >= win_min.x && mouse.x <= win_max.x &&
                     mouse.y >= win_min.y && mouse.y <= win_max.y;

    if (ImGui::IsMouseClicked(0) && in_region) {
        if (try_toggle_class_expanded(mouse.x, mouse.y))
            return;
        dragging_ = true;
        drag_start_x_ = mouse.x;
        drag_start_y_ = mouse.y;
        drag_start_offset_x_ = offset_x_;
        drag_start_offset_y_ = offset_y_;
    }
    if (ImGui::IsMouseReleased(0)) {
        dragging_ = false;
    }
    if (dragging_) {
        offset_x_ = drag_start_offset_x_ + (mouse.x - drag_start_x_);
        offset_y_ = drag_start_offset_y_ + (mouse.y - drag_start_y_);
    }

    if (in_region && io.MouseWheel != 0.0f) {
        float local_x = mouse.x - win_min.x;
        float local_y = mouse.y - win_min.y;
        float factor = io.MouseWheel > 0 ? 1.2f : 1.0f / 1.2f;
        zoom_at(mouse.x, mouse.y, factor);
    }
}

bool DiagramCanvas::update_and_draw(float region_width, float region_height) {
    if (region_width <= 0 || region_height <= 0) return false;

    handle_input(region_width, region_height);

    ImVec2 region_min = ImGui::GetCursorScreenPos();
    ImVec2 region_max = ImVec2(region_min.x + region_width, region_min.y + region_height);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    if (!draw_list) return true;

    draw_grid(region_min, region_max);

    if (class_diagram_) {
        diagram_placement::PlacedClassDiagram placed = diagram_placement::place_class_diagram(*class_diagram_, class_expanded_);
        for (const auto& block : placed.blocks)
            class_rect_animator_.set_target(block.class_id, block.rect);
        class_rect_animator_.tick(ImGui::GetIO().DeltaTime);
        diagram_placement::PlacedClassDiagram displayed;
        for (const auto& block : placed.blocks) {
            diagram_placement::PlacedClassBlock b;
            b.class_id = block.class_id;
            b.rect = class_rect_animator_.get_current(block.class_id);
            b.margin = block.margin;
            b.expanded = block.expanded;
            displayed.blocks.push_back(b);
        }
        diagram_render::render_class_diagram(draw_list, *class_diagram_, displayed, offset_x_, offset_y_, zoom_);
    } else if (diagram_) {
        diagram_placement::PlacedDiagram placed = diagram_placement::place_diagram(*diagram_,
            (double)region_width, (double)region_height);
        diagram_render::render_diagram(draw_list, placed, offset_x_, offset_y_, zoom_);
    }

    return true;
}

} // namespace canvas
