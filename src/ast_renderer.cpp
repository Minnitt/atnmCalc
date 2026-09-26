// ast_renderer.cpp
//
// How this works, in short:
//
//   Measure(node) -> Box     computes {width, height, baseline} WITHOUT drawing
//   Draw(node, pos)          actually draws, using Measure() internally wherever
//                            it needs to know a child's size to position it
//
// `baseline` is simplified from real typography: it's just "the y-distance
// from the top of this node's box down to its vertical center line". When
// two nodes sit side by side (e.g. the two sides of a '+'), we align them
// by that center line rather than by their tops, so a fraction (tall) and
// a plain number (short) next to each other still look vertically sane.
//
// Parenthesization: a node only knows it needs parentheses from its PARENT's
// point of view (e.g. an Add node appearing as the left side of a Mul needs
// parens). See NeedsParens() for the precedence rules this follows.

#include "ast_renderer.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <string>

namespace {

struct Box {
    ImVec2 size{0, 0};
    float baseline = 0.0f;
};

int Precedence(BinOp op) {
    switch (op) {
        case BinOp::Add:
        case BinOp::Sub: return 1;
        case BinOp::Mul:
        case BinOp::Div: return 2;
        case BinOp::Pow: return 3;
    }
    return 0;
}

// See file header comment. `parentPrec` is the precedence of the operator
// `child` sits under; `isRightOperand` distinguishes left/right because
// non-associative ops (-, /, and the base side of ^) need parens at EQUAL
// precedence on the "tighter" side, e.g. a-(b-c) != (a-b)-c.
bool NeedsParens(const ASTNodePtr& child, int parentPrec, bool isRightOperand) {
    if (!child) return false;
    if (child->type() == NodeType::UnaryMinus) return parentPrec >= 2;
    if (child->type() != NodeType::BinaryOp) return false; // atoms, Frac, Sqrt are visually self-contained
    auto* b = static_cast<BinaryOpNode*>(child.get());
    int childPrec = Precedence(b->op);
    if (childPrec < parentPrec) return true;
    if (childPrec == parentPrec && isRightOperand) return true;
    return false;
}

const char* OpSymbol(BinOp op) {
    switch (op) {
        case BinOp::Add: return " + ";
        case BinOp::Sub: return " - ";
        case BinOp::Mul: return " \xC3\x97 "; // U+00D7 multiplication sign
        case BinOp::Div: return " / ";
        default: return "?"; // Pow is handled separately (true superscript, not inline text)
    }
}

std::string FormatNumber(double v) {
    char buf[64];
    if (v == static_cast<long long>(v) && std::fabs(v) < 1e15) {
        std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(v));
    } else {
        std::snprintf(buf, sizeof(buf), "%g", v);
    }
    return buf;
}

Box Measure(const ASTNodePtr& node, float fontSize, ImFont* font);
void Draw(const ASTNodePtr& node, ImVec2 topLeft, float fontSize, ImFont* font, ImDrawList* dl, ImU32 color);

Box MeasureMaybeParens(const ASTNodePtr& node, float fontSize, ImFont* font, bool parens) {
    Box inner = Measure(node, fontSize, font);
    if (!parens) return inner;
    ImVec2 p = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, "(");
    Box out;
    out.size.x = inner.size.x + 2.0f * p.x;
    out.size.y = std::max(inner.size.y, p.y);
    out.baseline = std::max(inner.baseline, p.y * 0.5f);
    return out;
}

void DrawMaybeParens(const ASTNodePtr& node, ImVec2 topLeft, float fontSize, ImFont* font,
                      ImDrawList* dl, ImU32 color, bool parens) {
    if (!parens) { Draw(node, topLeft, fontSize, font, dl, color); return; }
    ImVec2 p = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, "(");
    Box outer = MeasureMaybeParens(node, fontSize, font, true);
    Box inner = Measure(node, fontSize, font);
    float parenTop = topLeft.y + (outer.size.y - p.y) * 0.5f;
    float innerTop = topLeft.y + (outer.size.y - inner.size.y) * 0.5f;
    dl->AddText(font, fontSize, ImVec2(topLeft.x, parenTop), color, "(");
    Draw(node, ImVec2(topLeft.x + p.x, innerTop), fontSize, font, dl, color);
    dl->AddText(font, fontSize, ImVec2(topLeft.x + p.x + inner.size.x, parenTop), color, ")");
}

