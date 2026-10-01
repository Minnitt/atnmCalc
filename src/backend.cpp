//
// Created by atnm on 30/09/2026.
//

#include "backend_interface.h"
#include "evaluator.h"
#include "lexer.h"
#include "parser.h"

ParseResult parseLatex(const std::string& latex) {
    ParseResult result;
    try {
        Parser parser = Parser(Lexer::tokenise(latex));
        ASTNodePtr root = parser.parse();
        std::set<std::string> variables = parser.getVariableNames();
        result.success = true;
        result.root = root;
        result.variables.assign(variables.begin(), variables.end());

    } catch (std::exception& e) {
        result.success = false;
        result.errorMessage = e.what();
    }
    return result;
}

EvalResult evaluateAst(const ASTNodePtr& root, const std::map<std::string, double>& variables) {
    return Evaluator::evaluateAST(root, variables);
}