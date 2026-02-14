#include <canvas/canvas.hpp>
#include <diagram_placement/placer.hpp>
#include <diagram_placement/class_diagram_layout_constants.hpp>
#include <diagram_render/renderer.hpp>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include "imgui.h"
#include <cmath>
#include <filesystem>
#include <memory>
#include <vector>

namespace {

const float class_button_size = 20.0f;
const float class_padding = 8.0f;
const float class_header_height = 28.0f;

struct Obb {
    double cx = 0.0;
    double cy = 0.0;
    double ex = 0.0;
    double ey = 0.0;
    double angle = 0.0;
};

std::filesystem::path find_project_root() {
    std::filesystem::path p = std::filesystem::current_path();
    for (int i = 0; i < 8; ++i) {
        if (std::filesystem::exists(p / "CMakeLists.txt") && std::filesystem::exists(p / "src")) {
            return p;
        }
        if (!p.has_parent_path()) break;
        p = p.parent_path();
    }
    return std::filesystem::current_path();
}

std::shared_ptr<spdlog::logger> overlap_logger() {
    static std::shared_ptr<spdlog::logger> logger;
    if (logger) return logger;

    try {
        const std::filesystem::path logs_dir = find_project_root() / "logs";
        std::filesystem::create_directories(logs_dir);
        const std::filesystem::path log_file = logs_dir / "diagram_overlap_latest.log";
        logger = spdlog::basic_logger_mt("diagram_overlap_logger", log_file.string(), true);
        logger->set_level(spdlog::level::info);
        logger->flush_on(spdlog::level::info);
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
        logger->info("Overlap logger initialized. file={}", log_file.string());
    } catch (const spdlog::spdlog_ex&) {
        logger = spdlog::default_logger();
    }
    return logger;
}

Obb to_obb(const diagram_placement::Rect& r) {
    Obb o;
    o.cx = r.x + r.width * 0.5;
    o.cy = r.y + r.height * 0.5;
    o.ex = r.width * 0.5;
    o.ey = r.height * 0.5;
    o.angle = 0.0;
    return o;
}

void obb_axes(const Obb& o, double& ax1x, double& ax1y, double& ax2x, double& ax2y) {
    const double c = std::cos(o.angle);
    const double s = std::sin(o.angle);
    ax1x = c;  ax1y = s;
    ax2x = -s; ax2y = c;
}

double projected_radius(const Obb& o, double lx, double ly) {
    double a1x, a1y, a2x, a2y;
    obb_axes(o, a1x, a1y, a2x, a2y);
    const double p1 = std::abs(a1x * lx + a1y * ly) * o.ex;
    const double p2 = std::abs(a2x * lx + a2y * ly) * o.ey;
    return p1 + p2;
}

bool obb_intersects(const Obb& a, const Obb& b) {
    const double dx = b.cx - a.cx;
    const double dy = b.cy - a.cy;
    double a1x, a1y, a2x, a2y, b1x, b1y, b2x, b2y;
    obb_axes(a, a1x, a1y, a2x, a2y);
    obb_axes(b, b1x, b1y, b2x, b2y);

    const std::vector<std::pair<double, double>> axes = {
        {a1x, a1y}, {a2x, a2y}, {b1x, b1y}, {b2x, b2y}
    };
    for (const auto& axis : axes) {
        const double lx = axis.first;
        const double ly = axis.second;
        const double dist = std::abs(dx * lx + dy * ly);
        const double ra = projected_radius(a, lx, ly);
        const double rb = projected_radius(b, lx, ly);
        if (dist > ra + rb) {
            return false;
        }
    }
    return true;
}

std::string pair_key(const std::string& a, const std::string& b) {
    if (a < b) return a + "|" + b;
    return b + "|" + a;
}

bool pick_block_at(const diagram_placement::PlacedClassDiagram& placed,
    double wx,
    double wy,
    std::string& out_id,
    diagram_placement::Rect& out_rect)
{
    for (auto it = placed.blocks.rbegin(); it != placed.blocks.rend(); ++it) {
        const auto& b = *it;
        if (wx >= b.rect.x && wx <= b.rect.x + b.rect.width &&
            wy >= b.rect.y && wy <= b.rect.y + b.rect.height)
        {
            out_id = b.class_id;
            out_rect = b.rect;
            return true;
        }
    }
    return false;
}

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
    dragging_block_ = false;
    dragged_block_id_.clear();
    active_overlap_pairs_.clear();
    settle_error_reported_ = false;
    connection_lines_dirty_ = true;
    if (!class_diagram_) return;

