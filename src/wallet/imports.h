// Copyright (c) 2026-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_IMPORTS_H
#define BITCOIN_WALLET_IMPORTS_H

#include <wallet/wallet.h>

namespace wallet {

struct ImportDescriptorResult {
    enum class ImportResultCode {
        OK,
        INVALID_DESCRIPTOR,
        INVALID_PARAMETER,
        WALLET_ERROR,
        WALLET_UNLOCK_NEEDED,
        MISC_ERROR
    };
    ImportResultCode result_code{ImportResultCode::OK};
    std::string error;
    std::vector<std::string> warnings;
    //! Set to true when a wallet-wide precondition failed before any descriptor was
    //! processed (e.g. wallet is already rescanning, or wallet is locked).
    //! Callers that support top-level errors should surface this as a
    //! top-level / call-wide error rather than a per-descriptor failure.
    bool global_error{false};
    ImportDescriptorResult& Error(ImportResultCode r, std::string e, bool g_e = false) {
        result_code = r;
        error = std::move(e);
        global_error = g_e;
        return *this;
    }
};
//! Information about a descriptor to be imported.
struct ImportDescriptorRequest {
    std::string descriptor;
    std::optional<std::string> label;
    std::optional<int64_t> timestamp;
    bool active{false};
    std::optional<bool> internal;
    std::optional<std::pair<int64_t,int64_t>> range;
    std::optional<int64_t> next_index;
};

ImportDescriptorResult ImportDescriptor(CWallet& wallet,
    ImportDescriptorRequest request,
    int64_t timestamp) EXCLUSIVE_LOCKS_REQUIRED(wallet.cs_wallet);

std::vector<ImportDescriptorResult> ProcessDescriptorsImport(CWallet& wallet,
    std::vector<ImportDescriptorRequest> requests);

} // namespace wallet

#endif // BITCOIN_WALLET_IMPORTS_H
