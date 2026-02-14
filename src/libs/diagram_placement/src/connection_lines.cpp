#include <diagram_placement/connection_lines.hpp>
#include <cmath>
#include <unordered_map>

namespace diagram_placement {

namespace {

struct BlockRect {
    double x, y, w, h;
    double cx() const { return x + w * 0.5; }
    double cy() const { return y + h * 0.5; }
    double top() const { return y; }
    double bottom() const { return y + h; }
    double left() const { return x; }
    double right() const { return x + w; }
};

// Find the placed rect for a given class_id.  Returns false if not found.
bool find_block_rect(const PlacedClassDiagram& placed, const std::string& id, BlockRect& out) {
    for (const auto& b : placed.blocks) {
        if (b.class_id == id) {
            out = { b.rect.x, b.rect.y, b.rect.width, b.rect.height };
            return true;
        }
    }
    return false;
}

// Choose best anchor pair between two blocks.
// For inheritance (parent above child): bottom-center of parent -> top-center of child.
// For composition: choose closest edge pair (left/right/top/bottom centers).
struct AnchorPair {
    double x1, y1; // from anchor
    double x2, y2; // to anchor
};

AnchorPair inheritance_anchors(const BlockRect& child_block, const BlockRect& parent_block) {
    AnchorPair a;
    a.x1 = child_block.cx();
    a.y1 = child_block.top();
    a.x2 = parent_block.cx();
    a.y2 = parent_block.bottom();
    return a;
}

AnchorPair closest_anchors(const BlockRect& from, const BlockRect& to) {
    // 4 candidate pairs: from-right to to-left, from-left to to-right,
    // from-bottom to to-top, from-top to to-bottom.
    struct Candidate { double x1, y1, x2, y2; double dist; };
    Candidate candidates[4] = {
        { from.right(), from.cy(), to.left(), to.cy(), 0.0 },
        { from.left(), from.cy(), to.right(), to.cy(), 0.0 },
        { from.cx(), from.bottom(), to.cx(), to.top(), 0.0 },
        { from.cx(), from.top(), to.cx(), to.bottom(), 0.0 },
    };
    for (auto& c : candidates) {
        double dx = c.x2 - c.x1;
        double dy = c.y2 - c.y1;
        c.dist = dx * dx + dy * dy;
    }
    const Candidate* best = &candidates[0];
    for (int i = 1; i < 4; ++i) {
        if (candidates[i].dist < best->dist) best = &candidates[i];
    }
    return { best->x1, best->y1, best->x2, best->y2 };
}

} // namespace

std::vector<ConnectionLine> compute_connection_lines(
    const diagram_model::ClassDiagram& diagram,
    const PlacedClassDiagram& placed)
{
    std::vector<ConnectionLine> lines;

    for (const auto& cls : diagram.classes) {
        BlockRect child_rect;
        if (!find_block_rect(placed, cls.id, child_rect)) continue;

        // Inheritance lines.
        for (std::size_t pi = 0; pi < cls.parent_class_ids.size(); ++pi) {
            BlockRect parent_rect;
            if (!find_block_rect(placed, cls.parent_class_ids[pi], parent_rect)) continue;

            ConnectionLine line;
            line.from_class_id = cls.id;
            line.to_class_id = cls.parent_class_ids[pi];
            line.kind = (pi == 0) ? ConnectionKind::PrimaryInheritance
                                  : ConnectionKind::SecondaryInheritance;

            AnchorPair a = inheritance_anchors(child_rect, parent_rect);
            line.points.push_back({a.x1, a.y1});

            // If not roughly vertical, add a midpoint for an orthogonal bend.
            double mid_y = (a.y1 + a.y2) * 0.5;
            if (std::abs(a.x2 - a.x1) > 5.0) {
                line.points.push_back({a.x1, mid_y});
                line.points.push_back({a.x2, mid_y});
            }

            line.points.push_back({a.x2, a.y2});
            lines.push_back(std::move(line));
        }

        // Composition lines (child_objects).
        for (const auto& co : cls.child_objects) {
            BlockRect target_rect;
            if (!find_block_rect(placed, co.class_id, target_rect)) continue;

            ConnectionLine line;
            line.from_class_id = cls.id;
            line.to_class_id = co.class_id;
            line.kind = ConnectionKind::Composition;
            line.label = co.label;

            AnchorPair a = closest_anchors(child_rect, target_rect);
            line.points.push_back({a.x1, a.y1});

            // Orthogonal routing: add midpoint bend.
            double mid_x = (a.x1 + a.x2) * 0.5;
            double mid_y = (a.y1 + a.y2) * 0.5;
            bool horizontal = std::abs(a.x2 - a.x1) > std::abs(a.y2 - a.y1);
            if (horizontal) {
                line.points.push_back({mid_x, a.y1});
                line.points.push_back({mid_x, a.y2});
            } else {
                line.points.push_back({a.x1, mid_y});
                line.points.push_back({a.x2, mid_y});
            }

            line.points.push_back({a.x2, a.y2});
            lines.push_back(std::move(line));
        }
    }

    return lines;
}

} // namespace diagram_placement
