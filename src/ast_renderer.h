#pragma once
// ast_renderer.h
//
// Draws an ASTNode tree as standard math notation (fraction bars,
// radical signs, raised/shrunk exponents) using ImGui's draw list.
// This is a small two-pass typesetter: Measure() computes sizes
// without drawing anything (needed because, e.g., a fraction bar's
// width depends on the wider of its numerator/denominator, which you
// only know after measuring both), then Draw() places everything.

#include "ast.h"
#include "imgui.h"

// Total size (in pixels) that `node` will occupy when drawn at `fontSize`.
// Useful for centering the equation inside a fixed-size box before drawing it.
ImVec2 MeasureEquation(const ASTNodePtr& node, float fontSize);

// Draws `node` at the current ImGui cursor position and advances the
// cursor by the space used (like any other ImGui widget). Call this
// from inside a window/child region, e.g.:
//
//   ImGui::SetCursorPosX((availableWidth - size.x) * 0.5f); // to center
//   DrawEquation(parseResult.root, ImGui::GetFontSize());
void DrawEquation(const ASTNodePtr& node, float fontSize);
