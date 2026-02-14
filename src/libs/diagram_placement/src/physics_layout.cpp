#include <diagram_placement/physics_layout.hpp>
#include <diagram_placement/class_diagram_layout_constants.hpp>
#include <algorithm>
#include <cmath>
#include <map>
#include <queue>

namespace diagram_placement {

namespace {

using namespace layout;

constexpr float kLinearDamping = 2.0f;
constexpr float kAngularDamping = 8.0f;
constexpr float kMinStep = 1.0f / 240.0f;
constexpr float kMaxStep = 1.0f / 30.0f;
constexpr int kSettleSteps = 600;

Rect fallback_size(bool expanded_state) {
    if (expanded_state) {
        return Rect{0.0, 0.0, expanded_min_width, collapsed_height + 160.0};
    }
    return Rect{0.0, 0.0, collapsed_width, collapsed_height};
}

} // namespace

PhysicsLayout::PhysicsLayout() = default;

PhysicsLayout::~PhysicsLayout() {
    destroy_world();
}

void PhysicsLayout::destroy_world() {
    if (b2World_IsValid(world_id_)) {
        b2DestroyWorld(world_id_);
    }
    world_id_ = b2_nullWorldId;
    blocks_.clear();
    active_anims_.clear();
    dragged_id_.clear();
    settle_steps_remaining_ = 0;
}

void PhysicsLayout::collect_current_positions(std::unordered_map<std::string, Rect>& out) const {
    out.clear();
    for (const auto& kv : blocks_) {
        const auto& state = kv.second;
        if (!b2Body_IsValid(state.body_id)) continue;
        const b2Vec2 p = b2Body_GetPosition(state.body_id);
        Rect r = state.rect;
        r.x = static_cast<double>(p.x) - r.width * 0.5;
        r.y = static_cast<double>(p.y) - r.height * 0.5;
        out.emplace(kv.first, r);
    }
}

void PhysicsLayout::build_world(const std::unordered_map<std::string, Rect>* previous_positions) {
    if (!diagram_) return;

    destroy_world();

    b2WorldDef world_def = b2DefaultWorldDef();
    world_def.gravity = b2Vec2{0.0f, 0.0f};
    world_def.maxContactPushSpeed = 200.0f;
    world_def.contactHertz = 120.0f;
    world_def.contactDampingRatio = 5.0f;
    world_id_ = b2CreateWorld(&world_def);

    // --- Compute inheritance hierarchy for initial layout ---
    // 1. Build depth map via BFS from root classes.
    std::unordered_map<std::string, int> depth_map;
    std::unordered_map<std::string, std::vector<std::string>> children_of;
    for (const auto& cls : diagram_->classes) {
        if (cls.parent_class_ids.empty()) {
            depth_map[cls.id] = 0;
        } else {
            // Primary parent ([0]) defines the tree structure for layout.
            children_of[cls.parent_class_ids[0]].push_back(cls.id);
        }
    }
    std::queue<std::string> bfs_queue;
    for (const auto& kv : depth_map) {
        bfs_queue.push(kv.first);
    }
    while (!bfs_queue.empty()) {
        const std::string cur = bfs_queue.front();
        bfs_queue.pop();
        const auto cit = children_of.find(cur);
        if (cit == children_of.end()) continue;
        for (const auto& child_id : cit->second) {
            if (depth_map.find(child_id) == depth_map.end()) {
                depth_map[child_id] = depth_map[cur] + 1;
                bfs_queue.push(child_id);
            }
        }
    }
    // Assign depth 0 to any class not reached (disconnected).
    for (const auto& cls : diagram_->classes) {
        if (depth_map.find(cls.id) == depth_map.end()) {
            depth_map[cls.id] = 0;
        }
    }

    // 2. Precompute sizes for all classes.
    struct BlockInfo {
        std::string id;
        double width;
        double height;
    };
    std::unordered_map<std::string, BlockInfo> block_info;
    for (const auto& cls : diagram_->classes) {
        bool expanded_state = false;
        if (const auto it = expanded_.find(cls.id); it != expanded_.end()) {
            expanded_state = it->second;
        }
        Rect sz = fallback_size(expanded_state);
        if (const auto it = sizes_.find(cls.id); it != sizes_.end()) {
            sz.width = it->second.width;
            sz.height = it->second.height;
        }
        block_info[cls.id] = BlockInfo{cls.id, sz.width, sz.height};
    }

    // 3. Group classes by depth level, preserving order within each level.
    std::map<int, std::vector<std::string>> levels;
    for (const auto& cls : diagram_->classes) {
        levels[depth_map[cls.id]].push_back(cls.id);
    }

    // 3b. Barycentric sorting: minimize edge crossings between levels.
    // Build composition ownership map for cross-level attraction.
    std::unordered_map<std::string, std::vector<std::string>> composition_targets;
    std::unordered_map<std::string, std::vector<std::string>> composition_owners;
    for (const auto& cls : diagram_->classes) {
        for (const auto& co : cls.child_objects) {
            composition_targets[cls.id].push_back(co.class_id);
            composition_owners[co.class_id].push_back(cls.id);
        }
    }

    // Assign initial X positions (center of block in its row).
    std::unordered_map<std::string, double> pos_x;
    {
        for (const auto& kv : levels) {
            double x = 0.0;
            for (const auto& id : kv.second) {
                pos_x[id] = x + block_info[id].width * 0.5;
                x += block_info[id].width + block_margin;
            }
        }
    }

    int max_depth = 0;
    for (const auto& kv : levels) {
        if (kv.first > max_depth) max_depth = kv.first;
    }

    // 3 iterations of down-sweep + up-sweep.
    for (int iter = 0; iter < 3; ++iter) {
        // Down-sweep: depth 1 -> max_depth.
        for (int d = 1; d <= max_depth; ++d) {
            auto& ids = levels[d];
            std::vector<std::pair<double, std::string>> bary_ids;
            for (const auto& id : ids) {
                double sum = 0.0;
                int count = 0;
                // Primary parent (inheritance).
                auto pit = children_of.end();
                for (const auto& cls : diagram_->classes) {
                    if (cls.id == id && !cls.parent_class_ids.empty()) {
                        auto px_it = pos_x.find(cls.parent_class_ids[0]);
                        if (px_it != pos_x.end()) {
                            sum += px_it->second;
                            ++count;
                        }
                        break;
                    }
                }
                // Composition owners (weaker influence).
                auto oit = composition_owners.find(id);
                if (oit != composition_owners.end()) {
                    for (const auto& owner : oit->second) {
                        auto ox = pos_x.find(owner);
                        if (ox != pos_x.end()) {
                            sum += ox->second * 0.3; // weak influence
                            count += 1;
                        }
                    }
                }
                double bary = (count > 0) ? sum / count : pos_x[id];
                bary_ids.push_back({bary, id});
            }
            std::sort(bary_ids.begin(), bary_ids.end());
            ids.clear();
            double x = 0.0;
            for (const auto& bi : bary_ids) {
                ids.push_back(bi.second);
                pos_x[bi.second] = x + block_info[bi.second].width * 0.5;
                x += block_info[bi.second].width + block_margin;
            }
        }

        // Up-sweep: max_depth-1 -> 0.
        for (int d = max_depth - 1; d >= 0; --d) {
            auto& ids = levels[d];
            std::vector<std::pair<double, std::string>> bary_ids;
            for (const auto& id : ids) {
                double sum = 0.0;
                int count = 0;
                // Children (inheritance).
                auto cit = children_of.find(id);
                if (cit != children_of.end()) {
                    for (const auto& ch : cit->second) {
                        auto cx = pos_x.find(ch);
                        if (cx != pos_x.end()) {
                            sum += cx->second;
                            ++count;
                        }
                    }
                }
                // Composition targets (weaker influence).
                auto tit = composition_targets.find(id);
                if (tit != composition_targets.end()) {
                    for (const auto& tgt : tit->second) {
                        auto tx = pos_x.find(tgt);
                        if (tx != pos_x.end()) {
                            sum += tx->second * 0.3;
                            count += 1;
                        }
                    }
                }
                double bary = (count > 0) ? sum / count : pos_x[id];
                bary_ids.push_back({bary, id});
            }
            std::sort(bary_ids.begin(), bary_ids.end());
            ids.clear();
            double x = 0.0;
            for (const auto& bi : bary_ids) {
                ids.push_back(bi.second);
                pos_x[bi.second] = x + block_info[bi.second].width * 0.5;
                x += block_info[bi.second].width + block_margin;
            }
        }
    }

    // 4. Compute hierarchy-based positions (centered rows).
    const double row_gap = 60.0;
    std::unordered_map<std::string, std::pair<double, double>> hierarchy_pos;
    double cur_y = padding;
    for (const auto& kv : levels) {
        const auto& ids = kv.second;
        double total_w = 0.0;
        double max_h = 0.0;
        for (const auto& id : ids) {
            const auto& bi = block_info[id];
            total_w += bi.width;
            if (bi.height > max_h) max_h = bi.height;
        }
        total_w += static_cast<double>(ids.size() > 1 ? ids.size() - 1 : 0) * block_margin;

        double x = padding;
        for (const auto& id : ids) {
            const auto& bi = block_info[id];
            hierarchy_pos[id] = {x, cur_y};
            x += bi.width + block_margin;
        }
        cur_y += max_h + row_gap;
    }

    // --- Create bodies ---
    for (std::size_t i = 0; i < diagram_->classes.size(); ++i) {
        const auto& cls = diagram_->classes[i];
        const auto& bi = block_info[cls.id];

        Rect initial;
        initial.width = bi.width;
        initial.height = bi.height;

        bool from_previous = false;
        if (previous_positions) {
            if (const auto it = previous_positions->find(cls.id); it != previous_positions->end()) {
                initial.x = it->second.x;
                initial.y = it->second.y;
                from_previous = true;
            }
        }
        if (!from_previous) {
            const auto& pos = hierarchy_pos[cls.id];
            initial.x = pos.first;
            initial.y = pos.second;
        }

        bool expanded_state = false;
        if (const auto it = expanded_.find(cls.id); it != expanded_.end()) {
            expanded_state = it->second;
        }

        b2BodyDef body_def = b2DefaultBodyDef();
        body_def.type = b2_dynamicBody;
        body_def.position = b2Vec2{
            static_cast<float>(initial.x + initial.width * 0.5),
            static_cast<float>(initial.y + initial.height * 0.5)
        };
        body_def.linearDamping = kLinearDamping;
        body_def.angularDamping = kAngularDamping;
        body_def.fixedRotation = true;
        b2BodyId body_id = b2CreateBody(world_id_, &body_def);

        b2ShapeDef shape_def = b2DefaultShapeDef();
        shape_def.density = 1.0f;
        shape_def.material.friction = 0.3f;

        const float hx = static_cast<float>(initial.width * 0.5 + cls.margin + gap * 0.5);
        const float hy = static_cast<float>(initial.height * 0.5 + cls.margin + gap * 0.5);
        b2Polygon poly = b2MakeBox(hx, hy);
        b2ShapeId shape_id = b2CreatePolygonShape(body_id, &shape_def, &poly);

        BodyState state;
        state.body_id = body_id;
        state.shape_id = shape_id;
        state.rect = initial;
        state.margin = cls.margin;
        state.expanded = expanded_state;
        blocks_[cls.id] = state;
        order_[cls.id] = i;
        margins_[cls.id] = cls.margin;
    }

    warmup_settle(60);
    request_settle();
}

void PhysicsLayout::build(const diagram_model::ClassDiagram& diagram,
    const std::unordered_map<std::string, bool>& expanded,
    const std::unordered_map<std::string, Rect>* block_sizes)
{
    std::unordered_map<std::string, Rect> previous_positions;
    if (diagram_ == &diagram && b2World_IsValid(world_id_)) {
        collect_current_positions(previous_positions);
    }

    diagram_ = &diagram;
    expanded_ = expanded;
    sizes_.clear();
    if (block_sizes) {
        for (const auto& kv : *block_sizes) {
            sizes_[kv.first] = kv.second;
        }
    }

    build_world(previous_positions.empty() ? nullptr : &previous_positions);
}

void PhysicsLayout::update_block_size(const std::string& id, double w, double h, bool expanded) {
    if (!diagram_) return;
    if (!b2World_IsValid(world_id_)) return;

    expanded_[id] = expanded;
    sizes_[id] = Rect{0.0, 0.0, w, h};

    auto it = blocks_.find(id);
    if (it == blocks_.end()) return;
    auto& state = it->second;
    if (!b2Body_IsValid(state.body_id)) return;

    // Cancel any existing animation for this block.
    active_anims_.erase(
        std::remove_if(active_anims_.begin(), active_anims_.end(),
            [&](const ResizeAnim& a) { return a.block_id == id; }),
        active_anims_.end());

    // Compute current top-left as anchor.
    const b2Vec2 p = b2Body_GetPosition(state.body_id);
    const double anchor_x = static_cast<double>(p.x) - state.rect.width * 0.5;
    const double anchor_y = static_cast<double>(p.y) - state.rect.height * 0.5;

    ResizeAnim anim;
    anim.block_id = id;
    anim.from_w = state.rect.width;
    anim.from_h = state.rect.height;
    anim.to_w = w;
    anim.to_h = h;
    anim.anchor_x = anchor_x;
    anim.anchor_y = anchor_y;
    anim.progress = 0.0f;
    active_anims_.push_back(anim);

    // Pin the block so it doesn't move while growing.
    b2Body_SetType(state.body_id, b2_kinematicBody);
    b2Body_SetLinearVelocity(state.body_id, b2Vec2{0.0f, 0.0f});

    state.expanded = expanded;
    request_settle();
}

void PhysicsLayout::step(float dt) {
    if (!b2World_IsValid(world_id_)) return;
    if (active_anims_.empty() && dragged_id_.empty() && settle_steps_remaining_ <= 0) {
        return;
    }

    const float clamped_dt = std::clamp(dt, kMinStep, kMaxStep);

    // Advance resize animations and update shapes.
    for (auto& anim : active_anims_) {
        anim.progress += kAnimSpeed * clamped_dt;
        if (anim.progress > 1.0f) anim.progress = 1.0f;

        const float t = anim.progress;
        const double cur_w = anim.from_w + (anim.to_w - anim.from_w) * static_cast<double>(t);
        const double cur_h = anim.from_h + (anim.to_h - anim.from_h) * static_cast<double>(t);

        auto bit = blocks_.find(anim.block_id);
        if (bit == blocks_.end()) continue;
        auto& state = bit->second;
        if (!b2Body_IsValid(state.body_id)) continue;

        // Destroy old shape, create new one with interpolated size.
        if (b2Shape_IsValid(state.shape_id)) {
            b2DestroyShape(state.shape_id, true);
        }

        b2ShapeDef shape_def = b2DefaultShapeDef();
        shape_def.density = 1.0f;
        shape_def.material.friction = 0.3f;

        const float hx = static_cast<float>(cur_w * 0.5 + state.margin + gap * 0.5);
        const float hy = static_cast<float>(cur_h * 0.5 + state.margin + gap * 0.5);
        b2Polygon poly = b2MakeBox(hx, hy);
        state.shape_id = b2CreatePolygonShape(state.body_id, &shape_def, &poly);

        // Keep top-left anchored: set center from anchor + half-size.
        const b2Vec2 new_center{
            static_cast<float>(anim.anchor_x + cur_w * 0.5),
            static_cast<float>(anim.anchor_y + cur_h * 0.5)
        };
        b2Body_SetTransform(state.body_id, new_center, b2MakeRot(0.0f));
        b2Body_SetLinearVelocity(state.body_id, b2Vec2{0.0f, 0.0f});

        // Update visual rect for get_placed().
        state.rect.width = cur_w;
        state.rect.height = cur_h;
    }

    // Finalize completed animations: unpin blocks.
    for (auto it = active_anims_.begin(); it != active_anims_.end(); ) {
        if (it->progress >= 1.0f) {
            auto bit = blocks_.find(it->block_id);
            if (bit != blocks_.end() && b2Body_IsValid(bit->second.body_id)) {
                b2Body_SetType(bit->second.body_id, b2_dynamicBody);
                b2Body_SetAwake(bit->second.body_id, true);
            }
            it = active_anims_.erase(it);
        } else {
            ++it;
        }
    }

    b2World_Step(world_id_, clamped_dt, 4);

    if (!dragged_id_.empty() || !active_anims_.empty()) {
        return;
    }

    if (settle_steps_remaining_ > 0) {
        --settle_steps_remaining_;
    }
    if (settle_steps_remaining_ > 0 && is_settled()) {
        settle_steps_remaining_ = 0;
    }
}

PlacedClassDiagram PhysicsLayout::get_placed() const {
    PlacedClassDiagram placed;
    if (!diagram_) return placed;

    for (const auto& cls : diagram_->classes) {
        auto it = blocks_.find(cls.id);
        if (it == blocks_.end()) continue;
        const auto& state = it->second;
        if (!b2Body_IsValid(state.body_id)) continue;
        const b2Vec2 p = b2Body_GetPosition(state.body_id);

        PlacedClassBlock block;
        block.class_id = cls.id;
        block.rect.width = state.rect.width;
        block.rect.height = state.rect.height;
        block.rect.x = static_cast<double>(p.x) - block.rect.width * 0.5;
        block.rect.y = static_cast<double>(p.y) - block.rect.height * 0.5;
        block.margin = state.margin;
        block.expanded = state.expanded;
        placed.blocks.push_back(block);
    }
    return placed;
}

void PhysicsLayout::begin_drag(const std::string& id) {
    auto it = blocks_.find(id);
    if (it == blocks_.end()) return;
    if (!b2Body_IsValid(it->second.body_id)) return;
    dragged_id_ = id;
    b2Body_SetType(it->second.body_id, b2_kinematicBody);
    b2Body_SetAwake(it->second.body_id, true);
    request_settle();
}

void PhysicsLayout::drag_to(const std::string& id, double wx, double wy) {
    auto it = blocks_.find(id);
    if (it == blocks_.end()) return;
    if (!b2Body_IsValid(it->second.body_id)) return;
    if (dragged_id_ != id) return;
    const b2Vec2 p{
        static_cast<float>(wx + it->second.rect.width * 0.5),
        static_cast<float>(wy + it->second.rect.height * 0.5)
    };
    b2Body_SetTransform(it->second.body_id, p, b2MakeRot(0.0f));
    b2Body_SetLinearVelocity(it->second.body_id, b2Vec2{0.0f, 0.0f});
    b2Body_SetAngularVelocity(it->second.body_id, 0.0f);
}

void PhysicsLayout::end_drag(const std::string& id) {
    auto it = blocks_.find(id);
    if (it == blocks_.end()) return;
    if (!b2Body_IsValid(it->second.body_id)) return;
    b2Body_SetType(it->second.body_id, b2_dynamicBody);
    b2Body_SetAwake(it->second.body_id, true);
    if (dragged_id_ == id) {
        dragged_id_.clear();
    }
    request_settle();
}

bool PhysicsLayout::is_settled() const {
    if (!b2World_IsValid(world_id_)) return true;
    if (!active_anims_.empty()) return false;
    constexpr float max_linear_speed = 0.1f;
    for (const auto& kv : blocks_) {
        const b2BodyId body_id = kv.second.body_id;
        if (!b2Body_IsValid(body_id)) continue;
        const b2Vec2 v = b2Body_GetLinearVelocity(body_id);
        const float speed2 = v.x * v.x + v.y * v.y;
        if (speed2 > max_linear_speed * max_linear_speed) {
            return false;
        }
    }
    return true;
}

void PhysicsLayout::request_settle() {
    settle_steps_remaining_ = kSettleSteps;
    for (const auto& kv : blocks_) {
        const b2BodyId body_id = kv.second.body_id;
        if (!b2Body_IsValid(body_id)) continue;
        b2Body_SetAwake(body_id, true);
    }
}

void PhysicsLayout::warmup_settle(int steps) {
    if (!b2World_IsValid(world_id_)) return;
    for (int i = 0; i < steps; ++i) {
        b2World_Step(world_id_, 1.0f / 90.0f, 8);
    }
}

} // namespace diagram_placement