Box Measure(const ASTNodePtr& node, float fontSize, ImFont* font) {
    Box box;
    if (!node) return box;

    switch (node->type()) {
        case NodeType::Number: {
            std::string t = FormatNumber(static_cast<NumberNode*>(node.get())->value);
            box.size = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, t.c_str());
            box.baseline = box.size.y * 0.5f;
            break;
        }
        case NodeType::Variable: {
            const std::string& t = static_cast<VariableNode*>(node.get())->name;
            box.size = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, t.c_str());
            box.baseline = box.size.y * 0.5f;
            break;
        }
        case NodeType::UnaryMinus: {
            auto* u = static_cast<UnaryMinusNode*>(node.get());
            bool parens = NeedsParens(u->operand, 2, false);
            ImVec2 minusSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, "-");
            Box operand = MeasureMaybeParens(u->operand, fontSize, font, parens);
            box.size.x = minusSize.x + operand.size.x;
            box.size.y = std::max(minusSize.y, operand.size.y);
            box.baseline = std::max(minusSize.y * 0.5f, operand.baseline);
            break;
        }
        case NodeType::BinaryOp: {
            auto* b = static_cast<BinaryOpNode*>(node.get());
            if (b->op == BinOp::Pow) {
                // True superscript: exponent drawn smaller, raised so its
                // vertical center sits at the base's top edge. See the
                // derivation in the file header / assignment notes.
                bool baseParens = NeedsParens(b->left, 3, true);
                bool expParens = NeedsParens(b->right, 3, false);
                Box base = MeasureMaybeParens(b->left, fontSize, font, baseParens);
                float expFontSize = fontSize * 0.65f;
                Box exp = MeasureMaybeParens(b->right, expFontSize, font, expParens);
                float raise = exp.size.y * 0.5f;
                box.size.x = base.size.x + exp.size.x;
                box.size.y = base.size.y + raise;
                box.baseline = base.baseline + raise;
                break;
            }
            int prec = Precedence(b->op);
            bool leftParens = NeedsParens(b->left, prec, false);
            bool rightParens = NeedsParens(b->right, prec, true);
            Box left = MeasureMaybeParens(b->left, fontSize, font, leftParens);
            Box right = MeasureMaybeParens(b->right, fontSize, font, rightParens);
            ImVec2 opSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, OpSymbol(b->op));
            box.size.x = left.size.x + opSize.x + right.size.x;
            box.size.y = std::max({left.size.y, opSize.y, right.size.y});
            box.baseline = std::max({left.baseline, opSize.y * 0.5f, right.baseline});
            break;
        }
        case NodeType::Frac: {
            auto* f = static_cast<FracNode*>(node.get());
            Box num = Measure(f->numerator, fontSize, font);
            Box den = Measure(f->denominator, fontSize, font);
            const float gap = fontSize * 0.15f;
            const float bar = std::max(1.0f, fontSize * 0.06f);
            box.size.x = std::max(num.size.x, den.size.x) + fontSize * 0.3f;
            box.size.y = num.size.y + gap + bar + gap + den.size.y;
            box.baseline = num.size.y + gap + bar * 0.5f; // bar sits on the surrounding baseline
            break;
        }
        case NodeType::Sqrt: {
            auto* s = static_cast<SqrtNode*>(node.get());
            Box inner = Measure(s->radicand, fontSize, font);
            ImVec2 radical = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, "\xE2\x88\x9A"); // U+221A
            const float overbarGap = fontSize * 0.18f;
            box.size.x = radical.x + fontSize * 0.08f + inner.size.x;
            box.size.y = overbarGap + std::max(inner.size.y, radical.y);
            box.baseline = overbarGap + inner.baseline;
            break;
        }
    }
    return box;
}

