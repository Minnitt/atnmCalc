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
#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>
#include <tinyfiledialogs.h>

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
    currentFilePath_.clear(); // "New" starts an untitled session, same as most editors
}

void App::SaveFile() {
    if (currentFilePath_.empty()) {
        SaveAs(); // no path chosen yet this session -- behave like Save As
        return;
    }
    SaveEquationsToFile(currentFilePath_);
}

void App::SaveAs() {
    const char* filterPatterns[1] = {"*.json"};
    const char* chosen = tinyfd_saveFileDialog(
        "Save equations",
        currentFilePath_.empty() ? "equations.json" : currentFilePath_.c_str(),
        1, filterPatterns, "JSON files");
    if (!chosen) return; // user cancelled the dialog
    currentFilePath_ = chosen;
    SaveEquationsToFile(currentFilePath_);
}

void App::OpenFile() {
    const char* filterPatterns[1] = {"*.json"};
    const char* chosen = tinyfd_openFileDialog(
        "Open equations",
        "",
        1, filterPatterns, "JSON files",
        0); // no multi-select
    if (!chosen) return; // user cancelled the dialog
    OpenEquationsFromFile(chosen);
    currentFilePath_ = chosen;
}

void App::SaveEquationsToFile(const std::string& path) const {
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
    std::ofstream out(path);
    if (!out) return;
    out << arr.dump(2); // pretty-printed, 2-space indent -- readable if you open it by hand
}

void App::OpenEquationsFromFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) return; // shouldn't normally happen -- the dialog only returns existing files

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

bool App::Splitter(const char* id, bool vertical, float length, float thickness,
                   float* dragTotal, float idleLineOffset) {
    ImGui::InvisibleButton(id, vertical ? ImVec2(thickness, length) : ImVec2(length, thickness));

    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    const ImVec2 a = ImGui::GetItemRectMin();
    const ImVec2 b = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    if (hovered || active) {
        ImGui::SetMouseCursor(vertical ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS);
        dl->AddRectFilled(a, b, ImGui::GetColorU32(active ? ImGuiCol_SeparatorActive
                                                           : ImGuiCol_SeparatorHovered));
    } else if (idleLineOffset >= 0.0f) {
        // A thin line at an explicit offset from the handle's start edge
        // when idle -- used for the row-bottom handle so it doubles as the
        // visual divider between equation rows, rather than being fully
        // invisible until moused over like the pane splitters inside a row.
        const ImU32 col = ImGui::GetColorU32(ImGuiCol_Separator);
        if (vertical) {
            float x = a.x + idleLineOffset;
            dl->AddLine(ImVec2(x, a.y), ImVec2(x, b.y), col);
        } else {
            float y = a.y + idleLineOffset;
            dl->AddLine(ImVec2(a.x, y), ImVec2(b.x, y), col);
        }
    }

    // The draw-list calls above don't register as ImGui items (AddRectFilled/
    // AddLine bypass the item system entirely), so the "last item" the
    // caller's subsequent IsItemActivated()/IsItemHovered() will see is
    // still this InvisibleButton -- exactly as needed.
    //
    // IsMouseDragging only turns true once the mouse has moved past ImGui's
    // small drag threshold, so a plain click (or a double-click, used below
    // by callers to reset to auto-fit) never counts as a resize.
    if (!active || !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) return false;
    ImVec2 d = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left); // total since the drag began, not per-frame
    *dragTotal = vertical ? d.x : d.y;
    return true;
}

