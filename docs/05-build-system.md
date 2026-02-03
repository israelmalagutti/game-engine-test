# Build System

This document covers the Makefile-based build system and C++ compilation process.

## Table of Contents
- [Compilation Overview](#compilation-overview)
- [The Makefile](#the-makefile)
- [Compiler Flags](#compiler-flags)
- [Linking Libraries](#linking-libraries)
- [Include Paths](#include-paths)
- [Common Build Issues](#common-build-issues)

---

## Compilation Overview

C++ compilation is a multi-stage process:

```
Source Files (.cpp)
       │
       ▼ Preprocessing
Expanded Source (includes resolved, macros expanded)
       │
       ▼ Compilation
Object Files (.o)
       │
       ▼ Linking
Executable (game)
```

### Stages Explained

1. **Preprocessing**: Handles `#include`, `#define`, `#ifdef`
2. **Compilation**: Converts C++ to machine code (object files)
3. **Linking**: Combines object files + libraries into executable

---

## The Makefile

```makefile
# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic -I./include

# Libraries
LIBS = -lSDL2 -lSDL2_image -lGL

# Find all source files
SOURCES = $(wildcard src/*.cpp) src/glad.c

# Output binary
TARGET = game

# Default target
all: $(TARGET)

# Build the game
$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET) $(LIBS)

# Clean build files
clean:
	rm -f $(TARGET)

# Build and run
run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
```

### Makefile Syntax

```makefile
# Variable assignment
VAR = value

# Variable usage
$(VAR)

# Target rule
target: dependencies
	command  # Must use TAB, not spaces!

# Wildcard function
$(wildcard pattern)  # Expands to matching files

# Phony targets (not real files)
.PHONY: clean run
```

### How It Works

When you run `make`:

1. Make reads the Makefile
2. Finds the default target (`all`)
3. Checks if `all`'s dependency (`game`) needs building
4. If `game` doesn't exist or sources are newer, runs the build command

---

## Compiler Flags

### Current Flags

```makefile
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic -I./include
```

| Flag | Purpose |
|------|---------|
| `-std=c++17` | Use C++17 standard |
| `-Wall` | Enable common warnings |
| `-Wextra` | Enable extra warnings |
| `-pedantic` | Strict ISO C++ compliance |
| `-I./include` | Add `./include` to header search path |

### Warning Flags Explained

**-Wall** enables warnings for:
- Unused variables
- Uninitialized variables
- Missing return statements
- Implicit conversions

**-Wextra** adds:
- Unused parameters
- Empty bodies in if/while
- Missing field initializers

**-pedantic** enforces:
- Strict standard compliance
- No compiler extensions

### Other Useful Flags

| Flag | Purpose |
|------|---------|
| `-g` | Include debug symbols (for gdb) |
| `-O2` | Optimization level 2 |
| `-O3` | Aggressive optimization |
| `-fsanitize=address` | Memory error detection |
| `-Werror` | Treat warnings as errors |

### Debug vs Release

```makefile
# Debug build
CXXFLAGS_DEBUG = -std=c++17 -Wall -Wextra -g -O0

# Release build
CXXFLAGS_RELEASE = -std=c++17 -Wall -Wextra -O2 -DNDEBUG
```

`-DNDEBUG` disables `assert()` statements.

---

## Linking Libraries

### Library Flags

```makefile
LIBS = -lSDL2 -lSDL2_image -lGL
```

| Flag | Library | Purpose |
|------|---------|---------|
| `-lSDL2` | libSDL2.so | Window, events, timing |
| `-lSDL2_image` | libSDL2_image.so | PNG/JPG loading |
| `-lGL` | libGL.so | OpenGL implementation |

### How Linking Works

The `-l` flag tells the linker to search for a library:
- `-lSDL2` → looks for `libSDL2.so` or `libSDL2.a`

The linker searches in:
1. Paths specified by `-L/path`
2. Standard paths (`/usr/lib`, `/usr/local/lib`)
3. Paths in `LD_LIBRARY_PATH`

### Static vs Dynamic Linking

| Type | Extension | Behavior |
|------|-----------|----------|
| Static | `.a` | Code copied into executable |
| Dynamic | `.so` | Code loaded at runtime |

This project uses dynamic linking (default). Benefits:
- Smaller executable
- Shared memory across processes
- Library updates don't require recompilation

---

## Include Paths

### How #include Works

```cpp
#include <SDL2/SDL.h>    // System include - searches system paths
#include "Common.h"      // Local include - searches current dir first
#include <glad/gl.h>     // System-style, but needs -I flag
```

### Search Paths

**System includes** (`<>`) search:
- `/usr/include`
- `/usr/local/include`
- Compiler-specific paths
- Paths added with `-I`

**Local includes** (`""`) search:
- Directory of the current file
- Then system paths

### Adding Include Paths

```makefile
CXXFLAGS = ... -I./include
```

This allows:
```cpp
#include <glad/gl.h>  // Found in ./include/glad/gl.h
```

Without `-I./include`, the compiler wouldn't find GLAD headers.

---

## Common Build Issues

### "file not found" Error

```
fatal error: glad/gl.h: No such file or directory
```

**Cause**: Include path not set
**Fix**: Add `-I` flag for the header directory

### "undefined reference" Error

```
undefined reference to `SDL_Init'
```

**Cause**: Library not linked
**Fix**: Add `-lSDL2` to link flags

### "multiple definition" Error

```
multiple definition of `Entity::Entity(...)'
```

**Cause**: Implementation in header file included multiple times
**Fix**: Move implementation to `.cpp` file, or use `inline`

### Order of Libraries

Library order matters! Libraries must come after the files that use them:

```makefile
# Correct
$(CXX) $(SOURCES) -o $(TARGET) $(LIBS)

# Wrong - may fail
$(CXX) $(LIBS) $(SOURCES) -o $(TARGET)
```

### Missing Dependencies

If you modify a header but `.cpp` files don't recompile:

```makefile
# Quick solution: clean and rebuild
make clean && make

# Better solution: use dependency tracking
# (more advanced Makefile feature)
```

---

## Separate Compilation (Advanced)

For larger projects, compile each file separately:

```makefile
# Object files
OBJECTS = $(SOURCES:.cpp=.o)

# Compile each .cpp to .o
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Link all objects
$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET) $(LIBS)
```

Benefits:
- Only recompile changed files
- Faster incremental builds
- Better parallelization (`make -j4`)

### Automatic Dependencies

```makefile
# Generate dependency files
DEPFLAGS = -MMD -MP
CXXFLAGS += $(DEPFLAGS)

# Include generated dependencies
-include $(SOURCES:.cpp=.d)
```

This automatically tracks which headers each source file uses.

---

## Build Commands Reference

| Command | Action |
|---------|--------|
| `make` | Build the project |
| `make all` | Same as `make` |
| `make clean` | Remove built files |
| `make run` | Build and run |
| `make -j4` | Build with 4 parallel jobs |
| `make -B` | Force rebuild everything |
| `make CXXFLAGS="-O2"` | Override variables |

---

## IDE Configuration

### .clangd File

For IDE features (autocomplete, error checking):

```yaml
CompileFlags:
  Add: [-I./include]
```

This tells clangd (used by VS Code, Neovim, etc.) where to find headers.

### compile_commands.json

Alternative for more complex projects:

```bash
# Generate with bear
bear -- make

# Or with cmake
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..
```

Creates a JSON file describing how each file is compiled.
