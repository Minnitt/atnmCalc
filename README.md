# atnmCalc

A LaTeX equation calculator with a GUI, built for Advanced Algorithms
Assignment 1. Type an equation in LaTeX (`\frac{1}{2} + \sqrt{9}`), define
any variables it uses, see it rendered as proper math notation (fraction
bars, radicals, superscripts) and evaluated live.

## Architecture

```
LaTeX string --> [Lexer] --> tokens --> [Parser] --> AST --> [Evaluator] --> double
                                                        |
                                                        +--> [Renderer] --> drawn equation
```

The `ASTNode` tree (`ast.h`) is the shared contract between everything
downstream of parsing: the evaluator walks it to compute a number, and
the GUI's renderer walks the *same* tree to draw it. Neither one needs
to know how the tree was built.

## Algorithm

The parser is a hybrid: **shunting-yard**'s operator-precedence
mechanism (an operand stack, an operator stack, precedence/associativity
comparisons) resolves the flat arithmetic (`+ - * / ^`), while `(`,
`\frac{`, and `\sqrt{` each trigger a **recursive** call back into the
same parsing function to handle their bracketed sub-expressions —
something classical shunting-yard alone has no mechanism for, since it
was designed for flat infix expressions with no grouping structure
beyond plain parentheses. Rather than emitting an intermediate postfix
(RPN) token sequence as the textbook algorithm does, operators are
resolved directly into `ASTNode`s the moment they're popped, since the
GUI needs the tree, not a flat notation for it.

## Project layout

| File | What it does |
|---|---|
| `src/lexer.h` / `.cpp` | Tokenizes a LaTeX string into a `vector<Token>`, terminated by an `END` sentinel |
| `src/parser.h` / `.cpp` | Shunting-yard + recursion hybrid; `vector<Token>` -> `ASTNodePtr` |
| `src/evaluator.h` / `.cpp` | Recursive tree walk; `ASTNodePtr` + variable map -> `double` |
| `src/ast.h` | Shared AST node types (`NumberNode`, `BinaryOpNode`, `FracNode`, `SqrtNode`, etc.) — the contract between parsing, evaluation, and rendering |
| `src/backend_interface.h` | `parseLatex()` / `evaluateAst()` signatures — what the GUI actually calls |
| `src/backend.cpp` | Wires lexer -> parser -> evaluator together to implement `backend_interface.h` |
| `src/ast_renderer.h` / `.cpp` | Draws an `ASTNode` tree as math notation: fraction bars, hand-drawn radicals, raised/shrunk exponents |
| `src/app.h` / `.cpp` | The row-based UI — each row is an independent equation with an editable title, input, rendered preview, variable inputs, and result. Also owns the File menu, Settings/Preferences, UI scaling, and the resizable layout (see below) |
| `src/main.cpp` | GLFW/OpenGL window setup and the ImGui frame loop; detects the monitor's content scale on first launch |
| `src/tests/lexerTesting.cpp` | gtest suite for the lexer |
| `src/tests/parserTesting.cpp` | gtest suite for the parser (tree shape + evaluated values, via a small test-only evaluator) |
| `src/tests/evaluatorTesting.cpp` | gtest suite for the evaluator, run end-to-end through the real lexer + parser |
| `testEquations.json` | Example equations file, in the app's own save format — covers every category from the test list plus a few deliberate error cases. Load via `File > Open` |
| `CMakeLists.txt` | Build config — fetches GLFW, Dear ImGui, googletest, nlohmann/json, and tinyfiledialogs automatically |

