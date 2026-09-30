//
// Created by atnm on 30/09/2026.
//

#ifndef ATNMCALC_EVALUATOR_H
#define ATNMCALC_EVALUATOR_H
#include <set>

#include "backend_interface.h"


class Evaluator {
public:
    static EvalResult evaluateAST(ASTNodePtr root, const std::map<std::string, double>& variables);

    static double evaluateNode(ASTNodePtr node, const std::map<std::string, double>& variables);
};


#endif //ATNMCALC_EVALUATOR_H