void App::DrawRow(Row& row) {
    ImGui::PushID(row.id);
    // (No leading Separator here any more -- each row's bottom splitter
    // draws the dividing line before the next row. The very first row still
    // gets one, drawn once in Draw() before the row loop starts.)

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

    const float s = uiScale_;
    // THE gap size. This one number is the gap between the preview and
    // variables boxes, between the left column and Result, and (below) on
    // each side of the row divider line -- change it here and every gap
    // follows. Splitter() no longer has its own copy to keep in sync.
    const float splitterThickness = 6.0f * s;

    // The row divider is a handle whose idle line sits dividerLineY below
    // the row's boxes -- the same as the gap between boxes -- with the same
    // amount of visible space again between that line and the NEXT row's
    // title text.
    //
    // The title field already has FramePadding.y of empty space inside its
    // own frame above its text, and the font itself leaves a couple more
    // pixels of empty space above the first row of glyphs (measured ~2px for
    // ImGui's default font at scale 1) -- so the handle only needs to
    // supply what's left of the gap after those.
    const float dividerLineY = splitterThickness;
    const float dividerBelowLine = std::max(0.0f,
        splitterThickness - ImGui::GetStyle().FramePadding.y - 2.0f * s);
    const float dividerThickness = dividerLineY + 1.0f /*the line itself*/ + dividerBelowLine;
    const float minPaneHeight = 30.0f * s;
    const float previewFontSize = ImGui::GetFontSize() * 1.8f * s; // bumped up — was too small for stacked exponents
    const float resultFontSize = ImGui::GetFontSize() * 2.0f * s;
    const float previewPadding = 16.0f * s;   // vertical breathing room inside the preview box
    const float minPreviewHeight = 55.0f * s;
    const float rowAvailWidth = ImGui::GetContentRegionAvail().x;

    const float minResultWidth = 80.0f * s;
    const float maxResultWidth = std::max(minResultWidth, rowAvailWidth - 100.0f * s - splitterThickness);
    // This row's own width (default if it hasn't been dragged), clamped to
    // what the window currently allows -- so a width dragged out wide, then
    // a window shrunk afterwards, can't push the Result box past the edge.
    const float resultWidthPx = std::clamp(
        row.resultWidth < 0.0f ? 170.0f * s : row.resultWidth * s, minResultWidth, maxResultWidth);

    // Exact: the column splitter uses SameLine(0, 0) on both sides (see
    // below), so the row is precisely leftWidth + splitterThickness +
    // resultWidthPx wide, with no hidden ItemSpacing fudge-factor to
    // get slightly wrong. The old version subtracted a flat 8px guess here
    // while the real layout consumed splitterThickness + 2*ItemSpacing.x --
    // a mismatch that pushed the Result box a few pixels past the window
    // edge, clipping its right border.
    const float leftWidth = std::max(100.0f * s, rowAvailWidth - resultWidthPx - splitterThickness);

    // --- Auto-fit heights (used whenever the user hasn't dragged a splitter) ---
    float autoPreviewHeight = minPreviewHeight;
    if (!row.input.empty() && row.parseResult.success) {
        ImVec2 eqSize = MeasureEquation(row.parseResult.root, previewFontSize);
        autoPreviewHeight = std::max(minPreviewHeight, eqSize.y + previewPadding);
    }

    // The variables panel wraps to multiple lines rather than running off
    // the right edge (see the wrapping loop below), so its auto-fit height
    // needs to account for however many lines that wrapping actually
    // produces. The per-slot width here is measured from the ACTUAL
    // style/font metrics (not a guessed constant) specifically so this
    // estimate matches the real wrap loop's decisions as closely as
    // possible -- variable names are always exactly one character, so a
    // single measured sample is representative of every slot. Even so, the
    // "vars" child below is intentionally left scrollable (no NoScrollbar
    // flag) as a safety net: if this estimate is ever slightly short,
    // content stays reachable via a scrollbar rather than being clipped.
    const float varItemWidth = 100.0f * s;
    const float varItemSpacing = ImGui::GetStyle().ItemSpacing.x;
    const float varLabelWidth = ImGui::CalcTextSize("W").x; // representative: all variable labels are one char
    const float estimatedVarSlotWidth =
        varItemWidth + ImGui::GetStyle().ItemInnerSpacing.x + varLabelWidth + varItemSpacing;
    const float varLineHeight = ImGui::GetFrameHeightWithSpacing();
    const float variablesHeaderHeight = ImGui::GetTextLineHeightWithSpacing();
    const float variablesPadding = 10.0f * s;

    float autoVarsHeight = variablesHeaderHeight + varLineHeight + variablesPadding; // header + at least one line
    if (row.parseResult.success && !row.parseResult.variables.empty()) {
        int slotsPerRow = std::max(1, static_cast<int>(leftWidth / estimatedVarSlotWidth));
        int rowCount = static_cast<int>(std::ceil(
            static_cast<double>(row.parseResult.variables.size()) / slotsPerRow));
        autoVarsHeight = variablesHeaderHeight + rowCount * varLineHeight + variablesPadding;
    }

    // --- Effective heights: the user's dragged value if they've set one, else auto-fit ---
    const float previewHeight = row.previewHeight < 0.0f ? autoPreviewHeight : row.previewHeight * s;
    const float variablesHeight = row.varsHeight < 0.0f ? autoVarsHeight : row.varsHeight * s;
    const float totalHeight = previewHeight + splitterThickness + variablesHeight;

    // `drag` is written by Splitter() whenever it returns true; declared
    // once and reused across all three splitter calls below, each gated by
    // its own returned bool so there's no risk of reading a stale value
    // left over from a different splitter.
    float drag = 0.0f;

    ImGui::BeginGroup();
    {
        // HorizontalScrollbar (and no NoScrollbar override) so a row whose
        // equation renders wider than the box -- long implicit-multiplication
        // chains especially -- can be scrolled into view instead of silently
        // clipping past the right edge with no indication there's more.
        ImGui::BeginChild("preview", ImVec2(leftWidth, previewHeight), true, ImGuiWindowFlags_HorizontalScrollbar);
        if (row.input.empty()) {
            ImGui::TextDisabled("(rendered equation appears here)");
        } else if (!row.parseResult.success) {
            ImGui::TextColored(ImVec4(0.85f, 0.3f, 0.3f, 1.0f), "%s", row.parseResult.errorMessage.c_str());
        } else {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            ImVec2 size = MeasureEquation(row.parseResult.root, previewFontSize);
            ImGui::SetCursorPos(ImVec2(std::max(4.0f * s, (avail.x - size.x) * 0.5f),
                                        std::max(4.0f * s, (avail.y - size.y) * 0.5f)));
            DrawEquation(row.parseResult.root, previewFontSize);
        }
        ImGui::EndChild();

        // Pull the splitter flush against the preview box, rather than
        // leaving the default ItemSpacing gap -- the handle's own 6px
        // thickness becomes the only visible gap. Plain cursor arithmetic
        // rather than a PushStyleVar/PopStyleVar pair specifically because
        // that pair would straddle preview's own Begin/End boundary
        // asymmetrically (pushed "inside" at EndChild, popped "outside"
        // after) -- which is exactly what ImGui's debug stack-checker
        // flags as a mismatch, since it expects each window's own style
        // stack size to balance between its entry and exit.
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetStyle().ItemSpacing.y);

        // --- Splitter: between preview and variables ---
        // Moves the boundary between the two panes; their COMBINED height
        // stays fixed (one grows exactly as much as the other shrinks),
        // same as any ordinary split-pane resize.
        bool draggingPV = Splitter("##split_pv", false, leftWidth, splitterThickness, &drag);
        if (ImGui::IsItemActivated()) dragStart_ = ImVec2(previewHeight, variablesHeight);
        if (draggingPV) {
            const float combined = dragStart_.x + dragStart_.y;
            const float newPreview = std::clamp(dragStart_.x + drag, minPaneHeight, combined - minPaneHeight);
            row.previewHeight = newPreview / s;
            row.varsHeight = (combined - newPreview) / s;
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            row.previewHeight = row.varsHeight = -1.0f; // double-click resets to auto-fit
        }

        // ImGui adds ItemSpacing.y after EVERY item, including the handle
        // above, so without this the variables box starts ItemSpacing.y
        // lower than the layout math (totalHeight) assumes: the vertical gap
        // comes out bigger than the horizontal one, and the Result box ends
        // up ItemSpacing.y shorter than the left column. Pulling the cursor
        // back up here (same trick as after the preview box) makes the gap
        // exactly splitterThickness and the heights match.
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetStyle().ItemSpacing.y);

        ImGui::BeginChild("vars", ImVec2(leftWidth, variablesHeight), true);
        if (row.parseResult.success && !row.parseResult.variables.empty()) {
            ImGui::TextDisabled("Define variables:");
            // Standard ImGui "wrap to next line" pattern: after drawing each
            // input, check whether the NEXT one would run past the window's
            // visible right edge, and only call SameLine() if it wouldn't.
            float windowVisibleX2 = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
            const auto& names = row.parseResult.variables;
            for (size_t i = 0; i < names.size(); i++) {
                const std::string& name = names[i];
                ImGui::SetNextItemWidth(varItemWidth);
                float v = static_cast<float>(row.varValues[name]);
                if (ImGui::InputFloat(name.c_str(), &v)) {
                    row.varValues[name] = v;
                    row.evalResult = evaluateAst(row.parseResult.root, row.varValues);
                }
                float nextItemX2 = ImGui::GetItemRectMax().x + varItemSpacing + varItemWidth; // rough width of the next slot
                if (i + 1 < names.size() && nextItemX2 < windowVisibleX2) {
                    ImGui::SameLine();
                }
            }
        } else {
            ImGui::TextDisabled("(no variables in this equation)");
        }
        ImGui::EndChild();
    }
    ImGui::EndGroup();

    // --- Splitter: between the left column and Result ---
    // SameLine(0, 0) on both sides -- explicitly zero spacing, not the
    // default -- so the handle itself is the ENTIRE gap between panes, and
    // leftWidth's arithmetic above matches reality exactly.
    ImGui::SameLine(0.0f, 0.0f);
    bool draggingCol = Splitter("##split_col", true, totalHeight, splitterThickness, &drag);
    if (ImGui::IsItemActivated()) dragStart_.x = resultWidthPx;
    if (draggingCol) {
        // Result sits to the RIGHT of this handle, so dragging left widens it.
        const float newWidth = std::clamp(dragStart_.x - drag, minResultWidth, maxResultWidth);
        row.resultWidth = newWidth / s;
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        row.resultWidth = -1.0f; // double-click resets this row to the default width
    }
    ImGui::SameLine(0.0f, 0.0f);

    // Rendered solution — height matches the group above so the two sides
    // of the row line up regardless of how tall the equation preview got.
    ImGui::BeginChild("result", ImVec2(resultWidthPx, totalHeight), true);
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
        // Center within the space actually remaining below the header.
        ImGui::SetCursorPos(ImVec2(std::max(4.0f * s, (avail.x - size.x) * 0.5f),
                                    ImGui::GetCursorPosY() + std::max(4.0f * s, (avail.y - size.y) * 0.5f)));
        DrawEquation(resultNode, resultFontSize);
    }
    ImGui::EndChild();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetStyle().ItemSpacing.y); // flush against the bottom splitter, same reasoning as above

    // --- Splitter: bottom of the row ---
    // Resizes the variables pane (the bottom-most pane in the row), so the
    // whole row gets taller or shorter. Unlike the other two splitters this
    // one also serves as the permanent visual divider between this row and
    // the next (replacing the old per-row leading Separator()), so it draws
    // an idle line -- dividerLineY below the row's boxes, with matching
    // space underneath it (see the constants at the top of DrawRow).
    bool draggingBottom = Splitter("##split_bottom", false, rowAvailWidth, dividerThickness, &drag, dividerLineY);
    if (ImGui::IsItemActivated()) dragStart_.x = variablesHeight;
    if (draggingBottom) {
        row.varsHeight = std::max(minPaneHeight, dragStart_.x + drag) / s;
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        row.previewHeight = row.varsHeight = -1.0f; // double-click resets to auto-fit
    }

    // Cancel the ItemSpacing.y ImGui adds after the handle, so the next
    // row's title starts exactly where the divider geometry above puts it
    // (same technique as after the other two splitters).
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetStyle().ItemSpacing.y);

    ImGui::PopID();
}

