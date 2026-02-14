#include <diagram_placement/physics_layout.hpp>
#include <diagram_placement/class_diagram_layout_constants.hpp>
#include <algorithm>
#include <cmath>

namespace diagram_placement {

namespace {

using namespace layout;

constexpr float kLinearDamping = 5.0f;
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

    double next_x = padding;
    double next_y = padding;
    const double wrap_width = 2200.0;

    for (std::size_t i = 0; i < diagram_->classes.size(); ++i) {
        const auto& cls = diagram_->classes[i];
        bool expanded_state = false;
        if (const auto it = expanded_.find(cls.id); it != expanded_.end()) {
            expanded_state = it->second;
        }

        Rect size = fallback_size(expanded_state);
        if (const auto it = sizes_.find(cls.id); it != sizes_.end()) {
            size.width = it->second.width;
            size.height = it->second.height;
        }

        Rect initial = size;
        bool from_previous = false;
        if (previous_positions) {
            if (const auto it = previous_positions->find(cls.id); it != previous_positions->end()) {
                initial.x = it->second.x;
                initial.y = it->second.y;
                from_previous = true;
            }
        }
        if (!from_previous) {
            if (cls.x != 0.0 || cls.y != 0.0) {
                initial.x = cls.x;
                initial.y = cls.y;
            } else {
                initial.x = next_x;
                initial.y = next_y;
            }
        }

        if (!from_previous && cls.x == 0.0 && cls.y == 0.0) {
            next_x += size.width + block_margin;
            if (next_x > wrap_width) {
                next_x = padding;
                next_y += 180.0;
            }
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
