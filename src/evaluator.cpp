//
// Created by atnm on 30/09/2026.
//

#include "evaluator.h"

#include <cmath>

EvalResult Evaluator::evaluateAST(ASTNodePtr root, const std::map<std::string, double>& variables) {

    double result = 0;

    try {
        result = evaluateNode(root, variables);
    }
    catch (std::exception& e) {
        return EvalResult({false, 0.0, e.what()});
    }
    return {true, result, ""};
}

double Evaluator::evaluateNode(ASTNodePtr node, const std::map<std::string, double>& variables) {
    switch (node->type()) {
        case NodeType::Number: return dynamic_cast<NumberNode *>(node.get())->value;
        case NodeType::Variable: {
            auto* variableNode = dynamic_cast<VariableNode*>(node.get());
            std::string name = variableNode->name;
            auto it = variables.find(name);
            if (it == variables.end()) {
                throw std::runtime_error("Variable " + name + " does not exist");
            }
            double value = it->second;
            return value;
        }
        case NodeType::BinaryOp: {
            auto* binaryOpNode = dynamic_cast<BinaryOpNode*>(node.get());
            double leftResult = evaluateNode(binaryOpNode->left, variables);
            auto rightResult = evaluateNode(binaryOpNode->right, variables);
            switch (binaryOpNode->op) {
                case BinOp::Add: {
                    double result = leftResult + rightResult;
                    return result;
                }
                case BinOp::Sub: {
                    double result = leftResult - rightResult;
                    return result;
                }
                case BinOp::Mul: {
                    double result = leftResult * rightResult;
                    return result;
                }
                case BinOp::Div: {
                    if (rightResult == 0) {
                        throw std::runtime_error("Division by zero");
                    }
                    double result = leftResult / rightResult;
                    return result;
                }
                case BinOp::Pow: {

                    double result = std::pow(leftResult, rightResult);
                    return result;
                }
            }
        }
        case NodeType::UnaryMinus: {
            auto* operandNode = dynamic_cast<UnaryMinusNode*>(node.get());
            double result = evaluateNode(operandNode->operand, variables);
            return -result;
        }
        case NodeType::Frac: {
            auto* binaryOpNode = dynamic_cast<FracNode*>(node.get());
            double numeratorResult = evaluateNode(binaryOpNode->numerator, variables);
            double denominatorResult = evaluateNode(binaryOpNode->denominator, variables);
            if (denominatorResult == 0) {
                throw std::runtime_error("Division by zero");
            }

            double result = numeratorResult / denominatorResult;
            return result;
        }

        case NodeType::Sqrt: {
            auto* radicandNode = dynamic_cast<SqrtNode*>(node.get());
            double radicandResult = evaluateNode(radicandNode->radicand, variables);
            if (radicandResult < 0) {
                throw std::runtime_error("Square root of negative numbers is not supported");
            }

            double result = std::sqrt(radicandResult);
            return result;
        }
        default:
            throw std::runtime_error("Unknown node type");
    }
}
