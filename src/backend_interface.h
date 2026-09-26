#pragma once
// backend_interface.h
//
// Two functions the GUI calls into your backend. You implement both
// (see mock_backend.cpp for a throwaway placeholder implementation
// that lets the GUI run before your real parser exists — delete that
// file and implement these for real).
//
// Keeping "parse" and "evaluate" as separate functions (rather than one
// "parseAndEvaluate") is deliberate: the GUI needs the AST on its own,
// before evaluation, so it can render the equation as fraction bars /
// radicals / superscripts. Evaluation happens second, once the user has
// filled in values for any variables.

#include <map>
#include <string>
#include <vector>
#include "ast.h"

struct ParseResult {
    bool success = false;
    ASTNodePtr root;                    // valid AST if success == true, else nullptr
    std::string errorMessage;           // human-readable, shown directly in the GUI
    std::vector<std::string> variables; // distinct variable names found, e.g. {"x", "y"}
};

struct EvalResult {
    bool success = false;
    double value = 0.0;
    std::string errorMessage; // e.g. "division by zero", "unbound variable z"
};

// Tokenize + parse a LaTeX math string into an AST.
// Called every time the user edits the equation text box.
ParseResult parseLatex(const std::string& latex);

// Walk an already-parsed AST and compute its value, given bindings for
// every variable the AST references. `variables` only needs to contain
// entries for names that actually appear in `root` (see ParseResult::variables).
EvalResult evaluateAst(const ASTNodePtr& root, const std::map<std::string, double>& variables);
