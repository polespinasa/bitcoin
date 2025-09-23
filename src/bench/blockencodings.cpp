// Copyright (c) 2025-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>
#include <blockencodings.h>
#include <consensus/amount.h>
#include <kernel/cs_main.h>
#include <net_processing.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <sync.h>
#include <test/util/setup_common.h>
#include <test/util/txmempool.h>
#include <txmempool.h>
#include <util/check.h>

#include <memory>
#include <vector>


static void AddTx(const CTransactionRef& tx, const CAmount& fee, CTxMemPool& pool) EXCLUSIVE_LOCKS_REQUIRED(cs_main, pool.cs)
{
    LockPoints lp;
    AddToMempool(pool, CTxMemPoolEntry(tx, fee, /*time=*/0, /*entry_height=*/1, /*entry_sequence=*/0, /*spends_coinbase=*/false, /*sigops_cost=*/4, lp));
}

namespace {
class BenchCBHAST : public CBlockHeaderAndShortTxIDs
{
private:
    static CBlock DummyBlock()
    {
        CBlock block;
        block.nVersion = 5;
        block.hashPrevBlock.SetNull();
        block.hashMerkleRoot.SetNull();
        block.nTime = 1231006505;
        block.nBits = 0x1d00ffff;
        block.nNonce = 2083236893;
        block.fChecked = false;
        CMutableTransaction tx;
        tx.vin.resize(1);
        tx.vout.resize(1);
        block.vtx.emplace_back(MakeTransactionRef(tx)); // dummy coinbase
        return block;
    }

public:
    BenchCBHAST(FastRandomContext& rng, const std::vector<CTransactionRef> &txs)
    :  CBlockHeaderAndShortTxIDs(DummyBlock(), rng.rand64())
    {
        shorttxids.reserve(txs.size());
        for (const auto &tx : txs) {
            shorttxids.push_back(GetShortID(tx->GetWitnessHash()));
        }
    }
};
} // anon namespace

static std::vector<CTransactionRef> MakeTransactions(size_t count) {
    // bump up the size of txs
    std::array<std::byte,200> sigspam;
    sigspam.fill(std::byte(42));

    FastRandomContext rng{/*fDeterministic=*/false};

    std::vector<CTransactionRef> refs;
    refs.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        CMutableTransaction tx = CMutableTransaction();
        tx.vin.resize(1);
        tx.vin[0].scriptSig = CScript() << sigspam;
        tx.vin[0].scriptWitness.stack.push_back({1});
        tx.vout.resize(2);
        tx.vout[0].scriptPubKey = CScript() << OP_1 << OP_EQUAL;
        tx.vout[0].nValue = i;
        tx.vout[1].scriptPubKey = CScript() << OP_RETURN << rng.randbytes(80);
        tx.vout[1].nValue = 0;
        refs.push_back(MakeTransactionRef(tx));
    }

    // ensure mempool ordering is different to memory ordering of transactions,
    // to simulate a mempool that has changed over time
    std::shuffle(refs.begin(), refs.end(), rng);

    return refs;
}

static void BlockEncodingBench(benchmark::Bench& bench, size_t n_pool, size_t n_extra, size_t n_random_in_block, size_t n_pool_in_block = 0, size_t n_extra_in_block = 0)
{
    assert(n_pool >= n_pool_in_block && n_extra >= n_extra_in_block);
    const auto testing_setup = MakeNoLogFileContext<const ChainTestingSetup>(ChainType::MAIN);
    CTxMemPool& pool = *Assert(testing_setup->m_node.mempool);
    FastRandomContext rng(/*fDeterministic=*/false);

    LOCK2(cs_main, pool.cs);

    auto mempool_refs = MakeTransactions(n_pool);
    auto extra_refs = MakeTransactions(n_extra);
    auto random_refs = MakeTransactions(n_random_in_block);



    std::vector<CTransactionRef> refs_for_block;
    refs_for_block.reserve(n_pool_in_block + n_extra_in_block);
    for (size_t i = 0; i < n_pool_in_block; i++) {
        refs_for_block.push_back(mempool_refs[i]);
    }
    for (size_t i = 0; i < n_extra_in_block; i++) {
        refs_for_block.push_back(extra_refs[i]);
    }
    for (size_t i = 0; i < n_random_in_block; i++) {
        refs_for_block.push_back(random_refs[i]);
    }

    // Shuffle the mempool_refs and extra *after* inserting the transactions
    // into the cmpctblock, so that the top of the mempool is not identical to
    // the cmpctblock shorttxid's.
    std::shuffle(mempool_refs.begin(), mempool_refs.end(), rng);
    for (auto const &tx : mempool_refs) {
        AddTx(tx, /*fee=*/tx->vout[0].nValue, pool);
    }

    std::shuffle(extra_refs.begin(), extra_refs.end(), rng);
    // Insert extratxn refs into the extratxn vector
    std::vector<std::pair<Wtxid, CTransactionRef>> extratxn;
    extratxn.reserve(n_extra);
    for(auto const &tx : extra_refs) {
        extratxn.emplace_back(tx->GetWitnessHash(), tx);
    }

    std::unique_ptr<BenchCBHAST> cmpctblock;
    cmpctblock = std::make_unique<BenchCBHAST>(rng, refs_for_block);

    bench.unit("block").run([&] {
        PartiallyDownloadedBlock pdb{&pool};
        auto res = pdb.InitData(*cmpctblock, extratxn);

        // if there were duplicates the benchmark will be invalid
        // (eg, extra txns will be skipped) and we will receive
        // READ_STATUS_FAILED
        assert(res == READ_STATUS_OK);
    });
}

static void BlockEncodingOptimisticReconstruction(benchmark::Bench& bench)
{
    BlockEncodingBench(bench, /*n_pool=*/50'000, /*n_extra*/100, /*n_random_in_block=*/0, /*n_pool_in_block=*/7'000, /*n_extra_in_block=*/10);
}

static void BlockEncodingOptimisticReconstructionNoExtra(benchmark::Bench& bench)
{
    BlockEncodingBench(bench, /*n_pool=*/50'000, /*n_extra*/100, /*n_random_in_block=*/0, /*n_pool_in_block=*/7'000, /*n_extra_in_block=*/0);
}

// These three benchmarks have random shorttxid's, we will never find the txn's in our
// mempool/extra pool.
static void BlockEncodingNoExtra(benchmark::Bench& bench)
{
    BlockEncodingBench(bench, /*n_pool=*/50'000, /*n_extra=*/0, /*n_random_in_block=*/3'000);
}

static void BlockEncodingStdExtra(benchmark::Bench& bench)
{
    static_assert(DEFAULT_BLOCK_RECONSTRUCTION_EXTRA_TXN == 100);
    BlockEncodingBench(bench, /*n_pool=*/50'000, /*n_extra=*/100, /*n_random_in_block=*/3'000);
}

static void BlockEncodingLargeExtra(benchmark::Bench& bench)
{
    BlockEncodingBench(bench, /*n_pool=*/50'000, /*n_extra=*/5'000, /*n_random_in_block=*/3'000);
}

BENCHMARK(BlockEncodingOptimisticReconstruction, benchmark::PriorityLevel::HIGH);
BENCHMARK(BlockEncodingOptimisticReconstructionNoExtra, benchmark::PriorityLevel::HIGH);
BENCHMARK(BlockEncodingNoExtra, benchmark::PriorityLevel::HIGH);
BENCHMARK(BlockEncodingStdExtra, benchmark::PriorityLevel::HIGH);
BENCHMARK(BlockEncodingLargeExtra, benchmark::PriorityLevel::HIGH);
