#pragma once
// app.h
//
// Owns the list of calculation "rows" (each an independent equation,
// per your wireframe) and draws the whole window every frame.

#include <map>
#include <string>
#include <vector>
#include "backend_interface.h"
#include "imgui.h"

struct Row {
    int id;
    std::string title;                    // user-editable; "Equation N" shown as a hint when empty
    std::string input;                    // raw text typed into "Enter equation:"
    bool dirty = true;                    // true when input changed since last (re)parse
    ParseResult parseResult;              // result of parseLatex(input)
    std::map<std::string, double> varValues; // current value typed for each variable
    EvalResult evalResult;                // result of evaluateAst(...), recomputed on change
};

class App {
public:
    App();       // captures the base ImGui style and loads any saved preferences
    void Draw(); // call once per frame, inside your ImGui NewFrame()/Render() loop

    // Called once from main.cpp right after the window/ImGui context exist,
    // with the monitor's content scale (e.g. from glfwGetWindowContentScale).
    // Only takes effect the very first time atnmCalc is run on a machine --
    // once a scale has been saved, this never overrides the user's choice.
    void ApplySuggestedScaleIfUnset(float suggestedScale);

private:
    std::vector<Row> rows_{Row{1}};
    int nextId_ = 2;

    float uiScale_ = 1.0f;
    // The slider's bound value needs to persist across frames (not be a
    // fresh local re-read from uiScale_ every frame) -- otherwise the
    // value the slider last computed from the mouse, on the final frame
    // before release, gets discarded before IsItemDeactivatedAfterEdit()
    // ever gets a chance to read it. See DrawPreferencesWindow().
    float pendingScale_ = 1.0f;
    bool scaleSliderActive_ = false;
    bool hasSavedScale_ = false;
    bool showPreferences_ = false;
    ImGuiStyle baseStyle_{}; // captured once in the constructor, before any scaling is applied

    void DrawRow(Row& row);
    void ReparseAndEvaluate(Row& row);
    void DrawMenuBar();
    void DrawPreferencesWindow();

    // Generic drag handle. vertical=true is a left/right handle (between
    // columns); false is an up/down handle (between stacked panes). Returns
    // true while being dragged, writing the TOTAL mouse movement since the
    // drag began into *dragTotal (not per-frame delta -- that drifts once a
    // caller clamps the resulting size, since the mouse keeps moving further
    // than the clamped value allows). idleLine draws a thin divider line
    // when the handle isn't hovered/active, for handles that double as a
    // permanent visual divider (the row-bottom one) rather than staying
    // fully invisible until moused over.
    bool Splitter(const char* id, bool vertical, float length, bool idleLine, float* dragTotal);

    // The pane size(s) being resized at the moment a drag started. Only one
    // splitter can be dragged at a time (ImGui has one ActiveId), so a
    // single shared slot is enough for all of them.
    ImVec2 dragStart_{};

    void SetUiScale(float scale); // clamps, applies to ImGui style/fonts, and persists

    // User-draggable width of the Result column, shared across every row
    // (one shared value, not per-row -- dragging any row's splitter resizes
    // all of them together, the same way a file browser's sidebar width
    // applies everywhere rather than per-folder). Initialized to a sensible
    // default once uiScale_ is known, in the constructor.
    float resultColumnWidth_ = 170.0f;

    // Same sharing philosophy, applied to the other two splitters: dragging
    // the preview/variables boundary or a row's bottom edge on ANY row
    // affects every row identically, so rows can't end up looking
    // inconsistent with each other (an empty row elsewhere staying small
    // while one you happened to drag stays big). Negative means "auto-fit
    // to content"; dragging either splitter switches to an explicit height.
    float sharedPreviewHeight_ = -1.0f;
    float sharedVarsHeight_ = -1.0f;

    void LoadSettings();
    void SaveSettings() const;

    void NewFile();                     // File > New
    void SaveFile();                    // File > Save (prompts via SaveAs if no path yet)
    void SaveAs();                      // File > Save As...
    void OpenFile();                    // File > Open...

    void SaveEquationsToFile(const std::string& path) const;
    void OpenEquationsFromFile(const std::string& path);

    std::string currentFilePath_; // empty until Save/Save As/Open has been used this session

    static std::string SettingsFilePath();
};