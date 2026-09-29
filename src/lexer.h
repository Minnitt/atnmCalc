//
// Created by iadna on 28/09/2026.
//

#ifndef ATNMCALC_LEXER_H
#define ATNMCALC_LEXER_H
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

enum class TokenType {
    NUMBER,
    VARIABLE,
    PLUS,
    MINUS,
    ASTERISK,
    SLASH,
    CARET,
    LPAREN,
    RPAREN,
    LBRACE,
    RBRACE,
    FRAC,
    SQRT,
    END,
    UNKNOWN,
};

enum class CommandType {
    FRAC,
    SQRT,
    UNKNOWN
};

static const std::unordered_map<std::string, CommandType> CommandTypeMap = {
    {"frac", CommandType::FRAC},
    {"sqrt", CommandType::SQRT},
};

struct Token {
    TokenType type = TokenType::UNKNOWN;
    std::size_t index = 0;
    double value{};
    char name{};
};

class Lexer {

    std::vector<Token> tokenise(const std::string &source);

    static CommandType getCommandType(const std::string &source);
};


#endif //ATNMCALC_LEXER_H