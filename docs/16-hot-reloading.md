# Hot-Reloading

This document explains hot-reloading — the technique for updating game assets (shaders, tilemaps, textures) while the game is running, without restarting.

## Table of Contents
- [What is Hot-Reloading?](#what-is-hot-reloading)
- [The Core Problem](#the-core-problem)
- [Why Hot-Reload?](#why-hot-reload)
- [File Watching](#file-watching)
- [Why Atomic Swap Matters](#why-atomic-swap-matters)
- [Shaders vs C++ Code](#shaders-vs-c-code)
- [When to Check for Changes](#when-to-check-for-changes)
- [Shader Hot-Reload](#shader-hot-reload)
- [Tilemap Hot-Reload](#tilemap-hot-reload)
- [Graceful Failure](#graceful-failure)
- [Abstracting Hot-Reload: FileWatcher](#abstracting-hot-reload-filewatcher)
- [Why This Matters for HD-2D](#why-this-matters-for-hd-2d)

---

## What is Hot-Reloading?

**Hot-reloading** automatically detects when asset files change and reloads them into the running game, providing instant feedback without restarting.

```
Traditional Workflow (slow):
Edit file → Save → Stop game → Recompile → Run game → Navigate to test area → See result
Time: 30+ seconds per iteration

Hot-Reload Workflow (fast):
Edit file → Save → See result instantly
Time: <1 second per iteration
```

### Visual Example

```
┌─────────────────────┐         ┌─────────────────────┐
│   Code Editor       │         │    Game Window      │
│                     │         │                     │
│  // tile.frag       │  Save   │   ┌───┬───┬───┐    │
│  FragColor = vec4(  │ ───────►│   │ R │ R │ R │    │
│    1.0, 0.0, 0.0,   │         │   ├───┼───┼───┤    │
│    1.0);            │         │   │ R │ R │ R │    │
│                     │         │   └───┴───┴───┘    │
│                     │         │   Tiles turn red!  │
└─────────────────────┘         └─────────────────────┘
        ↑                                ↑
    You edit                    Instant visual feedback
```

### Web Development Analogy

Hot-reloading is exactly like **webpack-dev-server** or **Vite** in web development:

```javascript
// React with hot module replacement
// Edit component → Save → Browser updates instantly

// Same concept:
// Edit shader → Save → Game updates instantly
```

Or like browser **DevTools**:

```css
/* Edit CSS in DevTools inspector */
.tile { background: red; }  /* Changes appear instantly */
```

---

## The Core Problem

When you're developing a game, the traditional cycle is painful:

```
Traditional development cycle:
1. Edit code/shader/asset
2. Save
3. Stop the running game
4. Recompile (if C++)
5. Restart game
6. Wait for loading
7. Navigate to the spot you were testing
8. Finally see if your change worked
9. Repeat 100+ times per feature
```

For a single shader tweak, this might take 20-30 seconds. If you're experimenting with visual effects and need to try 50 different values, that's **25 minutes** of waiting.

Hot-reloading breaks this cycle by **keeping the game running** and swapping assets in-place. You stay exactly where you are in the game — same player position, same enemies, same camera view — and just see the visual change instantly.

---

## Why Hot-Reload?

### 1. Faster Iteration

```
Without hot-reload:
Edit → Compile (5s) → Run (2s) → Load (3s) → Navigate (10s) = 20 seconds

With hot-reload:
Edit → Save = instant
```

For shader development, you might tweak values hundreds of times. 20 seconds × 100 iterations = 33 minutes wasted. Hot-reload saves this.

### 2. Maintain Game State

Without hot-reload, restarting loses your current state:
- Player position
- Enemy spawns
- Camera view
- Progression

With hot-reload, you stay exactly where you are and just see the visual change.

### 3. Creative Flow

Stopping and restarting breaks concentration. Hot-reload keeps you in the creative zone, making small adjustments and immediately seeing results.

---

## File Watching

The core of hot-reloading is detecting when files change.

### How File Watching Works

```
Game loop:
┌─────────────────────────────────────────┐
│                                         │
│  1. Check file modification time        │
│     └─ Has tile.frag changed?           │
│                                         │
│  2. If changed:                         │
│     └─ Reload the asset                 │
│     └─ Update modification timestamp    │
│                                         │
│  3. Continue normal game loop           │
│                                         │
└─────────────────────────────────────────┘
```

### Using std::filesystem (C++17)

```cpp
#include <filesystem>
namespace fs = std::filesystem;

// Get when a file was last modified
fs::file_time_type lastModified = fs::last_write_time("shaders/tile.frag");

// Later, check if it changed
fs::file_time_type currentTime = fs::last_write_time("shaders/tile.frag");
if (currentTime != lastModified) {
    // File changed! Reload it.
    reload();
    lastModified = currentTime;
}
```

### Polling vs Event-Based

**Polling (simpler, what we use)**:
- Check modification time every frame
- Very cheap operation (~0.01ms)
- Cross-platform, no dependencies

**Event-based (more complex)**:
- OS notifies when file changes
- inotify (Linux), FSEvents (macOS), ReadDirectoryChangesW (Windows)
- Requires platform-specific code or library

For game development, polling every frame is perfectly fine.

---

## Why Atomic Swap Matters

When reloading, you could have a **broken** new version (syntax error, missing file, corrupted data). The game should **never crash** — it should keep using the old working version.

### Bad Approach (Crashes on Error)

```
1. Delete old shader
2. Try to load new shader
3. New shader has error
4. CRASH! No shader to use!
```

### Good Approach (Atomic Swap)

```
1. Try to load new shader into SEPARATE variable
2. If load fails:
   └─ Log error
   └─ Keep using old shader
   └─ Game continues running!
3. If load succeeds:
   └─ Delete old shader
   └─ Use new one
```

The "swap" only happens **after** the new asset is fully validated. This is why we:
- Compile new shader first
- Link new program first
- Validate everything works
- Only then delete the old program

The key insight: **never destroy what you have until the replacement is ready**.

---

## Shaders vs C++ Code

### Why Shaders Are Perfect for Hot-Reload

| Property | Shaders | C++ Code |
|----------|---------|----------|
| Format | Text files | Compiled binary |
| Compilation | Runtime (OpenGL does it) | Build time (compiler does it) |
| Swapping | Just change program ID | Executable is locked |
| State | Stateless functions | Functions at fixed memory addresses |

Shaders are **interpreted at runtime** by the GPU driver. You can compile a completely new shader while the game runs and swap to it instantly.

### Why C++ Hot-Reload is Hard

C++ compiles to machine code at fixed memory addresses. While your game is running:
- The executable file is locked by the OS
- Functions exist at specific memory locations
- Changing code requires recompilation
- The new code can't just "slot in"

This is why game engines often separate concerns:

```
C++ (no hot-reload):
└─ Core engine systems
└─ Performance-critical code
└─ Memory management

Hot-reloadable assets:
└─ Shaders (visuals)
└─ Data files (levels, configs)
└─ Scripts (if using Lua/Python)
└─ Textures, audio
```

---

## When to Check for Changes

You have several options for when to poll for file changes:

### Option A: Every Frame

```cpp
void Game::update(float deltaTime) {
    shader->checkAndReload();  // Called 60 times/second
}
```

| Pros | Cons |
|------|------|
| Instant feedback | 60 filesystem calls/sec per asset |
| Simplest code | (Still very cheap though) |

### Option B: On a Timer

```cpp
reloadTimer += deltaTime;
if (reloadTimer > 0.5f) {
    reloadTimer = 0;
    shader->checkAndReload();  // Check twice per second
}
```

| Pros | Cons |
|------|------|
| Fewer filesystem calls | Up to 0.5s delay |

### Option C: On Keypress

```cpp
if (input->isKeyPressed(KEY_F5)) {
    shader->reload();  // Manual reload
}
```

| Pros | Cons |
|------|------|
| Full control | Breaks "instant" feeling |
| Zero overhead | Must remember to press key |

**Recommendation**: Use Option A (every frame). The filesystem call to check modification time is so cheap (~0.01ms) that it's negligible. The instant feedback is worth it.

---

## Shader Hot-Reload

Shaders are the best candidate for hot-reload because:
1. They're text files (easy to reload)
2. They compile at runtime (no C++ recompilation needed)
3. Visual tweaks benefit most from instant feedback

### The Pattern

```cpp
class Shader {
    std::string vertexPath;    // Remember where files are
    std::string fragmentPath;

    fs::file_time_type vertexLastModified;    // When we last loaded them
    fs::file_time_type fragmentLastModified;

    GLuint programID;  // The actual shader program
};
```

### Reload Process

```
1. Read new source from file
        │
        ▼
2. Compile new vertex shader
        │
   ┌────┴────┐
   │ Success │ Failure → Keep old shader, log error
   └────┬────┘
        │
        ▼
3. Compile new fragment shader
        │
   ┌────┴────┐
   │ Success │ Failure → Delete vertex, keep old, log error
   └────┬────┘
        │
        ▼
4. Link new program
        │
   ┌────┴────┐
   │ Success │ Failure → Delete shaders, keep old, log error
   └────┬────┘
        │
        ▼
5. Delete old program, use new one
        │
        ▼
6. Update modification timestamps
```

### Key Insight: Atomic Swap

The reload creates a **completely new** shader program, then swaps it with the old one only if everything succeeds. This ensures the game never uses a broken shader.

```cpp
bool Shader::reload() {
    // Create new shader
    GLuint newProgram = createAndCompileNewShader();

    if (newProgram == 0) {
        // Compilation failed - keep using old shader
        return false;
    }

    // Success! Atomic swap
    glDeleteProgram(programID);  // Delete old
    programID = newProgram;      // Use new
    return true;
}
```

---

## Tilemap Hot-Reload

Tilemaps can also be hot-reloaded by loading them from data files instead of hardcoding.

### Data Format: CSV

Simple comma-separated values:

```
0,0,0,1,1,0,0,0,0,0
0,0,1,1,1,1,0,0,2,2
0,1,1,0,0,1,1,2,2,2
1,1,0,0,0,0,1,2,2,2
```

Each number is a tile ID. Each line is a row.

### Why CSV?

| Format | Pros | Cons |
|--------|------|------|
| **CSV** | Simple, human-editable, no dependencies | No metadata (tile size, etc.) |
| **JSON** | Metadata support, structured | Needs JSON library |
| **TMX (Tiled)** | Industry standard, visual editor | XML parsing, more complex |

CSV is perfect for learning and quick iteration.

### Reload Process

```cpp
bool Tilemap::loadFromCSV(const std::string& path) {
    std::ifstream file(path);
    if (!file) return false;

    std::vector<std::vector<int>> rows;
    std::string line;

    while (std::getline(file, line)) {
        std::vector<int> row;
        std::stringstream ss(line);
        std::string cell;

        while (std::getline(ss, cell, ',')) {
            row.push_back(std::stoi(cell));
        }
        rows.push_back(row);
    }

    // Update dimensions
    height = rows.size();
    width = rows[0].size();

    // Flatten to 1D array
    tiles.clear();
    for (const auto& row : rows) {
        for (int tile : row) {
            tiles.push_back(tile);
        }
    }

    return true;
}
```

### Dimension Changes

Unlike shaders, tilemaps can **change size** on reload. This requires:

1. Recalculating world bounds
2. Potentially repositioning the camera
3. Checking if player is now out-of-bounds

```cpp
bool Tilemap::reload() {
    int oldWidth = width;
    int oldHeight = height;

    if (loadFromCSV(dataFilePath)) {
        // Check if dimensions changed
        if (width != oldWidth || height != oldHeight) {
            std::cout << "Tilemap dimensions changed: "
                      << oldWidth << "x" << oldHeight << " → "
                      << width << "x" << height << std::endl;
            // Game may need to adjust camera bounds
        }
        return true;
    }
    return false;
}
```

---

## Graceful Failure

Hot-reload must **never crash** the game. If a file has errors, keep using the old version.

### Shader Failure Modes

```
Syntax error in GLSL:
┌────────────────────────────────────────┐
│ Hot-reload failed: Shader compilation  │
│ error: unexpected token at line 15     │
│                                        │
│ Keeping previous shader.               │
└────────────────────────────────────────┘

Game continues running with the old working shader.
You fix the syntax error, save again, and it reloads successfully.
```

### Tilemap Failure Modes

```
Malformed CSV:
┌────────────────────────────────────────┐
│ Hot-reload failed: Could not parse     │
│ tilemap at line 3                      │
│                                        │
│ Keeping previous tilemap.              │
└────────────────────────────────────────┘

Missing file:
┌────────────────────────────────────────┐
│ Hot-reload failed: File not found      │
│ assets/maps/farm.csv                   │
│                                        │
│ Keeping previous tilemap.              │
└────────────────────────────────────────┘
```

### The Golden Rule

```cpp
bool reload() {
    // Try to load new version
    NewAsset* newAsset = loadNewVersion();

    if (newAsset == nullptr) {
        // Failed - log error but DON'T crash
        std::cerr << "Reload failed, keeping old version" << std::endl;
        return false;
    }

    // Success - swap
    delete oldAsset;
    oldAsset = newAsset;
    return true;
}
```

**Never** delete the old asset until the new one is fully loaded and validated.

---

## Abstracting Hot-Reload: FileWatcher

Instead of duplicating file-watching logic in every reloadable class (Shader, Tilemap, Texture, etc.), we can extract it into a reusable **FileWatcher** utility.

### The Problem

Without abstraction, every reloadable asset has the same boilerplate:

```
Shader                              Tilemap
├── std::string path                ├── std::string path
├── file_time_type lastModified     ├── file_time_type lastModified
├── getFileModTime()                ├── getFileModTime()        ← Duplicated!
├── hasFileChanged()                ├── hasFileChanged()        ← Duplicated!
└── checkReload()                   └── checkReload()           ← Duplicated!
```

### The Solution: Composition

Create a `FileWatcher` class that handles all the file-watching logic. Any class that wants hot-reload just holds a FileWatcher instance.

```
FileWatcher (reusable utility)
├── std::vector<std::string> paths
├── std::vector<file_time_type> lastModified
├── hasChanged() → bool
└── updateTimestamps()

Shader                              Tilemap
├── FileWatcher watcher             ├── FileWatcher watcher
├── reload() ← asset-specific       ├── reload() ← asset-specific
└── checkReload() {                 └── checkReload() {
      if (watcher.hasChanged()) {         if (watcher.hasChanged()) {
        if (reload())                       if (reload())
          watcher.updateTimestamps();         watcher.updateTimestamps();
      }                                   }
    }                                   }
```

### FileWatcher Class

```cpp
// FileWatcher.h
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

class FileWatcher {
private:
    std::vector<std::string> paths;
    std::vector<fs::file_time_type> lastModified;

    fs::file_time_type getModTime(const std::string& path) const {
        try {
            return fs::last_write_time(path);
        } catch (const fs::filesystem_error&) {
            return fs::file_time_type::min();
        }
    }

public:
    // Watch a single file
    FileWatcher(const std::string& path) {
        paths.push_back(path);
        lastModified.push_back(getModTime(path));
    }

    // Watch multiple files (e.g., vertex + fragment shader)
    FileWatcher(const std::vector<std::string>& filePaths) {
        for (const auto& path : filePaths) {
            paths.push_back(path);
            lastModified.push_back(getModTime(path));
        }
    }

    // Check if ANY watched file has changed
    bool hasChanged() const {
        for (size_t i = 0; i < paths.size(); i++) {
            if (getModTime(paths[i]) != lastModified[i]) {
                return true;
            }
        }
        return false;
    }

    // Update timestamps after successful reload
    void updateTimestamps() {
        for (size_t i = 0; i < paths.size(); i++) {
            lastModified[i] = getModTime(paths[i]);
        }
    }

    // Get watched paths (for logging)
    const std::vector<std::string>& getPaths() const {
        return paths;
    }
};
```

### Using FileWatcher in Shader

```cpp
class Shader {
private:
    GLuint programID;
    FileWatcher watcher;  // Composition, not inheritance

public:
    Shader(const std::string& vertexPath, const std::string& fragmentPath)
        : programID(0),
          watcher({vertexPath, fragmentPath})  // Watch both files
    {
        // Initial compilation...
    }

    bool reload() {
        // Asset-specific reload logic
        // Compile new shaders, link, swap...
        return success;
    }

    bool checkReload() {
        if (watcher.hasChanged()) {
            if (reload()) {
                watcher.updateTimestamps();
                return true;
            }
        }
        return false;
    }
};
```

### Using FileWatcher in Tilemap

```cpp
class Tilemap {
private:
    std::vector<int> tiles;
    FileWatcher watcher;

public:
    Tilemap(const std::string& csvPath, int tileSize, Texture* tileset)
        : watcher(csvPath)  // Watch single file
    {
        loadFromCSV(csvPath);
    }

    bool reload() {
        // Asset-specific: re-parse CSV, update tiles vector
        return loadFromCSV(watcher.getPaths()[0]);
    }

    bool checkReload() {
        if (watcher.hasChanged()) {
            if (reload()) {
                watcher.updateTimestamps();
                return true;
            }
        }
        return false;
    }
};
```

### Web Development Analogy

This is like extracting a React hook or a utility function:

```javascript
// Instead of duplicating fetch logic in every component...
// You create a reusable hook:

function useFileWatcher(paths) {
    const [changed, setChanged] = useState(false);
    // ... watching logic
    return { changed, acknowledge };
}

// Then use it anywhere:
function ShaderEditor() {
    const watcher = useFileWatcher(['vertex.glsl', 'fragment.glsl']);
    if (watcher.changed) reloadShader();
}

function TilemapEditor() {
    const watcher = useFileWatcher(['map.csv']);
    if (watcher.changed) reloadMap();
}
```

### Benefits of Composition

| Aspect | Without FileWatcher | With FileWatcher |
|--------|---------------------|------------------|
| Code duplication | File watching in every class | Written once |
| Testing | Test each class separately | Test FileWatcher once |
| Flexibility | Fixed to one pattern | Can watch 1 or N files |
| Maintenance | Fix bugs in multiple places | Fix in one place |

### When NOT to Use FileWatcher

If a class has **special** file-watching needs (e.g., watching a directory instead of files, or needing file content hashes instead of timestamps), it can implement its own logic. FileWatcher is for the common case.

---

## Integration with Game Loop

### Where to Check for Changes

```cpp
void Game::update(float deltaTime) {
    // Check for asset changes (very cheap, ~0.01ms)
    tileShader->checkAndReload();
    spriteShader->checkAndReload();
    tilemap->checkAndReload();

    // Normal game logic
    player->update(deltaTime);
    // ...
}
```

### Throttling (Optional)

If you want to reduce filesystem calls:

```cpp
float reloadCheckTimer = 0.0f;
const float RELOAD_CHECK_INTERVAL = 0.5f;  // Check twice per second

void Game::update(float deltaTime) {
    reloadCheckTimer += deltaTime;

    if (reloadCheckTimer >= RELOAD_CHECK_INTERVAL) {
        reloadCheckTimer = 0.0f;

        tileShader->checkAndReload();
        spriteShader->checkAndReload();
        tilemap->checkAndReload();
    }

    // ...
}
```

For most games, checking every frame is fine. The filesystem call is extremely fast.

---

## Summary

| Concept | Description |
|---------|-------------|
| Hot-reload | Update assets without restarting |
| File watching | Detect when files change via modification time |
| Atomic swap | Only replace old asset after new one fully loads |
| Graceful failure | Never crash on bad data, keep old version |
| Polling | Check modification time every frame (cheap) |

### Key Pattern

```cpp
bool checkAndReload() {
    if (fileModificationTime != lastKnownTime) {
        return reload();  // Returns false on failure
    }
    return false;  // No change
}

bool reload() {
    NewThing* newThing = tryLoadNew();
    if (newThing) {
        delete oldThing;
        oldThing = newThing;
        lastKnownTime = fileModificationTime;
        return true;
    }
    return false;  // Keep old, don't crash
}
```

### What Can Be Hot-Reloaded

| Asset Type | Difficulty | Notes |
|------------|------------|-------|
| Shaders (GLSL) | Easy | Best ROI for visual work |
| Tilemaps (CSV/JSON) | Easy | Great for level design |
| Textures | Medium | Need to re-upload to GPU |
| Audio | Medium | Need audio library support |
| C++ Code | Very Hard | Requires recompilation |

---

## Why This Matters for HD-2D

HD-2D (the visual style from Octopath Traveler) relies heavily on **shader effects**:

```
HD-2D Visual Components:
├── Depth of field blur (background/foreground separation)
├── Dynamic lighting and shadows
├── Color grading and tone mapping
├── Vignette effects
├── Bloom and glow
├── Pixel art filtering/scaling
└── Particle effects
```

All of these are implemented in shaders. When developing HD-2D visuals, you'll constantly tweak values:

```glsl
// "Is 0.3 the right blur amount? Let me try 0.25..."
float blurAmount = 0.3;

// "Should the shadow be darker? Try 0.7..."
float shadowIntensity = 0.5;

// "What if the bloom threshold was lower?"
float bloomThreshold = 0.8;
```

Each of these experiments used to require:
- Stop game → Recompile → Run → Navigate back → Check result

With hot-reload:
- Change value → Save → See result **instantly**

This is why **every professional game engine** (Unity, Unreal, Godot) has shader hot-reload built in. It's not a nice-to-have — it's essential for visual development.

### The HD-2D Workflow

```
With hot-reload enabled:

1. Run game, navigate to a scene you want to style
2. Open shader in editor (side by side with game window)
3. Tweak a value, save
4. See result instantly in game
5. Not quite right? Tweak again, save
6. Repeat until perfect
7. Move to next effect

This turns hours of work into minutes.
```
