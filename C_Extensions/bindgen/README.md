# Ludork Bindgen — Automatic pybind11 Binding Generator

An automatic binding generation system inspired by UHT (Unreal Header Tool). It parses
`BIND_*` annotation comments in C++ header files and generates pybind11 binding code
along with CMakeLists.txt files.

## Requirements

```bash
pip install libclang
```

## Quick Start

### 1. Annotate your headers

Place `// BIND_*` annotations above the classes, functions, and members you wish to export:

```cpp
// BIND_CLASS
// Manages the lifecycle and rendering of all particles.
class ParticleSystem : public sf::Drawable {
public:
    // BIND_INIT
    ParticleSystem();

    // BIND_METHOD
    // Add a particle to the system.
    void addParticle(Particle* particle);

    // BIND_METHOD(return_policy="reference_internal")
    // Retrieve the particle at the given index.
    Particle* getParticle(int index);

    // BIND_PROPERTY
    // Current particle count.
    int count;

    // BIND_PROPERTY(readonly=true)
    // Whether the system is currently running.
    bool running;

private:
    // Private members are never bound.
    std::vector<Particle*> particles_;
};

// BIND_FUNCTION
// Convert a hexadecimal colour string to an SFML Color.
sf::Color C_HexColor(const std::string &value, int alpha = 255);
```

### 2. Enable bindgen in extensions.toml

```toml
[my_ext]
name = "MyExt"
target_dir = "Sample/MyModule"
needs_pysf = true

[my_ext.bindgen]
enabled = true
internals_id = "PYSF"
pysf_import = "MyExt.pysf"      # Optional: auto-import pysf on module load
extra_includes = ["<custom.h>"]  # Optional: additional #include directives
include_paths = ["../shared"]    # Optional: extra include search paths
clang_args = ["-DSOME_DEFINE"]   # Optional: flags forwarded to clang
```

### 3. Run the generator

```bash
# Run bindgen on its own
python C_Extensions/bindgen/generate.py

# Generate only a specific module
python C_Extensions/bindgen/generate.py --only EngineExt

# Full build (bindgen runs automatically first)
python C_Extensions/build.py

# Skip bindgen and use existing binding files
python C_Extensions/build.py --skip-bindgen
```

## Annotation Reference

### `// BIND_CLASS`

Mark a class or struct for export to Python.

**Options:**
- `module_name="Name"` — Python-side class name (defaults to the C++ class name)

**Behaviour:**
- Only `public` members are bound.
- Base classes are detected automatically for pybind11 inheritance declarations.
- Comment lines immediately below the annotation become the class docstring.

```cpp
// BIND_CLASS(module_name="Map")
// C++ accelerated implementation of the game map.
class GameMapExt { ... };
```

### `// BIND_METHOD`

Mark a method for export.

**Options:**
- `return_policy="..."` — Return-value policy, e.g. `reference_internal`, `take_ownership`

**Behaviour:**
- `const`, `virtual`, and `static` qualifiers are detected automatically.
- Overloaded methods are disambiguated via `static_cast`.
- Comment lines below the annotation become the method docstring.

```cpp
// BIND_METHOD(return_policy="reference_internal")
// Return a reference to the parent object.
ParticleSystem* getParent() const;
```

### `// BIND_PROPERTY`

Mark a member variable for export.

**Options:**
- `readonly=true` — Bind as read-only (`def_readonly`)

```cpp
// BIND_PROPERTY(readonly=true)
// Current position of the particle.
sf::Vector2f position;
```

### `// BIND_FUNCTION`

Mark a free function for export.

**Options:**
- `return_policy="..."` — Return-value policy

**Behaviour:**
- Overloaded functions are disambiguated using a lambda wrapper.
- Default argument values are extracted automatically.

```cpp
// BIND_FUNCTION
// Efficiently update the texture buffer from a flat array.
void C_ImageUpdateBuffer1D(sf::Texture &img, py::buffer buffer);
```

### `// BIND_INIT`

Mark a constructor for export.

```cpp
// BIND_INIT
GameMapExt(sf::Shader *shader);
```

### `// BIND_IGNORE`

Explicitly exclude a public member from binding.

```cpp
// BIND_IGNORE
void internalSetup();
```

## Docstrings

Consecutive comment lines placed directly below an annotation are collected as the
Python docstring:

```cpp
// BIND_METHOD
// Perform A* pathfinding on the map.
// Returns a list of coordinates from start to goal.
// Returns an empty list if the goal is unreachable.
std::vector<sf::Vector2i> findPathExt(...);
```

Generated binding code:

```cpp
cls.def("findPathExt", &GameMapExt::findPathExt, ...,
    "Perform A* pathfinding on the map.\n"
    "Returns a list of coordinates from start to goal.\n"
    "Returns an empty list if the goal is unreachable.");
```

## Migration Tool

For projects with existing hand-written bindings, a migration helper can infer
annotations automatically:

```bash
# Analyse existing binding code and annotate headers
python -m bindgen.migrate EngineExt/src/Particles.cpp \
    EngineExt/include/Particles/ParticleBase.hpp \
    EngineExt/include/Particles/Particle.hpp

# Preview without writing
python -m bindgen.migrate EngineExt/src/Particles.cpp \
    EngineExt/include/Particles/*.hpp --dry-run
```

## Generated Files

Each extension with bindgen enabled will have the following generated:

```
ExtName/
├── _generated_bindings.cpp    # Generated pybind11 binding code
├── CMakeLists.txt             # Generated CMake configuration
├── include/                   # Your headers (annotated)
└── src/                       # Your implementation files
```

**Note:** The generated files (`_generated_bindings.cpp` and `CMakeLists.txt`) should
not be edited manually — they are overwritten on each bindgen run. Consider adding them
to `.gitignore`.

## Architecture

```
bindgen/
├── __init__.py       # Package declaration
├── parser.py         # libclang AST parser; extracts annotations and type info
├── codegen.py        # pybind11 C++ code generator + CMakeLists generator
├── generate.py       # Main entry point; orchestrates parsing and generation
├── migrate.py        # Migration helper for existing hand-written bindings
├── examples.py       # Annotation examples
└── README.md         # This file
```
