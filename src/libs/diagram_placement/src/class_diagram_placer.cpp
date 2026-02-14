#include <diagram_placement/class_diagram_placement.hpp>
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace diagram_placement {

namespace {

const double collapsed_width = 140;
const double collapsed_height = 32;
const double expanded_min_width = 180;
const double header_height = 28;
const double section_header_height = 20;
const double row_height = 18;
const double padding = 8;
const double button_size = 20;
const double block_margin = 16;
const double gap = 8;
const double section_gap = 2;
const double group_gap = 6;
const double content_inset_top = 6;
const double content_inset_bottom = 10;

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

// Resolve overlap by pushing both blocks apart (like physical bodies). Relaxation factor
// to avoid oscillation; repeated iterations let pushes propagate (A pushes B, B pushes C).
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
    const std::unordered_map<std::string, bool>& expanded)
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
        if (!block.expanded) {
            w = collapsed_width;
            h = collapsed_height;
        } else {
            double content_w = estimate_text_width(c.type_name);
            size_t rows = 0;
            if (!c.parent_class_id.empty()) rows += 1;
            rows += c.properties.size();
            rows += c.components.size();
            rows += c.child_objects.size();
            content_w = std::max(content_w, expanded_min_width);
            for (const auto& p : c.properties)
                content_w = std::max(content_w, estimate_text_width(p.name + ": " + p.type));
            for (const auto& comp : c.components)
                content_w = std::max(content_w, estimate_text_width(comp.name + ": " + comp.type));
            for (const auto& co : c.child_objects)
                content_w = std::max(content_w, estimate_text_width(co.label.empty() ? co.class_id : co.label + " (" + co.class_id + ")"));
            w = content_w + 2 * padding + button_size;
            h = header_height + content_inset_top;
            size_t parent_rows = c.parent_class_id.empty() ? 1 : 1;
            size_t prop_rows = c.properties.empty() ? 1 : c.properties.size();
            size_t comp_rows = c.components.empty() ? 1 : c.components.size();
            size_t child_rows = c.child_objects.empty() ? 1 : c.child_objects.size();
            h += 4.0; /* gap after header line */
            h += section_header_height + parent_rows * row_height + section_gap + group_gap;
            h += section_header_height + prop_rows * row_height + section_gap + group_gap;
            h += section_header_height + comp_rows * row_height + section_gap;
            h += section_header_height + child_rows * row_height;
            h += content_inset_bottom;
        }

        block.rect.width = w;
        block.rect.height = h;
        block.rect.x = c.x != 0 || c.y != 0 ? c.x : next_x;
        block.rect.y = c.x != 0 || c.y != 0 ? c.y : row_top;
        block.margin = c.margin;

        if (c.x == 0 && c.y == 0) {
            next_x += w + block_margin;
            row_bottom = std::max(row_bottom, row_top + h);
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