void Draw(const ASTNodePtr& node, ImVec2 topLeft, float fontSize, ImFont* font, ImDrawList* dl, ImU32 color) {
    if (!node) return;
    switch (node->type()) {
        case NodeType::Number:
            dl->AddText(font, fontSize, topLeft, color,
                        FormatNumber(static_cast<NumberNode*>(node.get())->value).c_str());
            break;
        case NodeType::Variable:
            dl->AddText(font, fontSize, topLeft, color,
                        static_cast<VariableNode*>(node.get())->name.c_str());
            break;
        case NodeType::UnaryMinus: {
            auto* u = static_cast<UnaryMinusNode*>(node.get());
            bool parens = NeedsParens(u->operand, 2, false);
            Box whole = Measure(node, fontSize, font);
            ImVec2 minusSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, "-");
            float minusTop = topLeft.y + (whole.size.y - minusSize.y) * 0.5f;
            dl->AddText(font, fontSize, ImVec2(topLeft.x, minusTop), color, "-");
            Box operand = MeasureMaybeParens(u->operand, fontSize, font, parens);
            float opTop = topLeft.y + (whole.size.y - operand.size.y) * 0.5f;
            DrawMaybeParens(u->operand, ImVec2(topLeft.x + minusSize.x, opTop), fontSize, font, dl, color, parens);
            break;
        }
        case NodeType::BinaryOp: {
            auto* b = static_cast<BinaryOpNode*>(node.get());
            if (b->op == BinOp::Pow) {
                bool baseParens = NeedsParens(b->left, 3, true);
                bool expParens = NeedsParens(b->right, 3, false);
                Box base = MeasureMaybeParens(b->left, fontSize, font, baseParens);
                float expFontSize = fontSize * 0.65f;
                Box exp = MeasureMaybeParens(b->right, expFontSize, font, expParens);
                float raise = exp.size.y * 0.5f;
                // Exponent's top aligns with the whole box's top; base sits
                // pushed down by `raise` so the exponent overlaps its top edge.
                DrawMaybeParens(b->right, ImVec2(topLeft.x + base.size.x, topLeft.y),
                                 expFontSize, font, dl, color, expParens);
                DrawMaybeParens(b->left, ImVec2(topLeft.x, topLeft.y + raise),
                                 fontSize, font, dl, color, baseParens);
                break;
            }
            int prec = Precedence(b->op);
            bool leftParens = NeedsParens(b->left, prec, false);
            bool rightParens = NeedsParens(b->right, prec, true);
            Box whole = Measure(node, fontSize, font);
            Box left = MeasureMaybeParens(b->left, fontSize, font, leftParens);
            ImVec2 opSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, OpSymbol(b->op));
            Box right = MeasureMaybeParens(b->right, fontSize, font, rightParens);

            float leftTop = topLeft.y + (whole.size.y - left.size.y) * 0.5f;
            DrawMaybeParens(b->left, ImVec2(topLeft.x, leftTop), fontSize, font, dl, color, leftParens);

            float opX = topLeft.x + left.size.x;
            float opTop = topLeft.y + (whole.size.y - opSize.y) * 0.5f;
            dl->AddText(font, fontSize, ImVec2(opX, opTop), color, OpSymbol(b->op));

            float rightX = opX + opSize.x;
            float rightTop = topLeft.y + (whole.size.y - right.size.y) * 0.5f;
            DrawMaybeParens(b->right, ImVec2(rightX, rightTop), fontSize, font, dl, color, rightParens);
            break;
        }
        case NodeType::Frac: {
            auto* f = static_cast<FracNode*>(node.get());
            Box whole = Measure(node, fontSize, font);
            Box num = Measure(f->numerator, fontSize, font);
            Box den = Measure(f->denominator, fontSize, font);
            const float gap = fontSize * 0.15f;
            const float bar = std::max(1.0f, fontSize * 0.06f);

            float numX = topLeft.x + (whole.size.x - num.size.x) * 0.5f;
            Draw(f->numerator, ImVec2(numX, topLeft.y), fontSize, font, dl, color);

            float barY = topLeft.y + num.size.y + gap;
            dl->AddLine(ImVec2(topLeft.x, barY + bar * 0.5f),
                        ImVec2(topLeft.x + whole.size.x, barY + bar * 0.5f), color, bar);

            float denX = topLeft.x + (whole.size.x - den.size.x) * 0.5f;
            Draw(f->denominator, ImVec2(denX, barY + bar + gap), fontSize, font, dl, color);
            break;
        }
        case NodeType::Sqrt: {
            auto* s = static_cast<SqrtNode*>(node.get());
            Box whole = Measure(node, fontSize, font);
            Box inner = Measure(s->radicand, fontSize, font);
            ImVec2 radical = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, "\xE2\x88\x9A");
            const float overbarGap = fontSize * 0.18f;

            float radicalTop = topLeft.y + (whole.size.y - radical.y);
            dl->AddText(font, fontSize, ImVec2(topLeft.x, radicalTop), color, "\xE2\x88\x9A");

            float innerX = topLeft.x + radical.x + fontSize * 0.08f;
            float overbarY = topLeft.y + overbarGap * 0.4f;
            dl->AddLine(ImVec2(innerX, overbarY), ImVec2(innerX + inner.size.x, overbarY), color,
                        std::max(1.0f, fontSize * 0.05f));

            Draw(s->radicand, ImVec2(innerX, topLeft.y + overbarGap), fontSize, font, dl, color);
            break;
        }
    }
}

} // namespace

ImVec2 MeasureEquation(const ASTNodePtr& node, float fontSize) {
    return Measure(node, fontSize, ImGui::GetFont()).size;
}

void DrawEquation(const ASTNodePtr& node, float fontSize) {
    ImFont* font = ImGui::GetFont();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 topLeft = ImGui::GetCursorScreenPos();
    ImU32 color = ImGui::GetColorU32(ImGuiCol_Text);
    Draw(node, topLeft, fontSize, font, dl, color);
    ImGui::Dummy(Measure(node, fontSize, font).size);
}
