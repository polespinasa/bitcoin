// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/amount.h>
#include <policy/feerate.h>
#include <tinyformat.h>

#include <cmath>

CFeeRate::CFeeRate(const CAmount& nFeePaid, uint32_t num_bytes)
{
    assert(num_bytes <= static_cast<uint32_t>(std::numeric_limits<int32_t>::max()));
    const int32_t nSize = static_cast<int32_t>(num_bytes);

    if (nSize > 0) {
        nSatoshisPerV = FeePerVSize(nFeePaid, nSize);
    } else {
        nSatoshisPerV = FeePerVSize();
    }
}

CAmount CFeeRate::GetFee(uint32_t num_bytes) const
{
    assert(num_bytes <= static_cast<uint32_t>(std::numeric_limits<int32_t>::max()));
    const int32_t nSize = static_cast<int32_t>(num_bytes);

    if (nSatoshisPerV.IsEmpty()) { return CAmount(0);}
    if (nSize < 0 || nSatoshisPerV.size <= 0) { return CAmount(-1);}
    CAmount nFee = CAmount(nSatoshisPerV.EvaluateFeeUp(nSize));
    if (nFee == 0 && nSize != 0) { 
        if (nSatoshisPerV.EvaluateFeeUp(nSatoshisPerV.size) > 0) return CAmount(1);
        if (nSatoshisPerV.EvaluateFeeUp(nSatoshisPerV.size) < 0) return CAmount(-1);
    }
    return nFee;
}

std::string CFeeRate::ToString(const FeeEstimateMode& fee_estimate_mode) const
{
    const CAmount rate_per_kvb = nSatoshisPerV.fee * 1000 / nSatoshisPerV.size;

    switch (fee_estimate_mode) {
    case FeeEstimateMode::SAT_VB:
        return strprintf("%d.%03d %s/vB", rate_per_kvb / 1000, rate_per_kvb % 1000, CURRENCY_ATOM);
    default:
        return strprintf("%d.%08d %s/kvB", rate_per_kvb / COIN, rate_per_kvb % COIN, CURRENCY_UNIT);
    }
}

