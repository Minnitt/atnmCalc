// mock_backend.cpp
//
// *** PLACEHOLDER — NOT the graded backend. Delete this file once you've
// *** written your own lexer/parser/evaluator per the assignment, and
// *** implement parseLatex() / evaluateAst() for real.
//
// This exists only so the GUI has something to call while it's being
// built, so the renderer (fraction bars, sqrt, superscripts) can be
// tested against real ASTs from day one instead of waiting on the
// backend to be finished. It's a quick recursive-descent parser over
// the same grammar we discussed:
//
//   expression := term (('+' | '-') term)*
//   term       := factor ( ('*' | '/') factor | factor )*
//   factor     := unary ('^' factor)?
//   unary      := '-' unary | atom
//   atom       := NUMBER | IDENT | '(' expression ')'
//               | '\frac' '{' expression '}' '{' expression '}'
//               | '\sqrt' '{' expression '}'
//
// NOTE on IDENT and implicit multiplication (a deliberate grammar
// decision, not an oversight — you may want to make a different choice
// in your real backend, but document whichever you pick):
//
// Each letter is its OWN single-character variable ("xy" is the two
// variables x and y, not one variable named "xy"), matching standard
// math notation. That only makes sense combined with implicit
// multiplication via juxtaposition: "xy" means x*y, "2x" means 2*x,
// "2(x+1)" means 2*(x+1). See `term`'s trailing bare `factor` case
// above, and atAtomStart() below.
//
// Implicit multiplication deliberately does NOT trigger between two
// plain numbers separated only by whitespace — "2 3" is still a parse
// error, not "2*3" — since that's ambiguous/almost certainly a typo in
// a way "2x" is not. See atAtomStart()'s comment for the exact rule.

#include "backend_interface.h"
#include <cctype>
#include <cmath>
#include <set>
#include <stdexcept>

namespace {

struct ParseException : std::runtime_error {
    explicit ParseException(const std::string& m) : std::runtime_error(m) {}
};

class Parser {
public:
    Parser(const std::string& text, std::set<std::string>& vars) : text_(text), vars_(vars) {}

    ASTNodePtr parseExpression() {
        ASTNodePtr left = parseTerm();
        for (;;) {
            if (consume("+")) left = std::make_shared<BinaryOpNode>(BinOp::Add, left, parseTerm());
            else if (consume("-")) left = std::make_shared<BinaryOpNode>(BinOp::Sub, left, parseTerm());
            else return left;
        }
    }

    void expectEnd() {
        skipSpace();
        if (pos_ != text_.size()) throw ParseException("unexpected trailing input");
    }

private:
    const std::string& text_;
    size_t pos_ = 0;
    std::set<std::string>& vars_;

    void skipSpace() { while (pos_ < text_.size() && std::isspace((unsigned char)text_[pos_])) pos_++; }

    bool consume(const std::string& lit) {
        skipSpace();
        if (text_.compare(pos_, lit.size(), lit) == 0) { pos_ += lit.size(); return true; }
        return false;
    }

    char peekChar() { skipSpace(); return pos_ < text_.size() ? text_[pos_] : '\0'; }

    // Lookahead (does not consume): could the next token start a new atom
    // via implicit multiplication, e.g. the 'x' in "2x" or the '(' in
    // "2(x+1)"? Deliberately excludes digits/'.' — two adjacent number
    // literals ("2 3") stay a parse error rather than silently becoming
    // "2*3"; that combination is essentially always a typo, unlike "2x".
    bool atAtomStart() {
        size_t save = pos_;
        skipSpace();
        bool result = pos_ < text_.size() &&
                      (std::isalpha((unsigned char)text_[pos_]) || text_[pos_] == '(' || text_[pos_] == '\\');
        pos_ = save;
        return result;
    }

    ASTNodePtr parseTerm() {
        ASTNodePtr left = parseFactor();
        for (;;) {
            if (consume("*")) left = std::make_shared<BinaryOpNode>(BinOp::Mul, left, parseFactor());
            else if (consume("/")) left = std::make_shared<BinaryOpNode>(BinOp::Div, left, parseFactor());
            else if (atAtomStart()) left = std::make_shared<BinaryOpNode>(BinOp::Mul, left, parseFactor());
            else return left;
        }
    }

