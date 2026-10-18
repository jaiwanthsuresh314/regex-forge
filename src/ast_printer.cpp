#include "regexforge/ast_printer.h"

#include <sstream>

namespace regexforge {

namespace {
std::string print(const Ast& node) {
    switch (node->kind) {
        case AstNode::Kind::Empty: return "Empty";
        case AstNode::Kind::Literal: return "Literal('" + std::string(1, node->literal) + "')";
        case AstNode::Kind::Star: return "Star(" + print(node->children.front()) + ")";
        case AstNode::Kind::Concatenation: {
            std::string result = "Concat(";
            for (std::size_t i = 0; i < node->children.size(); ++i) result += (i ? ", " : "") + print(node->children[i]);
            return result + ")";
        }
        case AstNode::Kind::Alternation: {
            std::string result = "Alt(";
            for (std::size_t i = 0; i < node->children.size(); ++i) result += (i ? ", " : "") + print(node->children[i]);
            return result + ")";
        }
    }
    return "<invalid>";
}
}

std::string print_ast(const Ast& ast) { return print(ast); }

}  // namespace regexforge
