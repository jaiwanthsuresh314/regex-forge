#include "regexforge/lexer.h"

namespace regexforge {

std::string token_kind_name(TokenKind kind) {
    switch (kind) {
        case TokenKind::Literal: return "literal";
        case TokenKind::LeftParen: return "'('";
        case TokenKind::RightParen: return "')'";
        case TokenKind::Star: return "'*'";
        case TokenKind::Pipe: return "'|'";
        case TokenKind::End: return "end of pattern";
    }
    return "unknown token";
}

Lexer::Lexer(std::string pattern) : pattern_(std::move(pattern)) {}

std::vector<Token> Lexer::tokenize() const {
    std::vector<Token> tokens;
    for (std::size_t i = 0; i < pattern_.size(); ++i) {
        const char c = pattern_[i];
        TokenKind kind = TokenKind::Literal;
        switch (c) {
            case '(': kind = TokenKind::LeftParen; break;
            case ')': kind = TokenKind::RightParen; break;
            case '*': kind = TokenKind::Star; break;
            case '|': kind = TokenKind::Pipe; break;
            default: break;
        }
        tokens.push_back(Token{kind, kind == TokenKind::Literal ? c : '\0', i});
    }
    tokens.push_back(Token{TokenKind::End, '\0', pattern_.size()});
    return tokens;
}

}  // namespace regexforge
