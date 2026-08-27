#!/usr/bin/env python3
# Copyright (c) 2024-present The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test sendall's silent broadcast soft-failure at the -maxfeerate boundary.

This is a characterization test of a *known bug*.  It asserts the current bad
behaviour so that the test PASSES today and will FAIL once the bug is fixed,
acting as a regression detector for the issue.

Bug summary: sendall's creation-time -maxfeerate check ignores bump fees for
unconfirmed inputs, while the broadcast-time check in BroadcastTransaction
includes them.  When fee_rate is set exactly equal to -maxfeerate and the
wallet spends unconfirmed inputs with positive bump fees, the creation check
passes (equality, ignoring bump fees) but the broadcast check fails (actual
fee > max_rate.GetFee(actual_vsize)).  The error is silently swallowed by
CommitTransaction, so sendall returns {"complete": true, "txid": ...} for a
transaction that never entered the mempool.

Root-cause chain:
  1. sendall (spend.cpp:1526): checks ``fee_from_size > max_rate.GetFee(E)``
     where fee_from_size = fee_rate.GetFee(estimated_vsize).  Bump fees are
     **excluded** from this check even though they are baked into the outputs.
  2. With fee_rate == max_rate:  fee_from_size == max_rate.GetFee(E), so the
     check is ``X > X`` → false → passes.
  3. BroadcastTransaction (transaction.cpp:81): checks
     ``actual_fee > max_rate.GetFee(actual_vsize)`` where actual_fee =
     fee_from_size + bump_fees.
  4. With E == A (exact estimate for descriptor wallets that can grind R):
     ``(fee_from_size + bump_fees) > fee_from_size`` → true → FAILS.
  5. BroadcastTransaction returns TransactionError::MAX_FEE_RATE_EXCEEDED,
     demoted to bool false at interfaces.cpp:690, swallowed by
     CommitTransaction (wallet.cpp:2364-2367) which only logs a warning.
  6. FinishTransaction returns {complete: true, txid: ...} unaware of the
     failure.  The transaction is stuck in the wallet as inactive forever.

When the bug is fixed (either by including bump fees in the creation check or
by propagating the BroadcastTransaction error), sendall will either refuse to
create the transaction or report the failure.  In both cases the assertions
below will start to FAIL, which is the desired outcome: it tells the developer
to update this test to reflect the corrected behaviour.
"""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal


class SendallSilentBroadcastFailureTest(BitcoinTestFramework):
    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        # Set -maxfeerate to 10 sat/vB (0.00010 BTC/kvB) so the bug can be
        # triggered with fee_rate=10 while keeping the absolute fee well below
        # the m_max_tx_fee limit (0.001 BTC).
        self.extra_args = [["-maxfeerate=0.00010"]]

    def run_test(self):
        self.log.info("Test sendall silent broadcast failure at -maxfeerate boundary")

        def_wallet = self.nodes[0].get_wallet_rpc(self.default_wallet_name)
        self.nodes[0].createwallet("sender")
        sender = self.nodes[0].get_wallet_rpc("sender")

        # Mature coinbase outputs in the default wallet.
        self.generate(self.nodes[0], 101)

        # Create an unconfirmed UTXO in the sender wallet with a low feerate
        # (1 sat/vB).  The parent's feerate is below the sendall fee_rate
        # (10 sat/vB), so calculateCombinedBumpFee will return a positive
        # bump fee — this is the key ingredient that triggers the bug.
        def_wallet.sendtoaddress(address=sender.getnewaddress(), amount=10, fee_rate=1)
        sender.syncwithvalidationinterfacequeue()
        assert_equal(sender.getbalances()["mine"]["untrusted_pending"], 10)
        unconfirmed_utxo = sender.listunspent(minconf=0)[0]

        # Call sendall with fee_rate exactly at the -maxfeerate boundary.
        #
        # Creation-time check (spend.cpp:1526):
        #   fee_from_size = 10 * estimated_vsize
        #   10*E > 10*E  →  false  →  PASSES  (bump fees ignored)
        #
        # Broadcast-time check (transaction.cpp:81):
        #   actual_fee = 10*E + bump_fees
        #   (10*E + bump_fees) > 10*A  →  true  →  FAILS  (E == A, bump_fees > 0)
        #
        # CommitTransaction swallows the failure → sendall reports success.
        self.log.info("Calling sendall with fee_rate=10 (at -maxfeerate boundary)...")
        result = sender.sendall(
            recipients=[def_wallet.getnewaddress()],
            inputs=[unconfirmed_utxo],
            fee_rate=10,
        )

        # --- Assertions for the known-bad behaviour (the bug) ---------------
        # These pass while the bug exists and will FAIL once it is fixed.
        txid = result["txid"]

        # Symptom 1: sendall falsely reports success.
        assert_equal(result["complete"], True)
        self.log.info(f"sendall returned complete=True, txid={txid}")

        # Symptom 2: the transaction never made it into the mempool despite the
        # success report above.
        mempool = self.nodes[0].getrawmempool()
        assert txid not in mempool, (
            "The bug appears to be fixed: the sendall transaction IS in the "
            "mempool. Update this test to assert the corrected behaviour."
        )
        self.log.info(
            "Confirmed: sendall returned complete=True but the transaction is "
            "NOT in the mempool (silent broadcast failure reproduced)."
        )

        # Symptom 3: the transaction is stuck in the wallet as inactive.  It is
        # neither confirmed nor in the mempool; resending will keep failing.
        tx_info = sender.gettransaction(txid)
        assert_equal(tx_info["confirmations"], 0)
        self.log.info(
            "Confirmed: the transaction is stuck in the wallet (0 confirmations, "
            "not in mempool) — it will be retried and fail silently forever."
        )


if __name__ == '__main__':
    SendallSilentBroadcastFailureTest(__file__).main()
