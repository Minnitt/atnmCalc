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
#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>

App::App() {
    // Capture the style as StyleColorsDark() left it, BEFORE any scaling --
    // SetUiScale() always re-derives from this rather than from whatever
    // the style currently is, since ImGuiStyle::ScaleAllSizes() multiplies
    // in place: calling it twice in a row would double-scale everything.
    baseStyle_ = ImGui::GetStyle();
    LoadSettings();
    if (hasSavedScale_) {
        SetUiScale(uiScale_); // re-apply the saved scale to the style/fonts now that we have it
    }
}

void App::ApplySuggestedScaleIfUnset(float suggestedScale) {
    if (hasSavedScale_) return; // user has an explicit saved preference -- never override it
    SetUiScale(suggestedScale);
}

void App::SetUiScale(float scale) {
    uiScale_ = std::clamp(scale, 0.5f, 3.0f);
    ImGui::GetStyle() = baseStyle_;
    ImGui::GetStyle().ScaleAllSizes(uiScale_);
    // NOTE: io.FontGlobalScale scales ImGui's own widget text (menus,
    // buttons, input boxes) but does NOT affect ast_renderer.cpp's
    // DrawEquation/MeasureEquation calls -- those use ImDrawList::AddText's
    // explicit-font-size overload, which bypasses FontGlobalScale entirely.
    // That's why DrawRow's previewFontSize/resultFontSize below multiply
    // by uiScale_ directly instead of relying on this.
    ImGui::GetIO().FontGlobalScale = uiScale_;

    // Force the Preferences window to its correctly-scaled size right now.
    // Without this, dragging the slider while the window is already open
    // re-scales its CONTENTS (bigger slider, bigger wrapped text) but the
    // window itself keeps whatever size it had when it first opened --
    // SetNextWindowSize's ImGuiCond_FirstUseEver only ever applies once
    // per process, not every time the scale changes. SetWindowSize by name
    // works even when the window isn't currently open; it's just a no-op
    // until that window actually exists.
    ImGui::SetWindowSize("Preferences", ImVec2(340.0f * uiScale_, 0.0f));

    SaveSettings();
}

std::string App::SettingsFilePath() { return "atnmcalc_settings.txt"; }
std::string App::EquationsFilePath() { return "atnmcalc_equations.json"; }

void App::LoadSettings() {
    std::ifstream in(SettingsFilePath());
    if (!in) return; // no saved settings yet -- fine, hasSavedScale_ stays false
    std::string line;
    while (std::getline(in, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);
        if (key == "uiScale") {
            try {
                uiScale_ = std::stof(value);
                hasSavedScale_ = true;
            } catch (const std::exception&) {
                // corrupted settings file -- ignore rather than crash on startup
            }
        }
    }
}

void App::SaveSettings() const {
    std::ofstream out(SettingsFilePath());
    if (!out) return; // best-effort -- a failed save shouldn't crash the app
    out << "uiScale=" << uiScale_ << "\n";
}

void App::NewFile() {
    rows_.clear();
    rows_.push_back(Row{1});
    nextId_ = 2;
}

void App::SaveEquationsToFile() const {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& row : rows_) {
        nlohmann::json obj;
        obj["title"] = row.title;
        obj["input"] = row.input;
        nlohmann::json vars = nlohmann::json::object();
        for (const auto& [name, value] : row.varValues) vars[name] = value;
        obj["variables"] = vars;
        arr.push_back(obj);
    }
    std::ofstream out(EquationsFilePath());
    if (!out) return;
    out << arr.dump(2); // pretty-printed, 2-space indent -- readable if you open it by hand
}

