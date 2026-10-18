#pragma once

#include "regexforge/ast.h"
#include "regexforge/token.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace regexforge {

class ParseError : public std::runtime_error {
public:
    ParseError(std::size_t position, std::string message);
    std::size_t position() const noexcept { return position_; }

private:
    std::size_t position_;
};

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);
    Ast parse();

private:
    Ast parse_expression();
    Ast parse_concatenation();
    Ast parse_repetition();
    Ast parse_atom();
    const Token& current() const;
    bool accept(TokenKind kind);
    const Token& expect(TokenKind kind, const char* message);

    std::vector<Token> tokens_;
    std::size_t index_ = 0;
};

Ast parse_pattern(const std::string& pattern);

}  // namespace regexforge
