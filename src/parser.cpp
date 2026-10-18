#include "regexforge/parser.h"

#include "regexforge/lexer.h"

#include <sstream>
#include <utility>

namespace regexforge {

std::shared_ptr<const AstNode> AstNode::empty() { return std::make_shared<const AstNode>(Kind::Empty); }
std::shared_ptr<const AstNode> AstNode::literal_node(char value) {
    auto node = std::make_shared<AstNode>(Kind::Literal);
    node->literal = value;
    return node;
}
std::shared_ptr<const AstNode> AstNode::concatenation(std::vector<Ast> parts) {
    auto node = std::make_shared<AstNode>(Kind::Concatenation);
    node->children = std::move(parts);
    return node;
}
std::shared_ptr<const AstNode> AstNode::alternation(std::vector<Ast> branches) {
    auto node = std::make_shared<AstNode>(Kind::Alternation);
    node->children = std::move(branches);
    return node;
}
std::shared_ptr<const AstNode> AstNode::star(Ast child) {
    auto node = std::make_shared<AstNode>(Kind::Star);
    node->children.push_back(std::move(child));
    return node;
}

ParseError::ParseError(std::size_t position, std::string message)
    : std::runtime_error("at position " + std::to_string(position) + ": " + std::move(message)), position_(position) {}

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

Ast Parser::parse() {
    Ast result = parse_expression();
    if (current().kind != TokenKind::End) {
        throw ParseError(current().position, "unexpected " + token_kind_name(current().kind));
    }
    return result;
}

Ast Parser::parse_expression() {
    std::vector<Ast> branches;
    branches.push_back(parse_concatenation());
    while (accept(TokenKind::Pipe)) branches.push_back(parse_concatenation());
    return branches.size() == 1 ? branches.front() : AstNode::alternation(std::move(branches));
}

Ast Parser::parse_concatenation() {
    std::vector<Ast> parts;
    while (current().kind == TokenKind::Literal || current().kind == TokenKind::LeftParen) {
        parts.push_back(parse_repetition());
    }
    return parts.empty() ? AstNode::empty() : (parts.size() == 1 ? parts.front() : AstNode::concatenation(std::move(parts)));
}

Ast Parser::parse_repetition() {
    Ast atom = parse_atom();
    if (accept(TokenKind::Star)) {
        if (current().kind == TokenKind::Star) throw ParseError(current().position, "repeated quantifier '*'");
        return AstNode::star(std::move(atom));
    }
    return atom;
}

Ast Parser::parse_atom() {
    if (current().kind == TokenKind::Literal) return AstNode::literal_node(tokens_[index_++].value);
    if (accept(TokenKind::LeftParen)) {
        Ast inner = parse_expression();
        expect(TokenKind::RightParen, "expected ')' to close group");
        return inner;
    }
    throw ParseError(current().position, "expected a literal or '('");
}

const Token& Parser::current() const { return tokens_.at(index_); }
bool Parser::accept(TokenKind kind) {
    if (current().kind != kind) return false;
    ++index_;
    return true;
}
const Token& Parser::expect(TokenKind kind, const char* message) {
    if (current().kind != kind) throw ParseError(current().position, message);
    return tokens_[index_++];
}

Ast parse_pattern(const std::string& pattern) { return Parser(Lexer(pattern).tokenize()).parse(); }

}  // namespace regexforge
