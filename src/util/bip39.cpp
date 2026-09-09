// Copyright (c) 2026-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <crypto/sha256.h>

#include <cstdint>
#include <span>

namespace {

bool IsValidChecksum(std::span<const uint16_t> indexes)
{
    const size_t total_bits{indexes.size() * 11};
    const size_t checksum_bits{total_bits / 33};

    CSHA256 hasher;
    unsigned char byte{0};
    unsigned int claimed_checksum{0};
    size_t entropy_bits_left{total_bits - checksum_bits};
    size_t bits_in_byte{0};
    for (const uint16_t index : indexes) {
        for (int i{10}; i >= 0; --i) {
            // bit i from current index.
            const unsigned int bit{(index >> i) & 1u};
            if (entropy_bits_left > 0) {
                // Store the bit at its position inside a byte,
                // as bytes are what CSHA256 consumes.
                byte |= static_cast<unsigned char>(bit << (8 - ++bits_in_byte));
                entropy_bits_left--;
                if (bits_in_byte == 8) { // this bit completed a byte
                    hasher.Write(&byte, 1);
                    byte = 0;
                    bits_in_byte = 0;
                }
            } else {
                // The extra bits are the expected checksum.
                claimed_checksum = (claimed_checksum << 1) | bit;
            }
        }
    }

    unsigned char hash[CSHA256::OUTPUT_SIZE];
    hasher.Finalize(hash);
    return claimed_checksum == static_cast<unsigned int>(hash[0]) >> (8 - checksum_bits);
}

} // anonymous namespace
