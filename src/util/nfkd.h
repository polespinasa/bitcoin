// Copyright (c) 2026-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_UTIL_NFKD_H
#define BITCOIN_UTIL_NFKD_H

#include <string>
#include <string_view>

//! Return the Unicode Normalization Form Compatibility Decomposition (NFKD)
//! of the UTF-8 encoded string utf8, as required by BIP 39: every character
//! is replaced by its recursive canonical or compatibility decomposition and
//! the combining marks in the result are ordered by canonical combining
//! class. If utf8 is not valid UTF-8 it is returned unchanged.
std::string NFKD(std::string_view utf8);

#endif // BITCOIN_UTIL_NFKD_H
