//
// Created by iadna on 28/09/2026.
//

#include "lexer.h"

#include <stdexcept>

std::vector<Token> Lexer::tokenise(const std::string &source) {
    std::vector<Token> tokens;
    std::size_t index = 0;

    while (index < source.length()) {
        Token token{};
        token.index = index;
        char c = source[index]; //char to be lexed

        if (std::isspace(c)) {
            index++;
            continue;
        }
        else if (c == '.' || std::isdigit(c)) {
            std::size_t consumed = 0;
            token.type = TokenType::NUMBER;
            std::string number{};
            while (c == '.' || std::isdigit(c)) {
                number.push_back(c);
                index++;
                c = source[index];
            }

            double value = std::stod(number, &consumed); //if the input is just '.', std::invalid_argument will be thrown
            if (consumed != number.size()) {
                throw std::invalid_argument("malformed number '" + number + "'");
            }
            token.value = value;
        }
        else if (std::isalpha(c)) {
            token.type = TokenType::VARIABLE;
            token.name = c;
            index++;
        }
        else if (c == '\\') {
            index++; //consume the backslash
            std::string word{};
            while (index < source.length() && std::isalpha(source[index])) {
                word.push_back(source[index]);
                index++;
            }
            switch (CommandType commandType = getCommandType(word)) {
                case CommandType::FRAC: token.type = TokenType::FRAC;  break;
                case CommandType::SQRT: token.type = TokenType::SQRT;  break;
                default:
                    throw std::invalid_argument("Unknown command type");
            }
        }
        else {
            switch (c) {
                case '(': token.type = TokenType::LPAREN;   break;
                case ')': token.type = TokenType::RPAREN;   break;
                case '{': token.type = TokenType::LBRACE;   break;
                case '}': token.type = TokenType::RBRACE;   break;
                case '+': token.type = TokenType::PLUS;     break;
                case '-': token.type = TokenType::MINUS;    break;
                case '*': token.type = TokenType::ASTERISK; break;
                case '/': token.type = TokenType::SLASH;    break;
                case '^': token.type = TokenType::CARET;    break;
                default:
                    throw std::invalid_argument(
                        std::string("Unknown character: '") + c + "' at " + std::to_string(index)
                        );
                    break;
            }
            index++;
        }
        tokens.push_back(token);
    }
    return tokens;
}

CommandType Lexer::getCommandType(const std::string &source) {
    auto it = CommandTypeMap.find(source);
    if (it != CommandTypeMap.end()) {
        return it->second;
    }
    return CommandType::UNKNOWN;
}
