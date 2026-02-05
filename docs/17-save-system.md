# Save/Load System

This document explains how to implement a save/load system for persisting player progress across game sessions.

## Table of Contents
- [What is a Save System?](#what-is-a-save-system)
- [What to Save](#what-to-save)
- [File Formats](#file-formats)
- [Save Triggers](#save-triggers)
- [Load Flow](#load-flow)
- [Implementation Approaches](#implementation-approaches)
- [File I/O in C++](#file-io-in-c)

---

## What is a Save System?

A save system persists game state to disk so players can resume from where they left off. When the game starts, it checks for existing save data and restores the player's progress.

```
Without save:                    With save:
┌─────────────────┐              ┌─────────────────┐
│ Start game      │              │ Start game      │
│ Always at farm  │              │ Check save file │
│ Position (0,0)  │              │ Load last state │
│ Fresh start     │              │ Resume progress │
└─────────────────┘              └─────────────────┘
```

### Web Development Analogy

Think of it like `localStorage` or cookies in web development:

```javascript
// Web: Saving user preferences
localStorage.setItem('userSettings', JSON.stringify({
    theme: 'dark',
    volume: 0.8,
    lastPage: '/dashboard'
}));

// Web: Loading on page refresh
const settings = JSON.parse(localStorage.getItem('userSettings'));
if (settings) {
    applyTheme(settings.theme);
    setVolume(settings.volume);
    navigate(settings.lastPage);
}
```

The game equivalent:

```cpp
// Game: Saving player state
saveGame({
    location: "farm",
    position: { x: 400, y: 300 },
    health: 85
});

// Game: Loading on startup
SaveData save = loadGame();
if (save.exists) {
    changeLocation(save.location);
    player->setPosition(save.position);
    player->setHealth(save.health);
}
```

---

## What to Save

### Minimal Save (Start Here)

```cpp
struct SaveData {
    // Where the player is
    std::string currentLocationId;  // "farm", "town", etc.
    float playerX;
    float playerY;

    // Player state
    int health;
};
```

This lets players resume at their last position.

### Extended Save (Future)

```cpp
struct SaveData {
    // Player location
    std::string currentLocationId;
    Vector2 playerPosition;

    // Player stats
    int health;
    int maxHealth;
    int gold;

    // Inventory (later)
    std::vector<ItemStack> inventory;

    // Time system (later)
    int dayNumber;
    std::string season;  // "spring", "summer", etc.
    int timeOfDay;       // Minutes since midnight

    // Per-location state (later)
    // Which chests are opened, which enemies are defeated, etc.
    std::map<std::string, LocationState> locationStates;
};
```

### What NOT to Save

Some things should reset each session:

```
DON'T save:
├── Temporary effects (screen shake, particles)
├── Enemy positions (respawn fresh)
├── Camera position (follows player anyway)
├── Input state (keys pressed)
└── Runtime-only flags (isPaused, debugMode)
```

---

## File Formats

### Option 1: Plain Text (Simplest)

```
farm
400.5 300.2
85
```

**Pros:** Human-readable, easy to debug, no dependencies
**Cons:** Fragile (order matters), hard to extend

```cpp
// Writing
std::ofstream file("save.txt");
file << locationId << "\n";
file << playerX << " " << playerY << "\n";
file << health << "\n";

// Reading
std::ifstream file("save.txt");
file >> locationId >> playerX >> playerY >> health;
```

### Option 2: JSON (Recommended)

```json
{
    "version": 1,
    "location": "farm",
    "player": {
        "x": 400.5,
        "y": 300.2,
        "health": 85
    }
}
```

**Pros:** Self-documenting, easy to extend, human-readable
**Cons:** Requires JSON library (nlohmann/json recommended)

```cpp
// Using nlohmann/json
#include <nlohmann/json.hpp>
using json = nlohmann::json;

// Writing
json save;
save["version"] = 1;
save["location"] = "farm";
save["player"]["x"] = 400.5f;
save["player"]["y"] = 300.2f;
save["player"]["health"] = 85;

std::ofstream file("save.json");
file << save.dump(2);  // Pretty print with 2-space indent

// Reading
std::ifstream file("save.json");
json save = json::parse(file);
std::string location = save["location"];
float x = save["player"]["x"];
```

### Option 3: Binary (Performance)

```cpp
struct SaveHeader {
    char magic[4] = {'S', 'A', 'V', 'E'};
    uint32_t version = 1;
    uint32_t dataSize;
};

// Writing
std::ofstream file("save.bin", std::ios::binary);
file.write(reinterpret_cast<char*>(&saveData), sizeof(SaveData));

// Reading
std::ifstream file("save.bin", std::ios::binary);
file.read(reinterpret_cast<char*>(&saveData), sizeof(SaveData));
```

**Pros:** Fast, compact, no parsing overhead
**Cons:** Not human-readable, version compatibility issues, endianness concerns

### Recommendation

Start with **plain text** for learning, then move to **JSON** when you need more complex data. Binary only if performance becomes an issue (unlikely for save files).

---

## Save Triggers

### When to Save

| Trigger | Description | Example |
|---------|-------------|---------|
| **Auto-save** | When player quits | Closing window, pressing Escape |
| **Checkpoint** | At story points | Entering a new location, sleeping |
| **Manual** | Player request | Menu → Save Game |
| **Periodic** | Every N minutes | Auto-save every 5 minutes |

### Stardew Valley Approach

Stardew Valley saves once per day when you sleep:

```
Day cycle:
Morning → Play all day → Sleep → SAVE → Next morning
                              ↑
                        Only save point
```

This design choice:
- Prevents save-scumming (reload to undo mistakes)
- Creates meaningful decisions (you can't undo selling items)
- Simplifies save logic (one clear save point)

### Simple Auto-Save Approach

For our engine, start with auto-save on quit:

```cpp
void Game::stop() {
    saveGame();  // Save before stopping
    isRunning = false;
}
```

---

## Load Flow

### Startup Sequence

```
Game::Game()
├── Initialize window, OpenGL
├── Load textures, shaders
├── Setup locations
│
├── tryLoadSave()
│   ├── File exists?
│   │   ├── YES → Parse save data
│   │   │         Set currentLocation
│   │   │         Set player position
│   │   │         Restore health, etc.
│   │   │
│   │   └── NO  → Use defaults
│   │             currentLocation = "farm"
│   │             player at (400, 300)
│   │             full health
│
└── currentLocation->onEnter()
```

### Code Structure

```cpp
void Game::tryLoadSave() {
    std::ifstream file("save.txt");
    if (!file.is_open()) {
        std::cout << "No save file, starting new game" << std::endl;
        // Defaults are already set in constructor
        return;
    }

    std::cout << "Loading save file..." << std::endl;

    std::string locationId;
    float x, y;
    int health;

    file >> locationId >> x >> y >> health;

    // Validate location exists
    if (locations.find(locationId) == locations.end()) {
        std::cerr << "Invalid location in save: " << locationId << std::endl;
        return;
    }

    // Apply loaded state
    currentLocation = locations[locationId].get();
    player->setPosition(Vector2(x, y));
    player->setHealth(health);

    std::cout << "Loaded: " << locationId << " at (" << x << ", " << y << ")" << std::endl;
}
```

---

## Implementation Approaches

### Approach 1: Direct in Game Class (Simplest)

Add save/load methods directly to Game:

```cpp
// Game.h
class Game {
private:
    void saveGame();
    void loadGame();
    // ...
};

// Game.cpp
void Game::saveGame() {
    std::ofstream file("save.txt");
    file << currentLocation->getId() << "\n";
    file << player->getPosition().x << " " << player->getPosition().y << "\n";
    file << player->getHealth() << "\n";
    std::cout << "Game saved!" << std::endl;
}

void Game::loadGame() {
    std::ifstream file("save.txt");
    if (!file.is_open()) return;

    std::string locationId;
    float x, y;
    int health;
    file >> locationId >> x >> y >> health;

    currentLocation = locations[locationId].get();
    player->setPosition(Vector2(x, y));
    player->setHealth(health);
}
```

### Approach 2: SaveManager Class (Scalable)

Separate save logic into its own class:

```cpp
// SaveManager.h
class SaveManager {
public:
    static bool save(const std::string& filepath, const SaveData& data);
    static std::optional<SaveData> load(const std::string& filepath);
    static bool saveExists(const std::string& filepath);
};

// Usage in Game
SaveData data;
data.locationId = currentLocation->getId();
data.playerPosition = player->getPosition();
data.health = player->getHealth();

SaveManager::save("saves/slot1.txt", data);

// Loading
auto loaded = SaveManager::load("saves/slot1.txt");
if (loaded.has_value()) {
    applyLoadedState(loaded.value());
}
```

### Approach 3: Serializable Interface (Extensible)

Make game objects know how to save themselves:

```cpp
// ISaveable.h
class ISaveable {
public:
    virtual void serialize(std::ostream& out) const = 0;
    virtual void deserialize(std::istream& in) = 0;
};

// Player.h
class Player : public Entity, public ISaveable {
public:
    void serialize(std::ostream& out) const override {
        out << position.x << " " << position.y << " " << health << "\n";
    }

    void deserialize(std::istream& in) override {
        in >> position.x >> position.y >> health;
    }
};
```

### Recommendation

Start with **Approach 1** (direct in Game). It's simple and you can refactor later if needed. Premature abstraction adds complexity without benefit.

---

## File I/O in C++

### Basic File Writing

```cpp
#include <fstream>
#include <string>

// Writing text
std::ofstream outFile("data.txt");
if (outFile.is_open()) {
    outFile << "Hello\n";
    outFile << 42 << " " << 3.14f << "\n";
    outFile.close();  // Optional, destructor closes automatically
}

// Result in data.txt:
// Hello
// 42 3.14
```

### Basic File Reading

```cpp
#include <fstream>
#include <string>

std::ifstream inFile("data.txt");
if (inFile.is_open()) {
    std::string line;
    int number;
    float decimal;

    std::getline(inFile, line);  // Read "Hello"
    inFile >> number >> decimal; // Read 42 and 3.14
}
```

### Checking File Existence

```cpp
#include <filesystem>

bool fileExists = std::filesystem::exists("save.txt");
```

### Error Handling

```cpp
std::ifstream file("save.txt");
if (!file.is_open()) {
    std::cerr << "Could not open save file" << std::endl;
    return;
}

// Check for read errors
if (file.fail()) {
    std::cerr << "Error reading save file" << std::endl;
}
```

### Save File Location

Where to put save files:

```cpp
// Option 1: Same directory as executable (simple, but not standard)
"save.txt"

// Option 2: User's home directory (Linux)
std::string home = std::getenv("HOME");
std::string savePath = home + "/.local/share/mygame/save.txt";

// Option 3: Create a saves folder
std::filesystem::create_directories("saves");
"saves/slot1.txt"
```

---

## Summary

| Concept | Description |
|---------|-------------|
| **SaveData** | Struct holding all data to persist |
| **Save trigger** | When to write save (quit, checkpoint, manual) |
| **Load flow** | Check for save on startup, apply or use defaults |
| **File format** | Plain text (simple) → JSON (flexible) → Binary (fast) |
| **Validation** | Check save file exists, data is valid |

### Minimal Implementation Checklist

1. [ ] Define what to save (location ID, position, health)
2. [ ] Add `saveGame()` method to Game
3. [ ] Add `loadGame()` method to Game
4. [ ] Call `saveGame()` in `Game::stop()`
5. [ ] Call `loadGame()` at end of `Game::Game()` constructor
6. [ ] Test: play, quit, restart, verify position restored
