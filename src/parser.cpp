//
// Created by iadna on 29/09/2026.
//

#include "parser.h"

#include <set>
#include <stack>
#include <stdexcept>

ASTNodePtr Parser::parse() {
    ASTNodePtr result = parseExpression();
    expect(TokenType::END);
    return result;
}

ASTNodePtr Parser::parseExpression() {
    std::stack<ASTNodePtr> operands{};
    std::stack<OperatorType> operators{};
    bool expectOperand = true;

    std::set expressionEndingTokens({TokenType::END, TokenType::RBRACE, TokenType::RPAREN});

    while (!expressionEndingTokens.contains(peek().type)) {
        if (expectOperand) {
            switch (peek().type) {
                case TokenType::NUMBER: {
                    Token token = advance();
                    operands.push(std::make_shared<NumberNode>(token.value));
                    expectOperand = false;
                    break;
                }
                case TokenType::VARIABLE: {
                    Token token = advance();
                    operands.push(std::make_shared<VariableNode>(std::string(1, token.name)));
                    variableNames.insert(std::string(1, token.name));
                    expectOperand = false;
                    break;
                }
                case TokenType::LPAREN: {
                    advance();
                    ASTNodePtr inner = parseExpression();
                    expect(TokenType::RPAREN);
                    operands.push(inner);
                    expectOperand = false;
                    break;
                }
                case TokenType::FRAC: {
                    advance();
                    expect(TokenType::LBRACE);
                    ASTNodePtr numerator = parseExpression();
                    expect(TokenType::RBRACE);
                    expect(TokenType::LBRACE);
                    ASTNodePtr denominator = parseExpression();
                    expect(TokenType::RBRACE);
                    operands.push(std::make_shared<FracNode>(numerator, denominator));
                    expectOperand = false;
                    break;
                }
                case TokenType::SQRT: {
                    advance();
                    expect(TokenType::LBRACE);
                    ASTNodePtr inner = parseExpression();
                    expect(TokenType::RBRACE);
                    operands.push(std::make_shared<SqrtNode>(inner));
                    expectOperand = false;
                    break;
                }
                case TokenType::MINUS: { //unary minus operator
                    advance();
                    operators.push(OperatorType::UNARY_MINUS);
                    break;
                }
                default:
                    throw std::invalid_argument("Expected an operand");
            }

        }
        else { //expecting an operator
            switch (peek().type) {
                case TokenType::PLUS:
                case TokenType::MINUS:
                case TokenType::ASTERISK:
                case TokenType::SLASH:
                case TokenType::CARET: { //operators
                    OperatorType incoming = tokenTypeToOperatorType(peek().type);
                    while (!operators.empty() && shouldPopBefore(operators.top(), incoming)) {
                        ASTNodePtr result = applyTopOperator(operators, operands);
                        operands.push(result);
                    }
                    advance();
                    operators.push(incoming);
                    expectOperand = true;
                    break;
                }
                case TokenType::VARIABLE:
                case TokenType::LPAREN:
                case TokenType::FRAC:
                case TokenType::SQRT: { //implied multiplication
                    while (!operators.empty() && shouldPopBefore(operators.top(), OperatorType::IMPLIED_MULTIPLY)) {
                        ASTNodePtr result = applyTopOperator(operators, operands);
                        operands.push(result);
                    }
                    operators.push(OperatorType::IMPLIED_MULTIPLY);
                    expectOperand = true;
                    break;
                }
                default:
                    throw std::invalid_argument("Expected an operator at position " + std::to_string(peek().index));
            }
        }
    }
    while (!operators.empty()) {
        ASTNodePtr result = applyTopOperator(operators, operands);
        operands.push(result);
    }

    if (operands.size() != 1) {
        throw std::logic_error("parseExpression ended with " + std::to_string(operands.size()) + " operands on the stack.");
    }

    return operands.top();
}

const Token &Parser::peek() const {
    return tokens[pos];
}

Token Parser::advance() {
    Token token = tokens[pos];
    pos++;
    return token;
}

