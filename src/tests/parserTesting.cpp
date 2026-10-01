// parserTesting.cpp
//
// Covers the same categorised equation list as the lexer tests, but
// checks what the PARSER produces: tree shape (which node types, in
// what nesting) and, via a tiny test-only evaluator below, the final
// numeric result. Tree-shape checks matter separately from value
// checks -- two different parses can evaluate to the same number while
// being structurally wrong (see the UnaryMinus-vs-BinaryOp discussion).

#include <gtest/gtest.h>
#include "../lexer.h"
#include "../parser.h"
#include "../ast.h"
#include <map>
#include <cmath>
#include <stdexcept>

namespace {

ASTNodePtr parse(const std::string& input) {
    auto tokens = Lexer::tokenise(input);
    Parser parser(std::move(tokens));
    return parser.parse();
}

// Test-only recursive evaluator -- NOT your graded backend. Exists purely
// so these tests can check final numeric results without depending on
// your real evaluateAst() being finished yet.
double evaluate(const ASTNodePtr& node, const std::map<std::string, double>& vars) {
    switch (node->type()) {
        case NodeType::Number:
            return static_cast<NumberNode*>(node.get())->value;
        case NodeType::Variable: {
            auto* v = static_cast<VariableNode*>(node.get());
            auto it = vars.find(v->name);
            if (it == vars.end()) throw std::invalid_argument("unbound variable '" + v->name + "'");
            return it->second;
        }
        case NodeType::UnaryMinus:
            return -evaluate(static_cast<UnaryMinusNode*>(node.get())->operand, vars);
        case NodeType::BinaryOp: {
            auto* b = static_cast<BinaryOpNode*>(node.get());
            double l = evaluate(b->left, vars), r = evaluate(b->right, vars);
            switch (b->op) {
                case BinOp::Add: return l + r;
                case BinOp::Sub: return l - r;
                case BinOp::Mul: return l * r;
                case BinOp::Div: return l / r;
                case BinOp::Pow: return std::pow(l, r);
            }
            throw std::logic_error("unhandled BinOp");
        }
        case NodeType::Frac: {
            auto* f = static_cast<FracNode*>(node.get());
            return evaluate(f->numerator, vars) / evaluate(f->denominator, vars);
        }
        case NodeType::Sqrt:
            return std::sqrt(evaluate(static_cast<SqrtNode*>(node.get())->radicand, vars));
    }
    throw std::logic_error("unhandled NodeType");
}

double parseAndEvaluate(const std::string& input, const std::map<std::string, double>& vars = {}) {
    return evaluate(parse(input), vars);
}

} // namespace

// --- Basic arithmetic & precedence ---------------------------------------

TEST(ParserArithmetic, SimpleAddition) {
    EXPECT_DOUBLE_EQ(parseAndEvaluate("2 + 3 * 4"), 14.0);
}

TEST(ParserArithmetic, ParenthesesOverridePrecedence) {
    EXPECT_DOUBLE_EQ(parseAndEvaluate("(2 + 3) * 4"), 20.0);
}

TEST(ParserArithmetic, SubtractionIsLeftAssociative) {
    // if this is 11 instead of 5, subtraction is grouping right-to-left
    EXPECT_DOUBLE_EQ(parseAndEvaluate("10 - 2 - 3"), 5.0);
}

TEST(ParserArithmetic, DivisionIsLeftAssociative) {
    EXPECT_DOUBLE_EQ(parseAndEvaluate("10 / 2 / 5"), 1.0);
}

TEST(ParserArithmetic, OperandPopOrderIsCorrect) {
    // regression guard: applyTopOperator must pop RIGHT first, then LEFT.
    // Swap that order and this silently becomes 2 - 5 = -3 instead of 3.
    EXPECT_DOUBLE_EQ(parseAndEvaluate("5 - 2"), 3.0);
    EXPECT_DOUBLE_EQ(parseAndEvaluate("10 / 2"), 5.0);
}

// --- Exponentiation (right-associativity) --------------------------------

TEST(ParserExponent, RightAssociative) {
    // if this is 64, ^ is grouping left-to-right instead of right-to-left
    EXPECT_DOUBLE_EQ(parseAndEvaluate("2^3^2"), 512.0);
}

TEST(ParserExponent, SimpleCase) {
    EXPECT_DOUBLE_EQ(parseAndEvaluate("2^0"), 1.0);
}

TEST(ParserExponent, ParenthesizedBase) {
    EXPECT_DOUBLE_EQ(parseAndEvaluate("(-2)^2"), 4.0);
}

TEST(ParserExponent, UnaryMinusBindsLooserThanExponent) {
    // documents the associativity/precedence decision made for UNARY_MINUS
    // in precedence() -- update this test if that decision changes.
    EXPECT_DOUBLE_EQ(parseAndEvaluate("-2^2"), -4.0);
}

// --- \frac ----------------------------------------------------------------

TEST(ParserFrac, Structure) {
    auto ast = parse("\\frac{1}{2}");
    ASSERT_EQ(ast->type(), NodeType::Frac);
    auto* frac = static_cast<FracNode*>(ast.get());
    EXPECT_EQ(frac->numerator->type(), NodeType::Number);
    EXPECT_EQ(frac->denominator->type(), NodeType::Number);
}

