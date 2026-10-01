// evaluatorTesting.cpp
//
// Tests Evaluator::evaluateAST against real ASTs produced by the actual
// Lexer + Parser (not the mock backend) -- these are effectively
// end-to-end tests of the whole pipeline: tokenise -> parse -> evaluate.

#include <gtest/gtest.h>
#include "../lexer.h"
#include "../parser.h"
#include "../evaluator.h"
#include <map>
#include <cmath>

namespace {

ASTNodePtr parse(const std::string& input) {
    Parser parser(Lexer::tokenise(input));
    return parser.parse();
}

EvalResult eval(const std::string& input, const std::map<std::string, double>& vars = {}) {
    return Evaluator::evaluateAST(parse(input), vars);
}

} // namespace

// --- Basic arithmetic & precedence ---------------------------------------

TEST(EvaluatorArithmetic, SimpleAddition) {
    auto r = eval("2 + 3 * 4");
    ASSERT_TRUE(r.success);
    EXPECT_DOUBLE_EQ(r.value, 14.0);
}

TEST(EvaluatorArithmetic, ParenthesesOverridePrecedence) {
    EXPECT_DOUBLE_EQ(eval("(2 + 3) * 4").value, 20.0);
}

TEST(EvaluatorArithmetic, SubtractionIsLeftAssociative) {
    EXPECT_DOUBLE_EQ(eval("10 - 2 - 3").value, 5.0);
}

TEST(EvaluatorArithmetic, DivisionIsLeftAssociative) {
    EXPECT_DOUBLE_EQ(eval("10 / 2 / 5").value, 1.0);
}

TEST(EvaluatorArithmetic, OperandOrderNotSwapped) {
    // regression guard -- catches both the parser's pop-order bug and
    // the evaluator's earlier Pow argument-order bug in one place
    EXPECT_DOUBLE_EQ(eval("5 - 2").value, 3.0);
    EXPECT_DOUBLE_EQ(eval("10 / 2").value, 5.0);
    EXPECT_DOUBLE_EQ(eval("2^3").value, 8.0);   // was 9.0 when base/exponent were swapped
}

// --- Exponentiation --------------------------------------------------------

TEST(EvaluatorExponent, RightAssociative) {
    EXPECT_DOUBLE_EQ(eval("2^3^2").value, 512.0);
}

TEST(EvaluatorExponent, NegativeExponentIsValid) {
    EXPECT_DOUBLE_EQ(eval("2^-1").value, 0.5);
}

TEST(EvaluatorExponent, UnaryMinusBindsLooserThanExponent) {
    EXPECT_DOUBLE_EQ(eval("-2^2").value, -4.0);
}

// --- \frac -----------------------------------------------------------------

TEST(EvaluatorFrac, SimpleCase) {
    EXPECT_DOUBLE_EQ(eval("\\frac{1}{2}").value, 0.5);
}

TEST(EvaluatorFrac, NestedFrac) {
    EXPECT_DOUBLE_EQ(eval("\\frac{\\frac{1}{2}}{\\frac{1}{4}}").value, 2.0);
}

TEST(EvaluatorFrac, WithVariables) {
    EXPECT_DOUBLE_EQ(eval("\\frac{x}{2} + 1", {{"x", 10.0}}).value, 6.0);
}

// --- \sqrt ------------------------------------------------------------

TEST(EvaluatorSqrt, SimpleCase) {
    EXPECT_DOUBLE_EQ(eval("\\sqrt{9}").value, 3.0);
}

TEST(EvaluatorSqrt, CombinedWithFrac) {
    // the exact case caught during hand-tracing earlier
    EXPECT_DOUBLE_EQ(eval("\\sqrt{\\frac{5*62}{3}}").value, std::sqrt(310.0 / 3.0));
}

// --- Unary minus -----------------------------------------------------------

TEST(EvaluatorUnaryMinus, NegatesOnlyImmediateOperand) {
    EXPECT_DOUBLE_EQ(eval("-5 + 3").value, -2.0);
    EXPECT_DOUBLE_EQ(eval("3 + -5").value, -2.0);
}

TEST(EvaluatorUnaryMinus, ChainedUnaryMinus) {
    EXPECT_DOUBLE_EQ(eval("--5").value, 5.0);
}

// --- Variables ---------------------------------------------------------

TEST(EvaluatorVariables, SingleVariable) {
    EXPECT_DOUBLE_EQ(eval("x + 1", {{"x", 5.0}}).value, 6.0);
}

TEST(EvaluatorVariables, ImplicitMultiplication) {
    EXPECT_DOUBLE_EQ(eval("xy", {{"x", 3.0}, {"y", 4.0}}).value, 12.0);
}

TEST(EvaluatorVariables, UnboundVariableFails) {
    auto r = eval("x + y", {{"x", 5.0}});
    EXPECT_FALSE(r.success);
    EXPECT_FALSE(r.errorMessage.empty());
}

// --- Error cases: check success==false, not exceptions -----------------
// (evaluateAST's whole contract is that it catches internally and reports
// via EvalResult -- these tests are checking that contract specifically)

TEST(EvaluatorErrors, DivisionByZeroFails) {
    EXPECT_FALSE(eval("1/0").success);
}

TEST(EvaluatorErrors, FracByZeroFails) {
    EXPECT_FALSE(eval("\\frac{1}{0}").success);
}

TEST(EvaluatorErrors, SqrtOfNegativeFails) {
    EXPECT_FALSE(eval("\\sqrt{-1}").success);
}

TEST(EvaluatorErrors, SuccessfulEvaluationHasNoErrorMessage) {
    auto r = eval("2 + 2");
    EXPECT_TRUE(r.success);
    EXPECT_TRUE(r.errorMessage.empty());
}