void Parser::expect(TokenType type) {
    if (peek().type != type) {
        throw std::invalid_argument("expected a different token at position " + std::to_string(peek().index));
    }
    advance();
}

int Parser::precedence(OperatorType op) const {
    switch (op) {
        case OperatorType::PLUS:
        case OperatorType::MINUS:
            return 1;
        case OperatorType::MULTIPLY:
        case OperatorType::DIVIDE:
        case OperatorType::IMPLIED_MULTIPLY:
        case OperatorType::UNARY_MINUS:
            return 2;
        case OperatorType::EXPONENT:
            return 3;
        default:
            throw std::logic_error("unhandled operator in precedence()");
    }
}

bool Parser::isRightAssociative(const OperatorType op) {
    return op == OperatorType::EXPONENT;
}

ASTNodePtr Parser::applyTopOperator(std::stack<OperatorType>& operators, std::stack<ASTNodePtr>& operands) {
    OperatorType op = operators.top();
    operators.pop();

    ASTNodePtr result = nullptr;

    if (op == OperatorType::UNARY_MINUS) {
        if (operands.empty()) {
            throw std::invalid_argument("incomplete expression: missing operand for unary '-'");
        }
        ASTNodePtr operand = operands.top(); operands.pop();
        return std::make_shared<UnaryMinusNode>(operand);
    }

    // every other OperatorType needs two operands
    if (operands.size() < 2) {
        throw std::invalid_argument("incomplete expression: missing operand for operator");
    }

    switch (op) {
        case OperatorType::PLUS: {
            ASTNodePtr right = operands.top(); operands.pop();
            ASTNodePtr left = operands.top(); operands.pop();

            result = std::make_shared<BinaryOpNode>(BinOp::Add, left, right);
            break;
        }
        case OperatorType::MINUS: {
            ASTNodePtr right = operands.top(); operands.pop();
            ASTNodePtr left = operands.top(); operands.pop();

            result = std::make_shared<BinaryOpNode>(BinOp::Sub, left, right);
            break;
        }
        case OperatorType::MULTIPLY: {
            ASTNodePtr right = operands.top(); operands.pop();
            ASTNodePtr left = operands.top(); operands.pop();

            result = std::make_shared<BinaryOpNode>(BinOp::Mul, left, right);
            break;
        }
        case OperatorType::DIVIDE: {
            ASTNodePtr right = operands.top(); operands.pop();
            ASTNodePtr left = operands.top(); operands.pop();

            result = std::make_shared<BinaryOpNode>(BinOp::Div, left, right);
            break;
        }
        case OperatorType::IMPLIED_MULTIPLY: {
            ASTNodePtr right = operands.top(); operands.pop();
            ASTNodePtr left = operands.top(); operands.pop();

            result = std::make_shared<BinaryOpNode>(BinOp::Mul, left, right);
            break;
        }
        case OperatorType::EXPONENT: {
            ASTNodePtr right = operands.top(); operands.pop();
            ASTNodePtr left = operands.top(); operands.pop();

            result = std::make_shared<BinaryOpNode>(BinOp::Pow, left, right);
            break;
        }
    }
    return result;
}

bool Parser::shouldPopBefore(const OperatorType top, const OperatorType incoming) const {
    const int topPrecedence = precedence(top);
    const int incomingPrecedence = precedence(incoming);
    if (topPrecedence > incomingPrecedence) {
        return true;
    }
    if (topPrecedence == incomingPrecedence && !isRightAssociative(incoming)) {
        return true;
    }
    return false;
}

OperatorType Parser::tokenTypeToOperatorType(const TokenType token) {
    switch (token) {
        case TokenType::PLUS:
            return OperatorType::PLUS;
        case TokenType::MINUS:
            return OperatorType::MINUS;
        case TokenType::ASTERISK:
            return OperatorType::MULTIPLY;
        case TokenType::SLASH:
            return OperatorType::DIVIDE;
        case TokenType::CARET:
            return OperatorType::EXPONENT;
        default:
            throw std::logic_error("token is not an operator");
    }
}

std::set<std::string> Parser::getVariableNames() {
    return variableNames;
}


