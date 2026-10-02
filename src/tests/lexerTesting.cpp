// lexerTesting.cpp
//
// Covers the categorised equation list: arithmetic/precedence, numbers,
// \frac, \sqrt, variables, whitespace, and malformed input. Checks the
// TOKEN SEQUENCE the lexer produces, not final evaluated values — that's
// the parser/evaluator's job, tested separately.

#include <gtest/gtest.h>
#include "../lexer.h"

namespace {

// Compares only `.type` for each token, in order, IGNORING the trailing
// END sentinel every call to tokenise() appends -- callers list only the
// "real" tokens they expect, same as before END existed. Also confirms
// that sentinel is actually there and is actually last, since that's an
// invariant the parser depends on.
void expectTypes(const std::string& input, const std::vector<TokenType>& expected) {
    auto tokens = Lexer::tokenise(input);
    ASSERT_FALSE(tokens.empty()) << "for input: \"" << input << "\"";
    EXPECT_EQ(tokens.back().type, TokenType::END)
        << "for input: \"" << input << "\" (every token list should end with END)";
    ASSERT_EQ(tokens.size() - 1, expected.size())
        << "for input: \"" << input << "\" (excluding the trailing END token)";
    for (size_t i = 0; i < expected.size(); i++) {
        EXPECT_EQ(tokens[i].type, expected[i])
            << "token " << i << " for input: \"" << input << "\"";
    }
}

void expectThrows(const std::string& input) {
    EXPECT_THROW(Lexer::tokenise(input), std::invalid_argument)
        << "expected an exception for input: \"" << input << "\"";
}

} // namespace

// --- Basic arithmetic & precedence ---------------------------------------

TEST(LexerArithmetic, SimpleAddition) {
    expectTypes("2+3", {TokenType::NUMBER, TokenType::PLUS, TokenType::NUMBER});
}

TEST(LexerArithmetic, AdditionWithSpaces) {
    expectTypes("2 + 3", {TokenType::NUMBER, TokenType::PLUS, TokenType::NUMBER});
}

TEST(LexerArithmetic, MixedPrecedenceOperators) {
    // The lexer doesn't know about precedence -- it just emits tokens in
    // order. Precedence is entirely the parser's problem. This test is
    // really checking "does the lexer stay out of the way".
    expectTypes("2 * 3 + 4 * 5", {
        TokenType::NUMBER, TokenType::ASTERISK, TokenType::NUMBER,
        TokenType::PLUS,
        TokenType::NUMBER, TokenType::ASTERISK, TokenType::NUMBER,
    });
}

TEST(LexerArithmetic, Parentheses) {
    expectTypes("(2+3)*4", {
        TokenType::LPAREN, TokenType::NUMBER, TokenType::PLUS, TokenType::NUMBER, TokenType::RPAREN,
        TokenType::ASTERISK, TokenType::NUMBER,
    });
}

TEST(LexerArithmetic, AllOperatorsRecognised) {
    expectTypes("+-*/^", {
        TokenType::PLUS, TokenType::MINUS, TokenType::ASTERISK, TokenType::SLASH, TokenType::CARET,
    });
}

TEST(LexerArithmetic, MinusIsNotFoldedIntoNumber) {
    // The lexer should NOT try to be clever about unary minus -- "-5" is
    // MINUS then NUMBER(5), two tokens. Whether that minus means negation
    // or subtraction is a parsing decision, made later with context the
    // lexer doesn't have.
    expectTypes("-5", {TokenType::MINUS, TokenType::NUMBER});
}

// --- Numbers ---------------------------------------------------------------

TEST(LexerNumbers, IntegerValue) {
    auto tokens = Lexer::tokenise("42");
    ASSERT_EQ(tokens.size(), 2);  // NUMBER, END
    EXPECT_EQ(tokens[0].type, TokenType::NUMBER);
    EXPECT_DOUBLE_EQ(tokens[0].value, 42.0);
}

TEST(LexerNumbers, DecimalValue) {
    auto tokens = Lexer::tokenise("12.5");
    ASSERT_EQ(tokens.size(), 2);  // NUMBER, END
    EXPECT_EQ(tokens[0].type, TokenType::NUMBER);
    EXPECT_DOUBLE_EQ(tokens[0].value, 12.5);
}

TEST(LexerNumbers, LeadingDecimalPoint) {
    // ".5" -- decide deliberately whether this is valid; this test
    // documents whichever choice you made. Currently expecting it to work.
    auto tokens = Lexer::tokenise(".5");
    ASSERT_EQ(tokens.size(), 2);  // NUMBER, END
    EXPECT_DOUBLE_EQ(tokens[0].value, 0.5);
}

TEST(LexerNumbers, MultipleDecimalPointsThrows) {
    // Regression test for the silent-truncation bug: std::stod("1.2.3")
    // doesn't throw on its own, it just parses "1.2" and stops. The lexer
    // must explicitly check that the whole numeric run was consumed.
    expectThrows("1.2.3");
}

TEST(LexerNumbers, LoneDecimalPointThrows) {
    expectThrows(".");
}

// --- \frac -------------------------------------------------------------

