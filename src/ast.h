#pragma once
// ast.h
//
// This is the SHARED CONTRACT between your backend (parser + evaluator)
// and the GUI (renderer). Both sides #include this file. Your parser
// builds trees of these nodes; my renderer walks them to draw fraction
// bars, radical signs, and superscripts.
//
// If you need a node type this doesn't cover yet (e.g. \sin, \sum),
// add it here first, then extend both the parser and ast_renderer.cpp
// to handle it — that keeps the two sides in sync.

#include <memory>
#include <string>

enum class NodeType { Number, Variable, BinaryOp, UnaryMinus, Frac, Sqrt };
enum class BinOp { Add, Sub, Mul, Div, Pow };

// Abstract base. Every concrete node overrides type() so the renderer
// (and evaluator) can switch on it without RTTI/dynamic_cast.
struct ASTNode {
    virtual ~ASTNode() = default;
    virtual NodeType type() const = 0;
};

using ASTNodePtr = std::shared_ptr<ASTNode>;

struct NumberNode : ASTNode {
    double value;
    explicit NumberNode(double v) : value(v) {}
    NodeType type() const override { return NodeType::Number; }
};

struct VariableNode : ASTNode {
    std::string name;
    explicit VariableNode(std::string n) : name(std::move(n)) {}
    NodeType type() const override { return NodeType::Variable; }
};

// Covers + - * / ^  (which operator is in `op`)
struct BinaryOpNode : ASTNode {
    BinOp op;
    ASTNodePtr left;
    ASTNodePtr right;
    BinaryOpNode(BinOp o, ASTNodePtr l, ASTNodePtr r)
        : op(o), left(std::move(l)), right(std::move(r)) {}
    NodeType type() const override { return NodeType::BinaryOp; }
};

// The leading minus in something like -x or -(1+2)
struct UnaryMinusNode : ASTNode {
    ASTNodePtr operand;
    explicit UnaryMinusNode(ASTNodePtr o) : operand(std::move(o)) {}
    NodeType type() const override { return NodeType::UnaryMinus; }
};

// \frac{numerator}{denominator}
struct FracNode : ASTNode {
    ASTNodePtr numerator;
    ASTNodePtr denominator;
    FracNode(ASTNodePtr n, ASTNodePtr d)
        : numerator(std::move(n)), denominator(std::move(d)) {}
    NodeType type() const override { return NodeType::Frac; }
};

// \sqrt{radicand}
struct SqrtNode : ASTNode {
    ASTNodePtr radicand;
    explicit SqrtNode(ASTNodePtr r) : radicand(std::move(r)) {}
    NodeType type() const override { return NodeType::Sqrt; }
};
