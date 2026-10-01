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

    // This row's own dragged layout, stored UNSCALED (divided by uiScale_
    // when set, multiplied back when used) so it follows UI scale changes.
    // Negative means "auto-fit / default" -- a new row starts that way, and
    // double-clicking one of this row's splitters resets it to that. Each
    // row owns its own values, so resizing one row never affects another.
    float previewHeight = -1.0f; // equation preview pane height
    float varsHeight = -1.0f;    // variables pane height
    float resultWidth = -1.0f;   // Result column width
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
    // columns); false is an up/down handle (between stacked panes). `length`
    // is the handle's extent along the divider, `thickness` its size across
    // it (the caller owns that number, so the layout math and the drawn
    // handle can never disagree). Returns true while being dragged, writing
    // the TOTAL mouse movement since the drag began into *dragTotal (not
    // per-frame delta -- that drifts once a caller clamps the resulting
    // size, since the mouse keeps moving further than the clamped value
    // allows). idleLineOffset >= 0 draws a thin divider line that far from
    // the handle's start edge while it isn't hovered/active, for handles
    // that double as a permanent visual divider (the row-bottom one);
    // negative (the default) keeps the handle invisible until moused over.
    bool Splitter(const char* id, bool vertical, float length, float thickness,
                  float* dragTotal, float idleLineOffset = -1.0f);

    // The pane size(s) being resized at the moment a drag started. Only one
    // splitter can be dragged at a time (ImGui has one ActiveId), so a
    // single shared slot is enough for all of them.
    ImVec2 dragStart_{};

    void SetUiScale(float scale); // clamps, applies to ImGui style/fonts, and persists

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