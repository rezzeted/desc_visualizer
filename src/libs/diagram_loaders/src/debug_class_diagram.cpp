#include <diagram_loaders/debug_class_diagram.hpp>

namespace diagram_loaders {

diagram_model::ClassDiagram generate_debug_class_diagram() {
    diagram_model::ClassDiagram out;
    out.name = "RPG roguelike classes (debug)";
    out.canvas_width = 2600;
    out.canvas_height = 1600;

    auto prop = [](const char* name, const char* type, const char* def) {
        return diagram_model::Property{ name, type, def };
    };
    auto comp = [&](const char* name, const char* type, std::initializer_list<diagram_model::Property> properties = {}) {
        diagram_model::Component c;
        c.name = name;
        c.type = type;
        c.properties.assign(properties.begin(), properties.end());
        return c;
    };
    auto child = [](const char* class_id, const char* label) {
        return diagram_model::ChildObject{ class_id, label };
    };
    auto add_class =
        [&](const char* id,
            const char* type_name,
            std::initializer_list<const char*> parents,
            double x,
            double y,
            std::initializer_list<diagram_model::Property> properties,
            std::initializer_list<diagram_model::Component> components,
            std::initializer_list<diagram_model::ChildObject> children = {})
    {
        diagram_model::DiagramClass cl;
        cl.id = id;
        cl.type_name = type_name;
        for (auto p : parents)
            if (p) cl.parent_class_ids.push_back(p);
        cl.x = x;
        cl.y = y;
        cl.properties.assign(properties.begin(), properties.end());
        cl.components.assign(components.begin(), components.end());
        cl.child_objects.assign(children.begin(), children.end());
        out.classes.push_back(std::move(cl));
    };

    add_class("GameObject", "GameObject", {}, 60, 40,
        { prop("id", "int", "0"), prop("name", "string", ""), prop("enabled", "bool", "true") },
        { comp("transform", "Transform", { prop("x", "float", "0.0"), prop("y", "float", "0.0"), prop("rotation", "float", "0.0") }),
          comp("tag", "TagComponent", { prop("tag", "string", "") }) });

    add_class("Entity", "Entity", {"GameObject"}, 60, 180,
        { prop("active", "bool", "true"), prop("layer", "int", "0"), prop("zIndex", "int", "0") },
        { comp("sprite", "SpriteRenderer", { prop("spriteSheet", "string", ""), prop("frameWidth", "int", "32"), prop("frameHeight", "int", "32") }),
          comp("collision", "CollisionBox", { prop("width", "float", "1.0"), prop("height", "float", "1.0") }) });

    add_class("Character", "Character", {"Entity"}, 60, 320,
        { prop("health", "int", "100"), prop("maxHealth", "int", "100"), prop("speed", "float", "1.0"), prop("level", "int", "1") },
        { comp("stats", "StatsComponent", { prop("strength", "int", "10"), prop("dexterity", "int", "10"), prop("intelligence", "int", "10") }),
          comp("animation", "AnimationController", { prop("defaultAnim", "string", "idle") }) },
        { child("HealthBar", "statusBar") });

    // Mixin/interface classes for demonstrating multiple inheritance.
    add_class("Serializable", "Serializable", {}, 400, 40,
        { prop("serializeVersion", "int", "1") },
        { comp("serializer", "BinarySerializer", { prop("endian", "string", "little") }) });

    add_class("Saveable", "Saveable", {}, 600, 40,
        { prop("saveSlot", "int", "0") },
        { comp("persistence", "SaveManager", { prop("autoSave", "bool", "true") }) });

    add_class("Player", "Player", {"Character", "Serializable", "Saveable"}, 60, 460,
        { prop("experience", "int", "0"), prop("playerName", "string", "Hero"), prop("classType", "string", "warrior") },
        { comp("input", "InputController", { prop("moveSpeed", "float", "5.0") }),
          comp("camera", "CameraFollow", { prop("smoothing", "float", "0.1"), prop("offset", "float", "0.0") }),
          comp("xp", "ExperienceTracker", { prop("levelUpThreshold", "int", "100") }) },
        { child("Container", "inventory"), child("HealthBar", "playerHud") });

    add_class("Enemy", "Enemy", {"Character"}, 300, 460,
        { prop("aggroRadius", "float", "5.0"), prop("expReward", "int", "10"), prop("lootChance", "float", "0.5") },
        { comp("ai", "AIController", { prop("behaviorTree", "string", "default_ai") }),
          comp("aggro", "AggroSensor", { prop("checkInterval", "float", "0.5") }) },
        { child("Container", "lootContainer") });

    add_class("MeleeEnemy", "MeleeEnemy", {"Enemy"}, 300, 600,
        { prop("attackDamage", "int", "15"), prop("attackRange", "float", "1.5"), prop("attackSpeed", "float", "1.0") },
        { comp("melee", "MeleeAttack", { prop("swingArc", "float", "90") }),
          comp("pathfinder", "PathFinder", { prop("algorithm", "string", "astar") }) });

    add_class("RangedEnemy", "RangedEnemy", {"Enemy"}, 520, 600,
        { prop("projectileType", "string", "arrow"), prop("fireRate", "float", "1.0"), prop("range", "float", "8.0") },
        { comp("ranged", "RangedAttack", { prop("accuracy", "float", "0.8") }),
          comp("spawner", "ProjectileSpawner", { prop("poolSize", "int", "10") }) });

    add_class("NPC", "NPC", {"Character"}, 520, 460,
        { prop("dialogue", "string", ""), prop("shopEnabled", "bool", "false"), prop("questId", "string", "") },
        { comp("dialogueSystem", "DialogueSystem", { prop("bubbleOffset", "float", "1.5") }),
          comp("questGiver", "QuestGiver", { prop("questPool", "string", "main") }) });

    add_class("Item", "Item", {"Entity"}, 740, 320,
        { prop("stackable", "bool", "false"), prop("maxStack", "int", "1"), prop("rarity", "string", "common"), prop("value", "int", "0"), prop("weight", "float", "0.1") },
        { comp("pickup", "PickupTrigger", { prop("radius", "float", "0.5") }),
          comp("icon", "ItemIcon", { prop("iconId", "string", "default_item") }) });

    add_class("Weapon", "Weapon", {"Item"}, 740, 460,
        { prop("damage", "int", "5"), prop("attackSpeed", "float", "1.0"), prop("durability", "int", "100"), prop("damageType", "string", "physical") },
        { comp("damageDealer", "DamageDealer", { prop("critChance", "float", "0.05") }),
          comp("durabilityTracker", "DurabilityTracker", { prop("degradeRate", "float", "1.0") }) });

    add_class("Sword", "Sword", {"Weapon"}, 740, 600,
        { prop("slashDamage", "int", "8"), prop("parryChance", "float", "0.15") },
        { comp("swing", "MeleeSwing", { prop("range", "float", "1.2") }),
          comp("block", "BlockHandler", { prop("blockAngle", "float", "60") }) });

    add_class("Bow", "Bow", {"Weapon"}, 960, 600,
        { prop("drawSpeed", "float", "0.8"), prop("projectileSpeed", "float", "15.0"), prop("ammoType", "string", "arrow") },
        { comp("launcher", "ProjectileLauncher", { prop("launchOffset", "float", "0.5") }),
          comp("charge", "ChargeSystem", { prop("maxCharge", "float", "2.0") }) });

    add_class("Armor", "Armor", {"Item"}, 960, 460,
        { prop("defense", "int", "5"), prop("slot", "string", "chest"), prop("resistFire", "int", "0"), prop("resistIce", "int", "0") },
        { comp("defenseModifier", "DefenseModifier", { prop("flatReduction", "int", "0") }),
          comp("equipRenderer", "EquipRenderer", { prop("meshOverride", "string", "") }) });

    add_class("Consumable", "Consumable", {"Item"}, 1180, 460,
        { prop("effectDuration", "float", "5.0"), prop("cooldown", "float", "1.0"), prop("charges", "int", "1") },
        { comp("useEffect", "UseEffect", { prop("effectType", "string", "instant") }),
          comp("cooldownTimer", "CooldownTimer", { prop("globalCooldown", "bool", "false") }) });

    add_class("Projectile", "Projectile", {"Entity"}, 1180, 320,
        { prop("speed", "float", "10.0"), prop("lifetime", "float", "3.0"), prop("damage", "int", "5"), prop("piercing", "bool", "false") },
        { comp("velocity", "Velocity", { prop("drag", "float", "0") }),
          comp("lifetimeTimer", "LifetimeTimer", { prop("fadeOut", "bool", "true") }),
          comp("damageOnHit", "DamageOnHit", { prop("knockback", "float", "0.5") }),
          comp("trail", "TrailRenderer", { prop("trailLength", "int", "5") }) });

    add_class("Container", "Container", {"Entity"}, 1400, 320,
        { prop("maxSlots", "int", "20"), prop("maxWeight", "float", "50.0"), prop("sortable", "bool", "true") },
        { comp("inventoryGrid", "InventoryGrid", { prop("columns", "int", "5") }),
          comp("weightCalc", "WeightCalculator", { prop("encumbranceThreshold", "float", "40.0") }) },
        { child("InventorySlot", "slot") });

    add_class("Tile", "Tile", {"GameObject"}, 60, 760,
        { prop("walkable", "bool", "true"), prop("tileset", "string", "dungeon"), prop("tileIndex", "int", "0") },
        { comp("tileRenderer", "TileRenderer", { prop("tileSize", "int", "16") }),
          comp("passability", "Passability", { prop("moveCost", "float", "1.0") }) });

    add_class("FloorTile", "FloorTile", {"Tile"}, 60, 900,
        { prop("hasTrap", "bool", "false"), prop("trapDamage", "int", "0"), prop("decorationType", "string", "none") },
        { comp("decoration", "FloorDecoration", { prop("variant", "int", "0") }),
          comp("trap", "TrapTrigger", { prop("triggerChance", "float", "1.0") }) });

    add_class("WallTile", "WallTile", {"Tile"}, 300, 900,
        { prop("destructible", "bool", "false"), prop("hitPoints", "int", "50"), prop("blocksSight", "bool", "true") },
        { comp("wallRenderer", "WallRenderer", { prop("wallType", "string", "stone") }),
          comp("destructibleCmp", "Destructible", { prop("debrisType", "string", "rubble") }) });

    add_class("DoorTile", "DoorTile", {"Tile"}, 520, 900,
        { prop("locked", "bool", "false"), prop("keyId", "string", ""), prop("autoClose", "bool", "true") },
        { comp("doorController", "DoorController", { prop("openSpeed", "float", "2.0") }),
          comp("lockMechanism", "LockMechanism", { prop("lockLevel", "int", "1") }) });

    add_class("UIElement", "UIElement", {"GameObject"}, 900, 760,
        { prop("visible", "bool", "true"), prop("zOrder", "int", "0"), prop("anchor", "string", "topLeft"), prop("opacity", "float", "1.0") },
        { comp("uiRenderer", "UIRenderer", { prop("blendMode", "string", "normal") }),
          comp("uiLayout", "UILayout", { prop("margin", "float", "4.0") }) });

    add_class("HealthBar", "HealthBar", {"UIElement"}, 900, 900,
        { prop("targetEntity", "string", ""), prop("barColor", "string", "red"), prop("showText", "bool", "true") },
        { comp("barRenderer", "BarRenderer", { prop("barWidth", "float", "100"), prop("barHeight", "float", "10") }),
          comp("valueBinding", "ValueBinding", { prop("sourceProperty", "string", "health") }) });

    add_class("MiniMap", "MiniMap", {"UIElement"}, 1120, 900,
        { prop("mapScale", "float", "0.1"), prop("showEnemies", "bool", "true"), prop("radius", "int", "64") },
        { comp("mapRenderer", "MapRenderer", { prop("pixelsPerTile", "int", "4") }),
          comp("fog", "FogOfWar", { prop("revealRadius", "int", "5") }),
          comp("playerMarker", "PlayerMarker", { prop("markerSize", "float", "3.0") }) });

    add_class("InventorySlot", "InventorySlot", {"UIElement"}, 1340, 900,
        { prop("slotIndex", "int", "0"), prop("acceptType", "string", "any") },
        { comp("dragDrop", "DragDrop", { prop("snapBack", "bool", "true") }),
          comp("slotRenderer", "SlotRenderer", { prop("slotSize", "float", "32") }),
          comp("tooltip", "TooltipProvider", { prop("delay", "float", "0.5") }) });

    add_class("SpawnPoint", "SpawnPoint", {"GameObject"}, 1600, 760,
        { prop("spawnClass", "string", ""), prop("spawnRate", "float", "5.0"), prop("maxSpawns", "int", "3"), prop("radius", "float", "2.0") },
        { comp("spawnTimer", "SpawnTimer", { prop("jitter", "float", "1.0") }),
          comp("spawnArea", "SpawnArea", { prop("shape", "string", "circle") }) });

    add_class("Level", "Level", {"GameObject"}, 1820, 760,
        { prop("width", "int", "64"), prop("height", "int", "64"), prop("difficulty", "int", "1"), prop("seed", "int", "0") },
        { comp("levelManager", "LevelManager", { prop("maxRooms", "int", "12") }),
          comp("turnManager", "TurnManager", { prop("turnDelay", "float", "0.3") }),
          comp("fogSystem", "FogOfWarSystem", { prop("defaultVisible", "bool", "false") }) },
        { child("DoorTile", "entrance"), child("DoorTile", "exit"), child("SpawnPoint", "spawner") });

    return out;
}

} // namespace diagram_loaders
