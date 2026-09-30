//
// Created by iadna on 29/09/2026.
//

#ifndef ATNMCALC_PARSER_H
#define ATNMCALC_PARSER_H
#include <stack>
#include <vector>

#include "ast.h"
#include "lexer.h"

enum class OperatorType {
    PLUS,
    MINUS,
    MULTIPLY,
    DIVIDE,
    EXPONENT,
    UNARY_MINUS,
    IMPLIED_MULTIPLY,
};

class Parser {
    std::vector<Token> tokens;
    std::size_t pos = 0;

    //give the current token
    [[nodiscard]] const Token& peek() const;
    //give the current token and iterate pos
    Token advance();
    //this is to be used when the code expects a certain token. if its not there, then throw an error. this also consumes
    //the token that is to be expected
    void expect(TokenType type);

    [[nodiscard]] int precedence(OperatorType op) const;

    static bool isRightAssociative(OperatorType op);

    ASTNodePtr applyTopOperator(std::stack<OperatorType>& operators, std::stack<ASTNodePtr>& operands);

    [[nodiscard]] bool shouldPopBefore(OperatorType top, OperatorType incoming) const;

    static OperatorType tokenTypeToOperatorType(TokenType token);

    ASTNodePtr parseExpression();

public:
    explicit Parser(std::vector<Token> tokens) : tokens(std::move(tokens)) {};

    ASTNodePtr parse();
};


#endif //ATNMCALC_PARSER_H