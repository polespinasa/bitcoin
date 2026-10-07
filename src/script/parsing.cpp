// Copyright (c) 2018-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <script/parsing.h>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace script {

bool Const(const std::string& str, std::span<const char>& sp, bool skip)
{
    if ((size_t)sp.size() >= str.size() && std::equal(str.begin(), str.end(), sp.begin())) {
        if (skip) sp = sp.subspan(str.size());
        return true;
    }
    return false;
}

bool Func(const std::string& str, std::span<const char>& sp)
{
    if ((size_t)sp.size() >= str.size() + 2 && sp[str.size()] == '(' && sp[sp.size() - 1] == ')' && std::equal(str.begin(), str.end(), sp.begin())) {
        sp = sp.subspan(str.size() + 1, sp.size() - str.size() - 2);
        return true;
    }
    return false;
}

std::optional<std::span<const char>> Bip39Expr(std::span<const char> sp)
{
    constexpr std::string_view token{"bip39("};
    if (sp.size() < token.size() || !std::equal(token.begin(), token.end(), sp.begin())) {
        return std::nullopt;
    }
    int depth = 1;
    bool in_quote = false;
    bool escaped = false;
    for (size_t i = token.size(); i < sp.size(); ++i) {
        const char ch = sp[i];
        if (in_quote) {
            // Within the quoted passphrase the escaped characters are literal
            // and an unescaped '"' terminates it; nothing else is meaningful.
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                in_quote = false;
            }
            continue;
        }
        switch (ch) {
            case '"':
                in_quote = true;
                break;
            case '(':
                ++depth;
                break;
            case ')':
                if (--depth == 0) return sp.first(i + 1);
                break;
            // Any other character (word list letters, ',', '[', ']', spaces)
            // is not meaningful for finding the matching ')'.
        }
    }
    // Unterminated quoted passphrase or missing matching ')'.
    return std::nullopt;
}

std::span<const char> Expr(std::span<const char>& sp)
{
    int level = 0;
    auto it = sp.begin();
    while (it != sp.end()) {
        // A "bip39(" key expression contains a word list and a quoted
        // passphrase that may hold characters which are metacharacters of the
        // surrounding grammar (including '(', ')', '{', '}' and ','). Upon
        // encountering it, skip the complete bip39() sub-expression and resume
        // the ordinary grammar after its matching closing braket ')'.
        if (*it == 'b') {
            if (auto bip39_expr = Bip39Expr(std::span<const char>{it, sp.end()})) {
                it += bip39_expr->size();
                continue;
            }
        }
        if (*it == '(' || *it == '{') {
            ++level;
        } else if (level && (*it == ')' || *it == '}')) {
            --level;
        } else if (level == 0 && (*it == ')' || *it == '}' || *it == ',')) {
            break;
        }
        ++it;
    }
    std::span<const char> ret = sp.first(it - sp.begin());
    sp = sp.subspan(it - sp.begin());
    return ret;
}

} // namespace script