TEST(ParserFrac, NumeratorAndDenominatorAreNotSwapped) {
    // regression guard for exactly the mix-up caught earlier by hand-tracing
    auto ast = parse("\\sqrt{\\frac{5*62}{3}}");
    auto* sqrt = static_cast<SqrtNode*>(ast.get());
    auto* frac = static_cast<FracNode*>(sqrt->radicand.get());
    EXPECT_DOUBLE_EQ(evaluate(frac->numerator, {}), 310.0);
    EXPECT_DOUBLE_EQ(evaluate(frac->denominator, {}), 3.0);
}

TEST(ParserFrac, EvaluatesCorrectly) {
    EXPECT_DOUBLE_EQ(parseAndEvaluate("\\frac{1}{2} + \\frac{1}{3}"), 1.0 / 2.0 + 1.0 / 3.0);
}

TEST(ParserFrac, NestedFrac) {
    EXPECT_DOUBLE_EQ(parseAndEvaluate("\\frac{\\frac{1}{2}}{\\frac{1}{4}}"), 2.0);
}

// --- \sqrt ------------------------------------------------------------

TEST(ParserSqrt, Structure) {
    auto ast = parse("\\sqrt{9}");
    ASSERT_EQ(ast->type(), NodeType::Sqrt);
}

TEST(ParserSqrt, EvaluatesCorrectly) {
    EXPECT_DOUBLE_EQ(parseAndEvaluate("\\sqrt{9}"), 3.0);
}

// --- Unary minus ---------------------------------------------------------

TEST(ParserUnaryMinus, ProducesUnaryMinusNodeNotBinaryOp) {
    // regression guard for the earlier 0-x mix-up: check the NODE TYPE,
    // not just the evaluated value, since both approaches evaluate the
    // same but only one matches what the renderer expects.
    auto ast = parse("-5");
    EXPECT_EQ(ast->type(), NodeType::UnaryMinus);
}

TEST(ParserUnaryMinus, NegatesOnlyTheImmediateOperand) {
    EXPECT_DOUBLE_EQ(parseAndEvaluate("-5 + 3"), -2.0);
    EXPECT_DOUBLE_EQ(parseAndEvaluate("3 + -5"), -2.0);
}

TEST(ParserUnaryMinus, ChainedUnaryMinus) {
    // documents current behaviour for "--5" -- decide deliberately whether
    // this is the convention you want (currently: double negation = 5).
    EXPECT_DOUBLE_EQ(parseAndEvaluate("--5"), 5.0);
}

// --- Variables -----------------------------------------------------------

TEST(ParserVariables, SingleVariable) {
    EXPECT_DOUBLE_EQ(parseAndEvaluate("x + 1", {{"x", 5.0}}), 6.0);
}

TEST(ParserVariables, PreservesActualLetterNotAsciiCode) {
    // regression guard for the earlier std::to_string(char) bug
    auto ast = parse("x");
    auto* v = static_cast<VariableNode*>(ast.get());
    EXPECT_EQ(v->name, "x");
}

TEST(ParserVariables, UnboundVariableThrowsOnEvaluate) {
    EXPECT_THROW(parseAndEvaluate("x + y", {{"x", 5.0}}), std::invalid_argument);
}

TEST(ParserVariables, ImplicitMultiplicationOfAdjacentVariables) {
    EXPECT_DOUBLE_EQ(parseAndEvaluate("xy", {{"x", 3.0}, {"y", 4.0}}), 12.0);
}

TEST(ParserVariables, NumberNextToVariableIsImplicitMultiplication) {
    EXPECT_DOUBLE_EQ(parseAndEvaluate("2x", {{"x", 5.0}}), 10.0);
}

// --- Whitespace ------------------------------------------------------------

TEST(ParserWhitespace, SpacingDoesNotAffectResult) {
    EXPECT_DOUBLE_EQ(parseAndEvaluate("2+3"), parseAndEvaluate("2 + 3"));
    EXPECT_DOUBLE_EQ(parseAndEvaluate("  2 + 3  "), 5.0);
}

// --- Malformed input -------------------------------------------------------

TEST(ParserMalformed, TrailingOperatorThrows) {
    EXPECT_THROW(parse("2 +"), std::exception);
}

TEST(ParserMalformed, UnmatchedClosingParenThrows) {
    // trailing garbage after a complete expression -- this is exactly
    // what Parser::parse()'s expect(TokenType::END) check exists to catch
    EXPECT_THROW(parse("2 + 3)"), std::exception);
}

TEST(ParserMalformed, EmptyParensThrows) {
    EXPECT_THROW(parse("()"), std::exception);
}

TEST(ParserMalformed, MissingClosingBraceThrows) {
    EXPECT_THROW(parse("\\frac{1}{2"), std::exception);
}

TEST(ParserMalformed, DanglingCaretThrows) {
    EXPECT_THROW(parse("2^"), std::exception);
}

// --- Open design question: decide, then fix this test to match -----------

TEST(ParserDesignDecision, BareNumbersWithNoOperatorBetweenThem) {
    // "2 3" is currently ambiguous in your design: your implicit-multiplication
    // case list still includes NUMBER, which means this currently evaluates
    // to 6 rather than throwing. The mock backend deliberately excluded this
    // (see atAtomStart() there) since two bare numbers side by side is almost
    // always a typo, unlike "2x". Pick one on purpose:
    //
       EXPECT_THROW(parse("2 3"), std::exception);       // if you exclude NUMBER
    //   EXPECT_DOUBLE_EQ(parseAndEvaluate("2 3"), 6.0);    // if you keep it as-is
    
}
