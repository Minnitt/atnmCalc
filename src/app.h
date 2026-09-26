#pragma once
// app.h
//
// Owns the list of calculation "rows" (each an independent equation,
// per your wireframe) and draws the whole window every frame.

#include <map>
#include <string>
#include <vector>
#include "backend_interface.h"

struct Row {
    int id;
    std::string input;                    // raw text typed into "Enter equation:"
    bool dirty = true;                    // true when input changed since last (re)parse
    ParseResult parseResult;              // result of parseLatex(input)
    std::map<std::string, double> varValues; // current value typed for each variable
    EvalResult evalResult;                // result of evaluateAst(...), recomputed on change
};

class App {
public:
    void Draw(); // call once per frame, inside your ImGui NewFrame()/Render() loop

private:
    std::vector<Row> rows_{Row{1}};
    int nextId_ = 2;

    void DrawRow(Row& row);
    void ReparseAndEvaluate(Row& row);
};