void App::OpenEquationsFromFile() {
    std::ifstream in(EquationsFilePath());
    if (!in) return; // nothing saved yet -- leave current rows alone

    nlohmann::json arr;
    try {
        in >> arr;
    } catch (const nlohmann::json::parse_error&) {
        return; // malformed/corrupted file -- leave current rows alone rather than crash
    }
    if (!arr.is_array()) return;

    std::vector<Row> loaded;
    int id = 1;
    for (const auto& obj : arr) {
        Row row{id++};
        row.title = obj.value("title", "");
        row.input = obj.value("input", "");
        row.dirty = true; // force a reparse on next draw

        // ReparseAndEvaluate (triggered by dirty above) keeps any existing
        // varValues entry whose name matches a variable the fresh parse
        // actually finds, so populating this now is enough to carry saved
        // values through -- no special-case needed on the reparse side.
        if (obj.contains("variables") && obj["variables"].is_object()) {
            for (const auto& [name, value] : obj["variables"].items()) {
                if (value.is_number()) row.varValues[name] = value.get<double>();
            }
        }
        loaded.push_back(std::move(row));
    }
    if (loaded.empty()) loaded.push_back(Row{1});
    rows_ = std::move(loaded);
    nextId_ = id;
}

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

    // Styled to blend into the background until hovered/focused, so it
    // reads as a title rather than an obvious text box -- but it's a real
    // InputText, so it's editable. "Equation N" shows as a greyed-out
    // hint (via InputTextWithHint) whenever row.title is empty, rather
    // than being baked permanently into the row.
    char titleBuf[128];
    std::snprintf(titleBuf, sizeof(titleBuf), "%s", row.title.c_str());
    std::string hint = "Equation " + std::to_string(row.id);

    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(1, 1, 1, 0.08f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(1, 1, 1, 0.12f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.70f, 1.00f, 1.0f));
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextWithHint("##title", hint.c_str(), titleBuf, sizeof(titleBuf))) {
        row.title = titleBuf;
    }
    ImGui::PopStyleColor(4);

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

    const float resultWidth = 170.0f * uiScale_;
    const float previewFontSize = ImGui::GetFontSize() * 1.8f * uiScale_; // bumped up — was too small for stacked exponents
    const float resultFontSize = ImGui::GetFontSize() * 2.0f * uiScale_;
    const float previewPadding = 16.0f * uiScale_;   // vertical breathing room inside the preview box
    const float minPreviewHeight = 55.0f * uiScale_;
    const float variablesHeight = 60.0f * uiScale_;
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
            ImGui::SetCursorPos(ImVec2(std::max(4.0f * uiScale_, (avail.x - size.x) * 0.5f),
                                        std::max(4.0f * uiScale_, (avail.y - size.y) * 0.5f)));
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
        ImGui::SetCursorPos(ImVec2(std::max(4.0f * uiScale_, (avail.x - size.x) * 0.5f), ImGui::GetCursorPosY() + 12.0f * uiScale_));
        DrawEquation(resultNode, resultFontSize);
    }
    ImGui::EndChild();

    ImGui::PopID();
}

void App::DrawMenuBar() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New")) NewFile();
            if (ImGui::MenuItem("Open")) OpenEquationsFromFile();
            if (ImGui::MenuItem("Save")) SaveEquationsToFile();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Settings")) {
            if (ImGui::MenuItem("Preferences")) showPreferences_ = true;
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
}

void App::DrawPreferencesWindow() {
    if (!showPreferences_) return;
    ImGui::SetNextWindowSize(ImVec2(340.0f * uiScale_, 0), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Preferences", &showPreferences_)) {
        ImGui::TextDisabled("Display");
        ImGui::Separator();
        float scale = uiScale_;
        if (ImGui::SliderFloat("UI Scale", &scale, 0.5f, 3.0f, "%.2fx")) {
            SetUiScale(scale);
        }
        ImGui::TextWrapped("Applies immediately and is remembered next time you open atnmCalc.");
    }
    ImGui::End();
}

void App::Draw() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::Begin("atnmCalc", nullptr,
                  ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_MenuBar);

    DrawMenuBar();

    for (auto& row : rows_) DrawRow(row);

    ImGui::Separator();
    if (ImGui::Button("+ Add row")) {
        rows_.push_back(Row{nextId_++});
    }

    ImGui::End();

    DrawPreferencesWindow();
}