#include <diagram_placement/class_diagram_placement.hpp>
#include <diagram_placement/class_diagram_layout_constants.hpp>
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace diagram_placement {

namespace {

using namespace layout;

double estimate_text_width(const std::string& s) {
    return std::max(expanded_min_width - 2 * padding, static_cast<double>(s.size()) * 7.0);
}

// Inflated rect = BB expanded by margin; we want inflated rects to be at least `gap` apart.
void inflated_rect(double x, double y, double w, double h, double m,
    double& left, double& top, double& right, double& bottom)
{
    left = x - m;
    top = y - m;
    right = x + w + m;
    bottom = y + h + m;
}

bool rects_overlap(double x1, double y1, double w1, double h1, double m1,
    double x2, double y2, double w2, double h2, double m2)
{
    double l1, t1, r1, b1, l2, t2, r2, b2;
    inflated_rect(x1, y1, w1, h1, m1, l1, t1, r1, b1);
    inflated_rect(x2, y2, w2, h2, m2, l2, t2, r2, b2);
    return !(r1 + gap <= l2 || r2 + gap <= l1 || b1 + gap <= t2 || b2 + gap <= t1);
}

// Resolve overlap using minimum translation distance (MTD): push along a single axis
// (the one with smaller overlap) to minimize movement and avoid diagonal cascades.
void resolve_overlap(PlacedClassBlock& a, PlacedClassBlock& b, double relax)
{
    double l1, t1, r1, b1, l2, t2, r2, b2;
    inflated_rect(a.rect.x, a.rect.y, a.rect.width, a.rect.height, a.margin, l1, t1, r1, b1);
    inflated_rect(b.rect.x, b.rect.y, b.rect.width, b.rect.height, b.margin, l2, t2, r2, b2);
    double overlap_x = (std::min(r1, r2) - std::max(l1, l2)) + gap;
    double overlap_y = (std::min(b1, b2) - std::max(t1, t2)) + gap;
    if (overlap_x <= 0 && overlap_y <= 0) return;

    double cx1 = a.rect.x + a.rect.width * 0.5;
    double cy1 = a.rect.y + a.rect.height * 0.5;
    double cx2 = b.rect.x + b.rect.width * 0.5;
    double cy2 = b.rect.y + b.rect.height * 0.5;

    // Push along one axis only: choose the one with smaller overlap (MTD).
    if (overlap_x > 0 && overlap_y > 0) {
        if (overlap_x <= overlap_y) {
            overlap_y = 0;
        } else {
            overlap_x = 0;
        }
    }
    if (overlap_x > 0) {
        double dx = overlap_x * relax * 0.5;
        if (cx1 < cx2) {
            a.rect.x -= dx;
            b.rect.x += dx;
        } else {
            a.rect.x += dx;
            b.rect.x -= dx;
        }
    }
    if (overlap_y > 0) {
        double dy = overlap_y * relax * 0.5;
        if (cy1 < cy2) {
            a.rect.y -= dy;
            b.rect.y += dy;
        } else {
            a.rect.y += dy;
            b.rect.y -= dy;
        }
    }
}

} // namespace

PlacedClassDiagram place_class_diagram(const diagram_model::ClassDiagram& diagram,
    const std::unordered_map<std::string, bool>& expanded,
    const std::unordered_map<std::string, Rect>* block_sizes,
    const std::unordered_map<std::string, Rect>* previous_positions)
{
    PlacedClassDiagram out;
    if (diagram.classes.empty()) return out;

    std::unordered_map<std::string, const diagram_model::DiagramClass*> by_id;
    for (const auto& c : diagram.classes)
        by_id[c.id] = &c;

    double next_x = padding;
    double row_top = padding;
    double row_bottom = padding;

    for (const auto& c : diagram.classes) {
        PlacedClassBlock block;
        block.class_id = c.id;
        block.expanded = false;
        auto it = expanded.find(c.id);
        if (it != expanded.end()) block.expanded = it->second;

        double w, h;
        if (block_sizes) {
            auto sizes_it = block_sizes->find(c.id);
            if (sizes_it != block_sizes->end()) {
                w = sizes_it->second.width;
                h = sizes_it->second.height;
            } else {
                if (!block.expanded) {
                    w = collapsed_width;
                    h = collapsed_height;
                } else {
                    double content_w = estimate_text_width(c.type_name);
                    content_w = std::max(content_w, expanded_min_width);
                    content_w = std::max(content_w, estimate_text_width("Parent:"));
                    content_w = std::max(content_w, estimate_text_width("Properties:"));
                    content_w = std::max(content_w, estimate_text_width("Components:"));
                    content_w = std::max(content_w, estimate_text_width("Children:"));
                    if (!c.parent_class_id.empty()) {
                        auto pit = by_id.find(c.parent_class_id);
                        const std::string& parent_name = (pit != by_id.end()) ? pit->second->type_name : c.parent_class_id;
                        content_w = std::max(content_w, estimate_text_width(parent_name));
                    }
                    for (const auto& p : c.properties)
                        content_w = std::max(content_w, estimate_text_width(p.type + ": " + p.name));
                    for (const auto& comp : c.components)
                        content_w = std::max(content_w, estimate_text_width(comp.type + ": " + comp.name));
                    for (const auto& co : c.child_objects) {
                        auto cit = by_id.find(co.class_id);
                        const std::string& type_name = (cit != by_id.end()) ? cit->second->type_name : co.class_id;
                        content_w = std::max(content_w, estimate_text_width(type_name + ": " + (co.label.empty() ? type_name : co.label)));
                    }
                    w = content_w + 2 * padding + button_size;
                    h = header_height + content_inset_top;
                    h += header_content_gap;
                    h += expanded_content_height(
                        1u,
                        c.properties.size(),
                        c.components.size(),
                        c.child_objects.size());
                    h += content_inset_bottom;
                }
            }
        } else {
            if (!block.expanded) {
                w = collapsed_width;
                h = collapsed_height;
            } else {
                double content_w = estimate_text_width(c.type_name);
                content_w = std::max(content_w, expanded_min_width);
                content_w = std::max(content_w, estimate_text_width("Parent:"));
                content_w = std::max(content_w, estimate_text_width("Properties:"));
                content_w = std::max(content_w, estimate_text_width("Components:"));
                content_w = std::max(content_w, estimate_text_width("Children:"));
                if (!c.parent_class_id.empty()) {
                    auto pit = by_id.find(c.parent_class_id);
                    const std::string& parent_name = (pit != by_id.end()) ? pit->second->type_name : c.parent_class_id;
                    content_w = std::max(content_w, estimate_text_width(parent_name));
                }
                for (const auto& p : c.properties)
                    content_w = std::max(content_w, estimate_text_width(p.type + ": " + p.name));
                for (const auto& comp : c.components)
                    content_w = std::max(content_w, estimate_text_width(comp.type + ": " + comp.name));
                for (const auto& co : c.child_objects) {
                    auto cit = by_id.find(co.class_id);
                    const std::string& type_name = (cit != by_id.end()) ? cit->second->type_name : co.class_id;
                    content_w = std::max(content_w, estimate_text_width(type_name + ": " + (co.label.empty() ? type_name : co.label)));
                }
                w = content_w + 2 * padding + button_size;
                h = header_height + content_inset_top;
                h += header_content_gap;
                h += expanded_content_height(
                    1u,
                    c.properties.size(),
                    c.components.size(),
                    c.child_objects.size());
                h += content_inset_bottom;
            }
        }

        block.rect.width = w;
        block.rect.height = h;
        block.margin = c.margin;

        if (c.x != 0 || c.y != 0) {
            block.rect.x = c.x;
            block.rect.y = c.y;
        } else if (previous_positions) {
            auto prev_it = previous_positions->find(c.id);
            if (prev_it != previous_positions->end()) {
                block.rect.x = prev_it->second.x;
                block.rect.y = prev_it->second.y;
                row_bottom = std::max(row_bottom, block.rect.y + h);
            } else {
                double place_x = next_x;
                double place_y = row_top;
            for (const auto& existing : out.blocks) {
                if (rects_overlap(place_x, place_y, w, h, block.margin,
                        existing.rect.x, existing.rect.y, existing.rect.width, existing.rect.height, existing.margin)) {
                    row_top = row_bottom + gap;
                    place_x = padding;
                    place_y = row_top;
                    break;
                }
            }
            block.rect.x = place_x;
            block.rect.y = place_y;
            next_x = place_x + w + block_margin;
            row_bottom = std::max(row_bottom, place_y + h);
            }
        } else {
            double place_x = next_x;
            double place_y = row_top;
            for (const auto& existing : out.blocks) {
                if (rects_overlap(place_x, place_y, w, h, block.margin,
                        existing.rect.x, existing.rect.y, existing.rect.width, existing.rect.height, existing.margin)) {
                    row_top = row_bottom + gap;
                    place_x = padding;
                    place_y = row_top;
                    break;
                }
            }
            block.rect.x = place_x;
            block.rect.y = place_y;
            next_x = place_x + w + block_margin;
            row_bottom = std::max(row_bottom, place_y + h);
        }

        out.blocks.push_back(std::move(block));
    }

    // Push propagation: overlapping blocks push each other apart (like balls in a box).
    // Both move by half the overlap; relaxation (0.5) avoids oscillation; many iterations
    // let the push propagate (A pushes B, B pushes C, ...).
    const double relax = 0.5;
    const int max_iter = 120;
    for (int iter = 0; iter < max_iter; ++iter) {
        bool any_overlap = false;
        for (size_t i = 0; i < out.blocks.size(); ++i) {
            for (size_t j = i + 1; j < out.blocks.size(); ++j) {
                double l1, t1, r1, b1, l2, t2, r2, b2;
                inflated_rect(out.blocks[i].rect.x, out.blocks[i].rect.y,
                    out.blocks[i].rect.width, out.blocks[i].rect.height, out.blocks[i].margin,
                    l1, t1, r1, b1);
                inflated_rect(out.blocks[j].rect.x, out.blocks[j].rect.y,
                    out.blocks[j].rect.width, out.blocks[j].rect.height, out.blocks[j].margin,
                    l2, t2, r2, b2);
                if (r1 + gap <= l2 || r2 + gap <= l1 || b1 + gap <= t2 || b2 + gap <= t1)
                    continue;
                any_overlap = true;
                resolve_overlap(out.blocks[i], out.blocks[j], relax);
            }
        }
        if (!any_overlap) break;
    }

    return out;
}

} // namespace diagram_placement