    auto block_sizes = diagram_render::compute_class_block_sizes(*class_diagram_, class_expanded_, nested_expanded_);
    physics_layout_.build(*class_diagram_, class_expanded_, &block_sizes);
}

bool DiagramCanvas::set_class_block_expanded(const std::string& class_id, bool expanded) {
    if (!class_diagram_) return false;
    bool found = false;
    for (const auto& c : class_diagram_->classes) {
        if (c.id == class_id) {
            found = true;
            break;
        }
    }
    if (!found) return false;

    class_expanded_[class_id] = expanded;
    auto block_sizes = diagram_render::compute_class_block_sizes(*class_diagram_, class_expanded_, nested_expanded_);
    auto it = block_sizes.find(class_id);
    if (it != block_sizes.end()) {
        physics_layout_.update_block_size(class_id, it->second.width, it->second.height, expanded);
    } else {
        physics_layout_.build(*class_diagram_, class_expanded_, &block_sizes);
    }
    settle_error_reported_ = false;
    connection_lines_dirty_ = true;
    return true;
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
    diagram_placement::PlacedClassDiagram placed = physics_layout_.get_placed();
    double wx, wy;
    screen_to_world(screen_x, screen_y, wx, wy);

    // Check main expand/collapse buttons on block headers.
    for (const auto& block : placed.blocks) {
        const auto& cur = block.rect;
        double btn_x = cur.x + cur.width - class_padding - class_button_size;
        double btn_y = cur.y + (class_header_height - class_button_size) * 0.5;
        if (wx >= btn_x && wx <= btn_x + class_button_size && wy >= btn_y && wy <= btn_y + class_button_size) {
            bool& exp = class_expanded_[block.class_id];
            exp = !exp;
            auto block_sizes = diagram_render::compute_class_block_sizes(*class_diagram_, class_expanded_, nested_expanded_);
            auto it = block_sizes.find(block.class_id);
            if (it != block_sizes.end()) {
                physics_layout_.update_block_size(block.class_id, it->second.width, it->second.height, exp);
            } else {
                physics_layout_.build(*class_diagram_, class_expanded_, &block_sizes);
            }
            return true;
        }
    }

    // Check nested expand/collapse buttons (recorded during last render pass).
    for (const auto& hb : nested_hit_buttons_) {
        if (wx >= hb.x && wx <= hb.x + hb.w && wy >= hb.y && wy <= hb.y + hb.h) {
            bool& exp = nested_expanded_[hb.path];
            exp = !exp;
            // Recompute the owning block's size.
            auto block_sizes = diagram_render::compute_class_block_sizes(*class_diagram_, class_expanded_, nested_expanded_);
            auto it = block_sizes.find(hb.block_class_id);
            if (it != block_sizes.end()) {
                bool block_exp = false;
                auto eit = class_expanded_.find(hb.block_class_id);
                if (eit != class_expanded_.end()) block_exp = eit->second;
                physics_layout_.update_block_size(hb.block_class_id,
                    it->second.width, it->second.height, block_exp);
            } else {
                physics_layout_.build(*class_diagram_, class_expanded_, &block_sizes);
            }
            settle_error_reported_ = false;
            connection_lines_dirty_ = true;
            return true;
        }
    }

    // Check navigation arrow buttons (recorded during last render pass).
    for (const auto& nb : nav_hit_buttons_) {
        if (wx >= nb.x && wx <= nb.x + nb.w && wy >= nb.y && wy <= nb.y + nb.h) {
            // Expand the target class card if it isn't already open.
            auto it = class_expanded_.find(nb.target_class_id);
            if (it == class_expanded_.end() || !it->second) {
                set_class_block_expanded(nb.target_class_id, true);
            }
            focus_on_class(nb.target_class_id);
            return true;
        }
    }

    return false;
}

