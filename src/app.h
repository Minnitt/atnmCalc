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
    bool hasSavedScale_ = false;
    bool showPreferences_ = false;
    ImGuiStyle baseStyle_{}; // captured once in the constructor, before any scaling is applied

    void DrawRow(Row& row);
    void ReparseAndEvaluate(Row& row);
    void DrawMenuBar();
    void DrawPreferencesWindow();

    void SetUiScale(float scale); // clamps, applies to ImGui style/fonts, and persists

    void LoadSettings();
    void SaveSettings() const;

    void NewFile();                    // File > New
    void SaveEquationsToFile() const;  // File > Save
    void OpenEquationsFromFile();      // File > Open

    static std::string SettingsFilePath();
    static std::string EquationsFilePath();
};