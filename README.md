# atnmCalc — GUI scaffold

## What's here

| File | What it does | Who owns it |
|---|---|---|
| `src/ast.h` | Shared AST node types (`NumberNode`, `FracNode`, etc.) | **Contract** — both sides use this |
| `src/backend_interface.h` | `parseLatex()` / `evaluateAst()` signatures | **Contract** — both sides use this |
| `src/mock_backend.cpp` | Throwaway placeholder implementing the interface | **Delete once you have a real backend** |
| `src/ast_renderer.h/.cpp` | Draws an AST as fraction bars / radicals / superscripts | GUI |
| `src/app.h/.cpp` | The row-based UI (matches the wireframe) | GUI |
| `src/main.cpp` | Window setup, frame loop | GUI |
| `CMakeLists.txt` | Build config (fetches GLFW + Dear ImGui automatically) | GUI |

## Building

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/atnmCalc
```

First configure will take a minute — it's downloading GLFW and Dear ImGui
via `FetchContent`. No manual vendoring needed. Needs a C++17 compiler,
CMake ≥ 3.16, and (on Linux) X11 + OpenGL dev headers:

```
sudo apt install libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
```

## Swapping in your real backend

1. Delete `src/mock_backend.cpp`.
2. Remove that line from `CMakeLists.txt`'s `add_executable(...)` list.
3. Add your own source file(s) implementing the two functions declared in
   `backend_interface.h`:
   ```cpp
   ParseResult parseLatex(const std::string& latex);
   EvalResult evaluateAst(const ASTNodePtr& root, const std::map<std::string, double>& variables);
   ```
   Your parser should build trees out of the node types in `ast.h`
   (`NumberNode`, `VariableNode`, `BinaryOpNode`, `UnaryMinusNode`,
   `FracNode`, `SqrtNode`) — that's the only thing the renderer needs to
   know how to draw. Everything else about how you tokenize/parse is
   entirely up to you.
4. Add those source files to `CMakeLists.txt` in place of `mock_backend.cpp`.
5. Rebuild. The GUI, renderer, and variable-input logic don't need to change at all.

## Extending the AST

If you add LaTeX support your backend needs but the renderer doesn't
know about yet (`\sin`, `\sum`, matrices, etc.):
1. Add a new node struct + `NodeType` enum value in `ast.h`.
2. Add a `case` for it in both `Measure()` and `Draw()` in `ast_renderer.cpp`.
3. Have your parser produce it.

## Known simplifications (documented, not bugs)

- Parentheses drawn around sub-expressions don't stretch vertically to
  match tall content (a nested fraction inside parens will look a bit
  cramped) — real typesetting engines do this; this one doesn't.
- Nested fractions/roots don't shrink progressively in size the way
  real LaTeX does — every level renders at the same relative scale
  (except exponents, which do shrink).
- `evaluateAst` in the mock backend throws C++ exceptions internally
  and catches them at the top — fine for a stub, but consider whether
  your real evaluator wants that style or an explicit error-return style instead.