    ASTNodePtr parseFactor() {
        ASTNodePtr base = parseUnary();
        if (consume("^")) return std::make_shared<BinaryOpNode>(BinOp::Pow, base, parseFactor()); // right-assoc
        return base;
    }

    ASTNodePtr parseUnary() {
        if (consume("-")) return std::make_shared<UnaryMinusNode>(parseUnary());
        return parseAtom();
    }

    ASTNodePtr parseAtom() {
        if (consume("\\frac")) {
            expect("{"); ASTNodePtr num = parseExpression(); expect("}");
            expect("{"); ASTNodePtr den = parseExpression(); expect("}");
            return std::make_shared<FracNode>(num, den);
        }
        if (consume("\\sqrt")) {
            expect("{"); ASTNodePtr inner = parseExpression(); expect("}");
            return std::make_shared<SqrtNode>(inner);
        }
        if (consume("(")) {
            ASTNodePtr inner = parseExpression();
            expect(")");
            return inner;
        }
        char c = peekChar();
        if (std::isdigit((unsigned char)c) || c == '.') return parseNumber();
        if (std::isalpha((unsigned char)c)) return parseVariable();
        throw ParseException(c == '\0' ? "unexpected end of input" : std::string("unexpected character '") + c + "'");
    }

    void expect(const std::string& lit) {
        if (!consume(lit)) throw ParseException("expected '" + lit + "'");
    }

    ASTNodePtr parseNumber() {
        skipSpace();
        size_t start = pos_;
        while (pos_ < text_.size() && (std::isdigit((unsigned char)text_[pos_]) || text_[pos_] == '.')) pos_++;
        return std::make_shared<NumberNode>(std::stod(text_.substr(start, pos_ - start)));
    }

    ASTNodePtr parseVariable() {
        // One character = one variable ("xy" is x*y via implicit
        // multiplication above, not a variable literally named "xy").
        skipSpace();
        std::string name(1, text_[pos_]);
        pos_++;
        vars_.insert(name);
        return std::make_shared<VariableNode>(name);
    }
};

double evalNode(const ASTNodePtr& node, const std::map<std::string, double>& vars) {
    switch (node->type()) {
        case NodeType::Number: return static_cast<NumberNode*>(node.get())->value;
        case NodeType::Variable: {
            auto* v = static_cast<VariableNode*>(node.get());
            auto it = vars.find(v->name);
            if (it == vars.end()) throw ParseException("unbound variable '" + v->name + "'");
            return it->second;
        }
        case NodeType::UnaryMinus:
            return -evalNode(static_cast<UnaryMinusNode*>(node.get())->operand, vars);
        case NodeType::BinaryOp: {
            auto* b = static_cast<BinaryOpNode*>(node.get());
            double l = evalNode(b->left, vars), r = evalNode(b->right, vars);
            switch (b->op) {
                case BinOp::Add: return l + r;
                case BinOp::Sub: return l - r;
                case BinOp::Mul: return l * r;
                case BinOp::Div:
                    if (r == 0.0) throw ParseException("division by zero");
                    return l / r;
                case BinOp::Pow: return std::pow(l, r);
            }
            break;
        }
        case NodeType::Frac: {
            auto* f = static_cast<FracNode*>(node.get());
            double d = evalNode(f->denominator, vars);
            if (d == 0.0) throw ParseException("division by zero in \\frac");
            return evalNode(f->numerator, vars) / d;
        }
        case NodeType::Sqrt: {
            double v = evalNode(static_cast<SqrtNode*>(node.get())->radicand, vars);
            if (v < 0.0) throw ParseException("\\sqrt of a negative number");
            return std::sqrt(v);
        }
    }
    throw ParseException("internal error: unknown node type");
}

} // namespace

ParseResult parseLatex(const std::string& latex) {
    ParseResult result;
    std::set<std::string> vars;
    try {
        Parser parser(latex, vars);
        result.root = parser.parseExpression();
        parser.expectEnd();
        result.success = true;
        result.variables.assign(vars.begin(), vars.end());
    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = e.what();
    }
    return result;
}

EvalResult evaluateAst(const ASTNodePtr& root, const std::map<std::string, double>& variables) {
    EvalResult result;
    try {
        result.value = evalNode(root, variables);
        result.success = true;
    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = e.what();
    }
    return result;
}