void App::DrawMenuBar() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New")) NewFile();
            if (ImGui::MenuItem("Open...")) OpenFile();
            if (ImGui::MenuItem("Save")) SaveFile();
            if (ImGui::MenuItem("Save As...")) SaveAs();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Settings")) {
            if (ImGui::MenuItem("Preferences")) {
                showPreferences_ = true;
                ImGui::SetWindowFocus("Preferences");
            }
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
        // Only pull from the committed value when the user isn't dragging --
        // otherwise this would clobber the in-progress drag every frame.
        // On the release frame itself, scaleSliderActive_ still holds last
        // frame's "true" (the widget was active a moment ago), so this sync
        // is correctly skipped right when it matters most.
        if (!scaleSliderActive_) pendingScale_ = uiScale_;

        // Both queries below MUST happen immediately after the slider,
        // before any other widget is drawn -- they report on whatever the
        // single most recently drawn item was, so a widget drawn in
        // between (even a plain Text call) silently becomes the "last
        // item" instead, and these would end up reporting on that.
        ImGui::SliderFloat("UI Scale", &pendingScale_, 0.5f, 3.0f, "%.2fx");
        scaleSliderActive_ = ImGui::IsItemActive();
        bool releasedAfterChange = ImGui::IsItemDeactivatedAfterEdit();

        if (scaleSliderActive_) {
            ImGui::TextDisabled("(release to apply)");
        }
        if (releasedAfterChange) {
            SetUiScale(pendingScale_);
        }
        ImGui::TextWrapped("Applies immediately and is remembered next time you open atnmCalc.");
    }
    ImGui::End();
}

void App::Draw() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    // NoBringToFrontOnFocus: this window covers the ENTIRE work area every
    // frame with no gaps, so without this flag, clicking anywhere in it
    // (completely normal interaction) pulls it to the front of the
    // z-order and buries any floating window above it -- like Preferences
    // -- with no exposed pixels left to click back onto. This is the
    // standard flag for a fullscreen "host" window that shouldn't compete
    // for z-order with the smaller utility windows floating above it.
    ImGui::Begin("atnmCalc", nullptr,
                  ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                  ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoBringToFrontOnFocus);

    DrawMenuBar();
    ImGui::Separator(); // top divider -- each row's own bottom splitter handles dividers after that

    for (auto& row : rows_) DrawRow(row);

    // No Separator() here: the last row's own bottom divider already draws
    // the line above this button (a second one made a visible double line).
    if (ImGui::Button("+ Add row")) {
        rows_.push_back(Row{nextId_++});
    }

    ImGui::End();

    DrawPreferencesWindow();
}