// app.cpp
//
// Layout per row (matches the wireframe):
//   [n]  Enter equation: [___________________]
//   +-------------------------------+  +-----------+
//   | rendered equation preview     |  | Result    |
//   | variable inputs               |  | (rendered)|
//   +-------------------------------+  +-----------+
//
// Each row is independent (per your earlier answer): no row can see
// another row's variables. Re-parsing happens on every keystroke, which
// is fine at this scale — if your real parser gets slow on long input,
// this is the first place to add debouncing.

#include "app.h"
#include "ast_renderer.h"
#include "imgui.h"
#include <algorithm>
#include <cstdio>
#include <memory>

void App::ReparseAndEvaluate(Row& row) {
    row.parseResult = parseLatex(row.input);
    if (row.parseResult.success) {
        // Keep values the user already typed for variables that still
        // exist after re-parsing; default any newly-appeared variable to 0.
        std::map<std::string, double> newValues;
        for (const auto& name : row.parseResult.variables) {
            auto it = row.varValues.find(name);
            newValues[name] = (it != row.varValues.end()) ? it->second : 0.0;
        }
        row.varValues = std::move(newValues);
        row.evalResult = evaluateAst(row.parseResult.root, row.varValues);
    } else {
        row.varValues.clear();
        row.evalResult = EvalResult{};
    }
    row.dirty = false;
}

void App::DrawRow(Row& row) {
    ImGui::PushID(row.id);
    ImGui::Separator();

    ImGui::Text("%d", row.id);
    ImGui::SameLine(40);
    ImGui::Text("Enter equation:");
    ImGui::SameLine();

    char buf[512];
    std::snprintf(buf, sizeof(buf), "%s", row.input.c_str());
    ImGui::PushItemWidth(-1);
    if (ImGui::InputText("##eq", buf, sizeof(buf))) {
        row.input = buf;
        row.dirty = true;
    }
    ImGui::PopItemWidth();

    if (row.dirty) ReparseAndEvaluate(row);

    const float resultWidth = 170.0f;
    const float previewFontSize = ImGui::GetFontSize() * 1.8f; // bumped up — was too small for stacked exponents
    const float resultFontSize = ImGui::GetFontSize() * 2.0f;
    const float previewPadding = 16.0f;   // vertical breathing room inside the preview box
    const float minPreviewHeight = 55.0f;
    const float variablesHeight = 60.0f;
    const float spacing = ImGui::GetStyle().ItemSpacing.y;

    // Size the preview box to whatever the equation actually needs, rather
    // than a fixed guess — this is what removes the scrollbar/clipping on
    // tall content like nested fractions or stacked exponents.
    float previewHeight = minPreviewHeight;
    if (!row.input.empty() && row.parseResult.success) {
        ImVec2 eqSize = MeasureEquation(row.parseResult.root, previewFontSize);
        previewHeight = std::max(minPreviewHeight, eqSize.y + previewPadding);
    }
    float leftWidth = ImGui::GetContentRegionAvail().x - resultWidth - 8.0f;
    float totalHeight = previewHeight + spacing + variablesHeight;

    // BeginGroup (rather than a fixed-size BeginChild) so this container
    // doesn't need to guess its own height up front — it just wraps
    // whatever height the preview+vars children end up being.
    ImGui::BeginGroup();
    {
        ImGui::BeginChild("preview", ImVec2(leftWidth, previewHeight), true, ImGuiWindowFlags_NoScrollbar);
        if (row.input.empty()) {
            ImGui::TextDisabled("(rendered equation appears here)");
        } else if (!row.parseResult.success) {
            ImGui::TextColored(ImVec4(0.85f, 0.3f, 0.3f, 1.0f), "%s", row.parseResult.errorMessage.c_str());
        } else {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            ImVec2 size = MeasureEquation(row.parseResult.root, previewFontSize);
            ImGui::SetCursorPos(ImVec2(std::max(4.0f, (avail.x - size.x) * 0.5f),
                                        std::max(4.0f, (avail.y - size.y) * 0.5f)));
            DrawEquation(row.parseResult.root, previewFontSize);
        }
        ImGui::EndChild();

        ImGui::BeginChild("vars", ImVec2(leftWidth, variablesHeight), true);
        if (row.parseResult.success && !row.parseResult.variables.empty()) {
            ImGui::TextDisabled("Define variables:");
            for (const auto& name : row.parseResult.variables) {
                ImGui::SetNextItemWidth(100);
                float v = static_cast<float>(row.varValues[name]);
                if (ImGui::InputFloat(name.c_str(), &v)) {
                    row.varValues[name] = v;
                    row.evalResult = evaluateAst(row.parseResult.root, row.varValues);
                }
                ImGui::SameLine();
            }
            ImGui::NewLine();
        } else {
            ImGui::TextDisabled("(no variables in this equation)");
        }
        ImGui::EndChild();
    }
    ImGui::EndGroup();

    ImGui::SameLine();

    // Rendered solution — height matches the group above so the two sides
    // of the row line up regardless of how tall the equation preview got.
    ImGui::BeginChild("result", ImVec2(resultWidth, totalHeight), true);
    ImGui::TextDisabled("Result");
    ImGui::Separator();
    if (row.input.empty() || !row.parseResult.success) {
        ImGui::TextDisabled("--");
    } else if (!row.evalResult.success) {
        ImGui::TextWrapped("%s", row.evalResult.errorMessage.c_str());
    } else {
        // Reuse the same renderer for the answer, wrapped as a NumberNode,
        // so the result looks visually consistent with the equation above.
        ASTNodePtr resultNode = std::make_shared<NumberNode>(row.evalResult.value);
        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImVec2 size = MeasureEquation(resultNode, resultFontSize);
        ImGui::SetCursorPos(ImVec2(std::max(4.0f, (avail.x - size.x) * 0.5f), ImGui::GetCursorPosY() + 12.0f));
        DrawEquation(resultNode, resultFontSize);
    }
    ImGui::EndChild();

    ImGui::PopID();
}

void App::Draw() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::Begin("atnmCalc", nullptr,
                  ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_MenuBar);

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            ImGui::MenuItem("New", nullptr, false, false);
            ImGui::MenuItem("Save", nullptr, false, false);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Settings")) {
            ImGui::MenuItem("Preferences", nullptr, false, false);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    for (auto& row : rows_) DrawRow(row);

    ImGui::Separator();
    if (ImGui::Button("+ Add row")) {
        rows_.push_back(Row{nextId_++});
    }

    ImGui::End();
}