TEST(LexerFrac, Structure) {
    expectTypes("\\frac{1}{2}", {
        TokenType::FRAC,
        TokenType::LBRACE, TokenType::NUMBER, TokenType::RBRACE,
        TokenType::LBRACE, TokenType::NUMBER, TokenType::RBRACE,
    });
}

TEST(LexerFrac, NestedFrac) {
    expectTypes("\\frac{\\frac{1}{2}}{\\frac{1}{4}}", {
        TokenType::FRAC, TokenType::LBRACE,
            TokenType::FRAC, TokenType::LBRACE, TokenType::NUMBER, TokenType::RBRACE,
                             TokenType::LBRACE, TokenType::NUMBER, TokenType::RBRACE,
        TokenType::RBRACE,
        TokenType::LBRACE,
            TokenType::FRAC, TokenType::LBRACE, TokenType::NUMBER, TokenType::RBRACE,
                             TokenType::LBRACE, TokenType::NUMBER, TokenType::RBRACE,
        TokenType::RBRACE,
    });
}

TEST(LexerFrac, SpacesInsideBraces) {
    expectTypes("\\frac { 1 } { 2 }", {
        TokenType::FRAC,
        TokenType::LBRACE, TokenType::NUMBER, TokenType::RBRACE,
        TokenType::LBRACE, TokenType::NUMBER, TokenType::RBRACE,
    });
}

// --- \sqrt -------------------------------------------------------------

TEST(LexerSqrt, Structure) {
    expectTypes("\\sqrt{9}", {TokenType::SQRT, TokenType::LBRACE, TokenType::NUMBER, TokenType::RBRACE});
}

TEST(LexerSqrt, NestedInsideFrac) {
    expectTypes("\\sqrt{\\frac{1}{4}}", {
        TokenType::SQRT, TokenType::LBRACE,
            TokenType::FRAC, TokenType::LBRACE, TokenType::NUMBER, TokenType::RBRACE,
                             TokenType::LBRACE, TokenType::NUMBER, TokenType::RBRACE,
        TokenType::RBRACE,
    });
}

// --- Variables ---------------------------------------------------------

TEST(LexerVariables, SingleVariable) {
    auto tokens = Lexer::tokenise("x");
    ASSERT_EQ(tokens.size(), 2);  // VARIABLE, END
    EXPECT_EQ(tokens[0].type, TokenType::VARIABLE);
    EXPECT_EQ(tokens[0].name, 'x');
}

TEST(LexerVariables, AdjacentLettersAreSeparateVariables) {
    // Each letter is its OWN token -- "xy" is two VARIABLE tokens, not one
    // token for a name "xy". This is the per-character-variable decision
    // discussed earlier, matching LaTeX math-mode convention.
    auto tokens = Lexer::tokenise("xy");
    ASSERT_EQ(tokens.size(), 3);  // VARIABLE, VARIABLE, END
    EXPECT_EQ(tokens[0].type, TokenType::VARIABLE);
    EXPECT_EQ(tokens[0].name, 'x');
    EXPECT_EQ(tokens[1].type, TokenType::VARIABLE);
    EXPECT_EQ(tokens[1].name, 'y');
}

TEST(LexerVariables, LongAlphabeticRunIsManySeparateVariables) {
    auto tokens = Lexer::tokenise("xyz");
    ASSERT_EQ(tokens.size(), 4);  // VARIABLE x3, END
    for (size_t i = 0; i < tokens.size() - 1; i++) EXPECT_EQ(tokens[i].type, TokenType::VARIABLE);
    EXPECT_EQ(tokens.back().type, TokenType::END);
}

TEST(LexerVariables, VariableNextToNumber) {
    expectTypes("2x", {TokenType::NUMBER, TokenType::VARIABLE});
}

// --- Whitespace & formatting robustness ---------------------------------

TEST(LexerWhitespace, LeadingAndTrailingSpacesIgnored) {
    expectTypes("  2 + 3  ", {TokenType::NUMBER, TokenType::PLUS, TokenType::NUMBER});
}

TEST(LexerWhitespace, SpacingDoesNotAffectTokenCount) {
    auto tight = Lexer::tokenise("2+3");
    auto spaced = Lexer::tokenise("2 + 3");
    ASSERT_EQ(tight.size(), spaced.size());
    for (size_t i = 0; i < tight.size(); i++) {
        EXPECT_EQ(tight[i].type, spaced[i].type);
    }
}

// --- Malformed input (should throw, not crash or hang) ------------------

TEST(LexerMalformed, UnknownCommandThrows) {
    expectThrows("\\unknown{1}");
}

TEST(LexerMalformed, UnrecognisedCharacterThrows) {
    expectThrows("2 $ 3");
}

TEST(LexerMalformed, EmptyStringProducesJustEndToken) {
    // Not an error at the LEXER level -- an empty equation is a valid
    // (if useless) token stream, just the END sentinel with nothing
    // before it. Whether that's an error is the PARSER's decision (it'll
    // see END immediately and can decide what that means).
    auto tokens = Lexer::tokenise("");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].type, TokenType::END);
}

TEST(LexerMalformed, TrailingBackslashThrows) {
    // "\" with nothing after it -- no command word to read at all.
    expectThrows("2 + \\");
}