void DiagramCanvas::focus_on_class(const std::string& class_id) {
    auto placed = physics_layout_.get_placed();
    for (const auto& block : placed.blocks) {
        if (block.class_id == class_id) {
            double cx = block.rect.x + block.rect.width * 0.5;
            double cy = block.rect.y + block.rect.height * 0.5;
            offset_x_ = last_region_width_ * 0.5f - static_cast<float>(cx) * zoom_;
            offset_y_ = last_region_height_ * 0.5f - static_cast<float>(cy) * zoom_;
            return;
        }
    }
}

void DiagramCanvas::handle_input(float region_width, float region_height) {
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 mouse = io.MousePos;
    ImVec2 win_min = ImGui::GetWindowPos();
    ImVec2 win_max = ImVec2(win_min.x + region_width, win_min.y + region_height);

    bool in_region = mouse.x >= win_min.x && mouse.x <= win_max.x &&
                     mouse.y >= win_min.y && mouse.y <= win_max.y;

    double wx = 0.0;
    double wy = 0.0;
    screen_to_world(mouse.x, mouse.y, wx, wy);

    if (ImGui::IsMouseClicked(0) && in_region) {
        if (try_toggle_class_expanded(mouse.x, mouse.y))
            return;

        if (class_diagram_ && io.KeyAlt) {
            std::string hit_id;
            diagram_placement::Rect hit_rect;
            auto placed = physics_layout_.get_placed();
            if (pick_block_at(placed, wx, wy, hit_id, hit_rect)) {
                dragging_block_ = true;
                dragged_block_id_ = hit_id;
                dragged_block_offset_x_ = wx - hit_rect.x;
                dragged_block_offset_y_ = wy - hit_rect.y;
                physics_layout_.begin_drag(hit_id);
                dragging_ = false;
                return;
            }
        }

        dragging_ = true;
        drag_start_x_ = mouse.x;
        drag_start_y_ = mouse.y;
        drag_start_offset_x_ = offset_x_;
        drag_start_offset_y_ = offset_y_;
    }

    if (ImGui::IsMouseClicked(1) && in_region && class_diagram_) {
        std::string hit_id;
        diagram_placement::Rect hit_rect;
        auto placed = physics_layout_.get_placed();
        if (pick_block_at(placed, wx, wy, hit_id, hit_rect)) {
            dragging_block_ = true;
            dragged_block_id_ = hit_id;
            dragged_block_offset_x_ = wx - hit_rect.x;
            dragged_block_offset_y_ = wy - hit_rect.y;
            physics_layout_.begin_drag(hit_id);
            dragging_ = false;
        }
    }

    if (ImGui::IsMouseReleased(0)) {
        dragging_ = false;
        if (dragging_block_) {
            physics_layout_.end_drag(dragged_block_id_);
            dragging_block_ = false;
            dragged_block_id_.clear();
        }
    }
    if (ImGui::IsMouseReleased(1) && dragging_block_) {
        physics_layout_.end_drag(dragged_block_id_);
        dragging_block_ = false;
        dragged_block_id_.clear();
    }

    if (dragging_block_ && !dragged_block_id_.empty()) {
        physics_layout_.drag_to(
            dragged_block_id_,
            wx - dragged_block_offset_x_,
            wy - dragged_block_offset_y_);
        return;
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

    last_region_width_ = region_width;
    last_region_height_ = region_height;

    handle_input(region_width, region_height);

    ImVec2 region_min = ImGui::GetCursorScreenPos();
    ImVec2 region_max = ImVec2(region_min.x + region_width, region_min.y + region_height);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    if (!draw_list) return true;

    draw_grid(region_min, region_max);

    if (class_diagram_) {
        physics_layout_.step(ImGui::GetIO().DeltaTime);
        diagram_placement::PlacedClassDiagram displayed = physics_layout_.get_placed();
        log_visual_overlaps(displayed);

        // Recompute connection lines when layout has changed, flagged dirty, or block is being dragged.
        if (connection_lines_dirty_ || !physics_layout_.is_settled() || dragging_block_) {
            connection_lines_ = diagram_placement::compute_connection_lines(*class_diagram_, displayed);
            if (physics_layout_.is_settled() && !dragging_block_) connection_lines_dirty_ = false;
        }

        // Detect hover: block-level (highlight parents) + row-level (highlight specific target).
        hovered_class_id_.clear();
        highlighted_class_ids_.clear();
        {
            ImGuiIO& io = ImGui::GetIO();
            ImVec2 mouse = io.MousePos;
            ImVec2 win_min_pt = ImGui::GetWindowPos();
            ImVec2 win_max_pt = ImVec2(win_min_pt.x + region_width, win_min_pt.y + region_height);
            bool mouse_in_region = mouse.x >= win_min_pt.x && mouse.x <= win_max_pt.x &&
                                   mouse.y >= win_min_pt.y && mouse.y <= win_max_pt.y;
            if (mouse_in_region) {
                double mx, my;
                screen_to_world(mouse.x, mouse.y, mx, my);

                // Header hover: if mouse is over a block's header, highlight all its parents.
                constexpr double hdr_h = diagram_placement::layout::header_height;
                for (const auto& block : displayed.blocks) {
                    double bx = block.rect.x, by = block.rect.y;
                    double bw = block.rect.width;
                    if (mx >= bx && mx <= bx + bw && my >= by && my <= by + hdr_h) {
                        hovered_class_id_ = block.class_id;
                        for (const auto& cls : class_diagram_->classes) {
                            if (cls.id == block.class_id) {
                                for (const auto& pid : cls.parent_class_ids)
                                    highlighted_class_ids_.insert(pid);
                                break;
                            }
                        }
                        break;
                    }
                }

                // Row-level hover regions: add specific targets to the highlighted set.
                for (const auto& hr : hover_regions_) {
                    if (mx >= hr.x && mx <= hr.x + hr.w && my >= hr.y && my <= hr.y + hr.h) {
                        highlighted_class_ids_.insert(hr.target_class_id);
                        break;
                    }
                }
            }
        }

        nested_hit_buttons_.clear();
        nav_hit_buttons_.clear();
        hover_regions_.clear();
        diagram_render::render_class_diagram(draw_list, *class_diagram_, displayed,
            offset_x_, offset_y_, zoom_, nested_expanded_, &nested_hit_buttons_, &nav_hit_buttons_,
            &hover_regions_, hovered_class_id_, connection_lines_, highlighted_class_ids_);
    } else if (diagram_) {
        diagram_placement::PlacedDiagram placed = diagram_placement::place_diagram(*diagram_,
            (double)region_width, (double)region_height);
        diagram_render::render_diagram(draw_list, placed, offset_x_, offset_y_, zoom_);
    }

    return true;
}

void DiagramCanvas::log_visual_overlaps(const diagram_placement::PlacedClassDiagram& displayed) {
    auto logger = overlap_logger();
    std::unordered_set<std::string> current_pairs;

    for (std::size_t i = 0; i < displayed.blocks.size(); ++i) {
        const auto& a = displayed.blocks[i];
        const Obb obb_a = to_obb(a.rect);
        for (std::size_t j = i + 1; j < displayed.blocks.size(); ++j) {
            const auto& b = displayed.blocks[j];
            const Obb obb_b = to_obb(b.rect);
            if (!obb_intersects(obb_a, obb_b)) {
                continue;
            }

            const std::string key = pair_key(a.class_id, b.class_id);
            current_pairs.insert(key);
            if (active_overlap_pairs_.find(key) == active_overlap_pairs_.end()) {
                logger->warn(
                    "overlap_detected pair={} a={} b={} "
                    "a_rect=({}, {}, {}, {}) b_rect=({}, {}, {}, {})",
                    key, a.class_id, b.class_id,
                    a.rect.x, a.rect.y, a.rect.width, a.rect.height,
                    b.rect.x, b.rect.y, b.rect.width, b.rect.height);
            }
        }
    }

    for (const auto& key : active_overlap_pairs_) {
        if (current_pairs.find(key) == current_pairs.end()) {
            logger->info("overlap_resolved pair={}", key);
        }
    }

    active_overlap_pairs_ = std::move(current_pairs);

    if (physics_layout_.is_settled()) {
        if (!active_overlap_pairs_.empty() && !settle_error_reported_) {
            logger->error(
                "settle_failed overlap_count={} pairs_unresolved={}",
                active_overlap_pairs_.size(),
                active_overlap_pairs_.size());
            for (const auto& pair : active_overlap_pairs_) {
                logger->error("settle_failed_pair pair={}", pair);
            }
            settle_error_reported_ = true;
        } else if (active_overlap_pairs_.empty()) {
            settle_error_reported_ = false;
        }
    } else {
        settle_error_reported_ = false;
    }
}

} // namespace canvas
