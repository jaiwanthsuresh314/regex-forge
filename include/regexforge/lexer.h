#pragma once

#include "regexforge/token.h"

#include <string>
#include <vector>

namespace regexforge {

class Lexer {
public:
    explicit Lexer(std::string pattern);
    std::vector<Token> tokenize() const;

private:
    std::string pattern_;
};

}  // namespace regexforge
