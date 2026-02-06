# Location/Scene System

This document explains how to implement a map switching system similar to Stardew Valley, where each location (Farm, Town, Beach, Mines) is a discrete, independent map.

## Table of Contents
- [What is a Location System?](#what-is-a-location-system)
- [Why Discrete Maps?](#why-discrete-maps)
- [Architecture Overview](#architecture-overview)
- [Warp Zones](#warp-zones)
- [Location Lifecycle](#location-lifecycle)
- [Ownership Model](#ownership-model)
- [Two-Phase Location Switching](#two-phase-location-switching)

---

## What is a Location System?

A **location system** manages multiple independent game maps. Instead of one massive world, you have separate areas that players transition between.

```
Stardew Valley Examples:
┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│    FARM     │───►│    TOWN     │───►│   BEACH     │
│  80×65 tiles│    │ 120×60 tiles│    │  50×40 tiles│
└─────────────┘    └─────────────┘    └─────────────┘
       │                  │
       ▼                  ▼
┌─────────────┐    ┌─────────────┐
│  FARMHOUSE  │    │    MINES    │
│  24×18 tiles│    │  40×40 tiles│
└─────────────┘    └─────────────┘

Each location is completely independent with its own:
- Tilemap (different dimensions)
- Enemies/NPCs
- Objects/Items
- Camera boundaries
```

### Web Development Analogy

Think of it like **page-based routing** vs a single-page app:

```
Page-based (Stardew Valley approach):
/farm   → loads farm.html (complete new page)
/town   → loads town.html (complete new page)

NOT like infinite scroll where everything exists at once.
```

Or like **iframes** where each page is self-contained:

```html
<!-- Only one "location" visible at a time -->
<iframe id="current-location" src="farm.html"></iframe>

<!-- Switching locations = changing the src -->
<script>
function changeLocation(newLocation) {
    document.getElementById('current-location').src = newLocation + '.html';
}
</script>
```

---

## Why Discrete Maps?

### 1. Memory Efficiency

```
One giant world: 500×500 tiles = 250,000 tiles always loaded
Discrete maps:   100×80 current map = 8,000 tiles loaded

31x less memory usage!
```

### 2. Independent Design

Each location can have:
- Different tile sizes
- Different tilesets
- Different rules (combat allowed? time passes?)
- Different camera constraints

### 3. Clean State Management

```cpp
// Easy to save/load per-location state
void Location::onExit() {
    saveEnemyPositions();
    saveObjectStates();
}

void Location::onEnter() {
    loadOrResetState();
    spawnEnemies();
}
```

### 4. Performance

Only update/render what's in the current location:

```cpp
// Bad: Update everything everywhere
for (auto& location : allLocations) {
    location->update(deltaTime);  // Updating 10 maps!
}

// Good: Only update current location
currentLocation->update(deltaTime);  // Just 1 map
```

---

## Architecture Overview

### Data Flow

```
Game (owns everything global)
├── Textures ─────────────────┐
├── Shaders                   │ Shared references
├── Player                    │ (raw pointers)
│                             │
├── locations (map)           │
│   ├── "farm" → Location ◄───┤
│   │   ├── Tilemap           │
│   │   ├── Enemies           │
│   │   └── WarpZones         │
│   │                         │
│   └── "town" → Location ◄───┘
│       ├── Tilemap
│       ├── Enemies
│       └── WarpZones
│
└── currentLocation* (non-owning pointer to active location)
```

### Key Classes

**Game** — Orchestrator, owns all resources
- Owns textures, shaders (shared across locations)
- Owns player (persists across locations)
- Owns all Location instances
- Holds pointer to current active location

**Location** — Self-contained map
- Owns its tilemap
- Owns its enemies
- Defines camera bounds
- Defines warp zones to other locations

**WarpZone** — Trigger area
- Rectangle in world coordinates
- Target location ID
- Spawn position in target location

---

## Warp Zones

A **warp zone** is a rectangular trigger area. When the player enters it, they transition to another location.

### Structure

```cpp
struct WarpZone {
    // Trigger area (world coordinates)
    float x, y;           // Top-left corner
    float width, height;  // Size

    // Destination
    std::string destinationLocationId;  // Key in locations map
    Vector2 spawnPosition;              // Where player appears

    bool contains(const Vector2& point) const {
        return point.x >= x && point.x <= x + width &&
               point.y >= y && point.y <= y + height;
    }
};
```

### Visual Example

```
FARM (480×320 pixels)
┌────────────────────────────────────────────┬────────┐
│                                            │ WARP   │
│                                            │ ZONE   │
│                                            │        │
│           Normal playable area             │x=448   │
│                                            │w=32    │
│                                            │h=320   │
│                                            │        │
│                                            │→ town  │
└────────────────────────────────────────────┴────────┘

When player.x >= 448, they're inside the warp zone.
Triggers: changeLocation("town", spawnPosition)
```

### Edge-Based Warps

For Stardew Valley-style "walk off the map" transitions:

```
Farm map edges:
         ┌─── North edge: leads to Mountain ───┐
         │                                      │
West ────┤            FARM                      ├──── East: leads to Bus Stop
edge     │                                      │
         │                                      │
         └─── South edge: leads to Forest ─────┘
```

Implementation:

```cpp
// Check if player walked off an edge
if (playerPos.x <= 0) {
    // Walked off west edge
    changeLocation("bus_stop", Vector2(780, playerPos.y));
}
if (playerPos.x >= mapWidth) {
    // Walked off east edge
    changeLocation("town", Vector2(20, playerPos.y));
}
```

---

## Location Lifecycle

### State Machine

```
                    ┌─────────────────────────────┐
                    │                             │
                    ▼                             │
┌──────────┐    ┌──────────┐    ┌──────────┐    │
│  IDLE    │───►│ onEnter()│───►│  ACTIVE  │────┘
│(in map)  │    │          │    │          │
└──────────┘    └──────────┘    └──────────┘
                                     │
                                     │ Player hits warp zone
                                     ▼
                               ┌──────────┐
                               │ onExit() │
                               └──────────┘
                                     │
                                     ▼
                            Change to new location
```

### onEnter()

Called when player enters a location:

```cpp
void Location::onEnter() {
    std::cout << "Entering: " << name << std::endl;

    // Clear any stale enemies
    clearEnemies();

    // Spawn fresh enemies from spawn data
    spawnEnemies();

    // Could also:
    // - Play location music
    // - Trigger cutscenes
    // - Start timers
    // - Load saved state
}
```

### onExit()

Called when player leaves a location:

```cpp
void Location::onExit() {
    std::cout << "Leaving: " << name << std::endl;

    // Could also:
    // - Save enemy positions
    // - Save object states
    // - Stop music
    // - Clear temporary effects
}
```

---

## Ownership Model

### The Problem

Who owns what? If both Game and Location own textures, we get double-free bugs.

### The Solution: Hierarchical Ownership

```
OWNS (unique_ptr)              REFERENCES (raw pointer)
─────────────────              ─────────────────────────
Game
├── Textures ─────────────────► Location receives Texture*
├── Shaders                     Location receives Shader*
├── Player ───────────────────► (passed to render methods)
└── Locations
    └── Location
        ├── Tilemap ──────────► Tilemap receives Texture*
        └── Enemies
            └── Enemy ────────► Enemy receives Texture*
```

**Rule**: The higher-level owner (Game) holds unique_ptr. Lower levels receive raw pointers.

### Why This Works

1. Game creates textures first, destroys them last
2. Locations are destroyed before textures
3. No circular dependencies
4. Clear lifetime hierarchy

```cpp
// Game destructor order (automatic via unique_ptr):
~Game() {
    // 1. locations destroyed (destroys tilemaps, enemies)
    // 2. player destroyed
    // 3. textures destroyed (safe - no one references them anymore)
    // 4. shaders destroyed
    // 5. window destroyed
}
```

---

## Two-Phase Location Switching

### The Problem

What happens if we change location mid-update?

```cpp
void Game::update(float deltaTime) {
    player->update(deltaTime);

    currentLocation->update(deltaTime);  // Updates enemies

    checkWarpCollisions();  // Player hit warp!
    changeLocation("town"); // DANGER: currentLocation just changed!

    // Now what? currentLocation->enemies is different!
    // Iterator invalidation, state corruption, chaos
}
```

### The Solution: Deferred Switching

**Phase 1: Request** (during update)
```cpp
void Game::checkWarpCollisions() {
    const WarpZone* warp = currentLocation->checkWarpCollision(player->getPosition());
    if (warp != nullptr) {
        // Don't switch NOW, just remember we need to
        requestLocationChange(warp->destinationLocationId, warp->spawnPosition);
    }
}

void Game::requestLocationChange(const std::string& locationId, const Vector2& spawnPos) {
    pendingLocationId = locationId;
    pendingSpawnPosition = spawnPos;
    locationChangeRequested = true;
}
```

**Phase 2: Execute** (after update/render)
```cpp
void Game::run() {
    while (running) {
        processInput();
        update(deltaTime);
        render();

        // Safe to switch now - frame is complete
        executeLocationChange();
    }
}

void Game::executeLocationChange() {
    if (locationChangeRequested) {
        changeLocation(pendingLocationId, pendingSpawnPosition);
        locationChangeRequested = false;
    }
}
```

### Timeline

```
Frame N:
├── processInput()
├── update()
│   ├── player.update()
│   ├── currentLocation.update()  ← Enemies update
│   └── checkWarpCollisions()     ← Sets locationChangeRequested = true
├── render()                       ← Renders FARM (old location)
└── executeLocationChange()        ← NOW switch to TOWN

Frame N+1:
├── processInput()
├── update()
│   ├── player.update()
│   └── currentLocation.update()  ← TOWN enemies update
├── render()                       ← Renders TOWN (new location)
└── executeLocationChange()        ← No pending change
```

---

## Summary

| Concept | Description |
|---------|-------------|
| Location | Self-contained map with tilemap, enemies, warps |
| WarpZone | Rectangular trigger that transitions to another location |
| onEnter/onExit | Lifecycle hooks for spawn/cleanup |
| Deferred switching | Request during update, execute after render |
| Ownership | Game owns resources, locations receive pointers |

### Key Formulas

```cpp
// Warp collision check
bool WarpZone::contains(Vector2 point) {
    return point.x >= x && point.x <= x + width &&
           point.y >= y && point.y <= y + height;
}

// World bounds from tilemap
worldWidth = tilemapWidth * tileSize;
worldHeight = tilemapHeight * tileSize;
```
