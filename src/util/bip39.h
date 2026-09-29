// Copyright (c) 2026-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_UTIL_BIP39_H
#define BITCOIN_UTIL_BIP39_H

#include <support/allocators/secure.h>
#include <util/expected.h>

#include <cstddef>
#include <span>
#include <string>
#include <vector>

//! Derive the BIP 39 binary seed for a mnemonic (given as its words) and a
//! passphrase, after validating the word count, the words themselves and the
//! mnemonic checksum. Both the mnemonic sentence and the passphrase are
//! NFKD-normalized as BIP 39 requires before the PBKDF2 round.
util::Expected<std::vector<std::byte, secure_allocator<std::byte>>, std::string> FromMnemonicToSeed(std::span<const std::string> words, const std::string& passphrase);

#endif // BITCOIN_UTIL_BIP39_H
