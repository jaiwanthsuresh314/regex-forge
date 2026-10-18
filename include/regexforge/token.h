#pragma once

#include <cstddef>
#include <string>

namespace regexforge {

enum class TokenKind { Literal, LeftParen, RightParen, Star, Pipe, End };

struct Token {
    TokenKind kind;
    char value{};
    std::size_t position{};
};

std::string token_kind_name(TokenKind kind);

}  // namespace regexforge