## Building

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/atnmCalc
```

First configure will take a minute — it's downloading GLFW, Dear ImGui,
googletest, nlohmann/json, and tinyfiledialogs via `FetchContent`. No
manual vendoring needed. Requires a C++20 **and** C compiler (tinyfiledialogs
is plain C), CMake >= 3.16, and (on Linux) X11 + OpenGL dev headers:

```
sudo apt install libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
```

## Testing

Three separate gtest executables, one per backend component, each built
from exactly the source files it actually exercises:

```
cmake --build build --target lexerTests parserTests evaluatorTests -j
./build/lexerTests
./build/parserTests
./build/evaluatorTests
```

`evaluatorTests` in particular runs end-to-end through the real
`Lexer` and `Parser` (not a mock), so it doubles as an integration check
on the whole pipeline. None of these build or touch the GUI (`app.cpp`)
at all.

## GUI features

- **File menu** — New, Open..., Save, Save As..., all using native OS file
  dialogs (via tinyfiledialogs). Equations save as JSON (nlohmann/json):
  an array of `{title, input, variables}`, where `variables` is a
  name → value map, so variable values survive a save/reload round trip.
  "Save" reuses whatever path was last opened/saved to in this session;
  if none yet, it behaves like "Save As".
- **Settings > Preferences** — a UI scale slider (0.5x–3x). Dragging only
  commits the change (restyles the UI, resizes the Preferences window,
  writes to `atnmcalc_settings.txt`) once you release the slider, not on
  every frame mid-drag (`ImGui::IsItemDeactivatedAfterEdit()`), so the
  window resizing mid-drag can't feed back into the slider's own position.
  On a machine with no saved preference yet, the monitor's content scale
  is used as the starting value (`main.cpp`, via
  `glfwGetWindowContentScale`), so the app isn't tiny by default on a
  HiDPI display — but never overrides a scale you've explicitly set.
- **Editable row titles** — click the small title line above any equation
  to rename it; shows "Equation N" as a greyed-out hint when empty.
- **Resizable layout** — three drag handles per row: between the equation
  preview and the variable inputs, between the equation column and the
  Result panel, and one at the bottom of each row (which also serves as
  the divider before the next row). All three are **shared across every
  row** — dragging any one of them resizes every row identically, by
  design, so rows can never end up inconsistently sized with each other.
  Double-click any handle to reset it back to auto-fit.
- **Horizontal scrolling** on the equation preview, for equations that
  render wider than the box (long implicit-multiplication chains
  especially). The variables panel instead **wraps to multiple lines**
  rather than scrolling, since seeing/setting several values at once
  matters more there than it does for a purely visual equation render.

## Design decisions

A few points where the grammar could reasonably have gone either way —
documented here so the reasoning doesn't get lost, and so the report's
"what I built" section has somewhere to draw from:

- **Each letter is its own variable.** `xy` is the two variables `x` and
  `y`, not one variable named `xy` — matching LaTeX math-mode convention,
  where `xy` typesets as two adjacent italic symbols. Multi-letter names
  would need `\text{...}` in real LaTeX, which isn't currently supported.
- **Implicit multiplication via juxtaposition** — `2x`, `xy`, `2(x+1)`
  all mean multiplication, since per-character variables only make sense
  combined with this. It deliberately does **not** apply between two
  bare numbers: `2 3` is a parse error, not `2*3`, since that combination
  is essentially always a typo, unlike `2x`.
- **Unary minus binds looser than `^`**: `-2^2` evaluates to `-4` (i.e.
  `-(2^2)`), matching standard mathematical convention and most
  calculators/languages (Python included). `(-2)^2` needs explicit
  parentheses to get `4`.
- **`\frac` vs `/`**: both mean division and evaluate identically, but
  render differently — `\frac{a}{b}` draws as a stacked fraction with a
  bar, while `a/b` draws as an inline slash. `FracNode` and
  `BinaryOpNode(BinOp::Div, ...)` are kept as separate AST node types
  specifically so the renderer can tell them apart.

## Extending the AST

To add LaTeX support the renderer doesn't know about yet (`\sin`, `\sum`,
matrices, etc.):
1. Add a new node struct + `NodeType` enum value in `ast.h`.
2. Add a `case` for it in the parser, the evaluator, and both
   `Measure()`/`Draw()` in `ast_renderer.cpp`.

## Known simplifications (documented, not bugs)

- Parentheses drawn around sub-expressions don't stretch vertically to
  match tall content (a nested fraction inside parens will look a bit
  cramped) — real typesetting engines do this; this one doesn't. The
  radical sign *does* stretch to match its content's height, since it's
  hand-drawn as line segments (see `ast_renderer.cpp`'s `Sqrt` case)
  rather than a text glyph — ImGui's default font doesn't contain the
  `√` character (U+221A) and silently falls back to `?` otherwise.
- Nested fractions/roots don't shrink progressively in size the way
  real LaTeX does — every level renders at the same relative scale
  (except exponents, which do shrink).
- The evaluator throws C++ exceptions internally and catches them once
  at the `evaluateAst`/`evaluateAST` boundary, converting them into
  `EvalResult`'s `success`/`errorMessage` fields — the recursive
  tree-walk itself never has to thread error state through every call.

## Open items

- `ParserDesignDecision.BareNumbersWithNoOperatorBetweenThem` in
  `parserTesting.cpp` is currently `GTEST_SKIP()`-ed even though the
  behaviour itself is decided and implemented (`2 3` throws) — swap it
  for a real `EXPECT_THROW` assertion.
