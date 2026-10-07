// Copyright (c) 2018-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SCRIPT_PARSING_H
#define BITCOIN_SCRIPT_PARSING_H

#include <optional>
#include <span>
#include <string>

namespace script {

/** Parse a constant.
 *
 * If sp's initial part matches str, sp is optionally updated to skip that part, and true is returned.
 * Otherwise sp is unmodified and false is returned.
 */
bool Const(const std::string& str, std::span<const char>& sp, bool skip = true);

/** Parse a function call.
 *
 * If sp's initial part matches str + "(", and sp ends with ")", sp is updated to be the
 * section between the braces, and true is returned. Otherwise sp is unmodified and false
 * is returned.
 */
bool Func(const std::string& str, std::span<const char>& sp);

/** Determine the extent of a "bip39(" key expression.
 *
 * If sp begins with the token "bip39(", return the complete expression,
 * extending up to and including the ')' matching the one in the token.
 *
 * If sp does not begin with "bip39(", or the passphrase is unterminated, or no
 * matching ')' is found, returns std::nullopt, in which case the caller should
 * interpret sp with the ordinary grammar.
 */
std::optional<std::span<const char>> Bip39Expr(std::span<const char> sp);

/** Extract the expression that sp begins with.
 *
 * This function will return the initial part of sp, up to (but not including) the first
 * comma or closing brace, skipping ones that are surrounded by braces. So for example,
 * for "foo(bar(1),2),3" the initial part "foo(bar(1),2)" will be returned. sp will be
 * updated to skip the initial part that is returned.
 */
std::span<const char> Expr(std::span<const char>& sp);

} // namespace script

#endif // BITCOIN_SCRIPT_PARSING_H
