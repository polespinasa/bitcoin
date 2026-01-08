#!/usr/bin/env python3
# Copyright (c) 2024-present The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test for SetBumpFeeDiscount assertion failure (negative discount).

This test attempts to reproduce a race condition bug where the assertion
`discount >= 0` fails in SelectionResult::SetBumpFeeDiscount.

The bug occurs when mempool state changes between:
1. Individual bump fee calculation (stored in COutput::ancestor_bump_fees)
2. Combined bump fee calculation (via calculateCombinedBumpFee)

When combined_bump_fee > summed_bump_fees, the discount becomes negative,
triggering an assertion failure.

Bug location: src/wallet/coinselection.cpp:823
Trigger: src/wallet/spend.cpp:802

Root cause: Two separate MiniMiner objects are created (one for individual
bump fees at line 516, one for combined at line 798). If the mempool state
changes between these calls, the calculations can be inconsistent.

This test uses the debug option -test_coinselection_bump_fee_sleep to create
a window between the two calculations, then uses a background thread to
modify the mempool with prioritisetransaction during that window.
"""

import threading
import time

from test_framework.authproxy import AuthServiceProxy
from test_framework.blocktools import COINBASE_MATURITY
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal


# Sleep time in milliseconds for the debug option
DEBUG_SLEEP_MS = 1000


class WalletBumpFeeDiscountRaceTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.extra_args = [[
            "-walletrbf=1",
            "-fallbackfee=0.0001",  # 10 sat/vB fallback fee
            f"-test_coinselection_bump_fee_sleep={DEBUG_SLEEP_MS}",
        ]]

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        node = self.nodes[0]

        self.log.info("Setup: Mine blocks to mature coinbase")
        self.generate(node, COINBASE_MATURITY + 10)

        self.log.info("Create wallet for testing")
        node.createwallet("test_wallet")
        wallet = node.get_wallet_rpc("test_wallet")

        # Fund the test wallet with just ONE UTXO to force use of unconfirmed outputs
        default_wallet = node.get_wallet_rpc(self.default_wallet_name)
        funding_addr = wallet.getnewaddress()
        default_wallet.sendtoaddress(funding_addr, 10)
        self.generate(node, 1)

        self.log.info("Test: Race condition with debug sleep")
        self.test_race_with_sleep(node, wallet)

        wallet.unloadwallet()

    def test_race_with_sleep(self, node, wallet):
        """
        Test that triggers the race condition using the debug sleep.

        The debug sleep (-test_coinselection_bump_fee_sleep) creates a window
        between individual bump fee calculation and combined bump fee calculation.
        During this window, we use prioritisetransaction to modify the ancestor
        fees, which should cause combined_bump_fee > summed_bump_fees.
        """
        # First check what UTXOs we have
        utxos = wallet.listunspent()
        self.log.info(f"Starting with {len(utxos)} confirmed UTXOs")
        assert_equal(len(utxos), 1)

        self.log.info("Create unconfirmed parent transaction with low fee")
        # Send most of our funds, leaving just change
        # This ensures subsequent transactions MUST use unconfirmed outputs
        parent_addr = wallet.getnewaddress()
        parent_txid = wallet.sendtoaddress(
            address=parent_addr,
            amount=9,  # Send 9 of 10 BTC - forces next tx to use unconfirmed change
            fee_rate=1,  # Very low fee
        )
        self.log.info(f"Parent tx: {parent_txid}")

        # Check we now have only unconfirmed UTXOs
        utxos = wallet.listunspent(minconf=0)
        confirmed_utxos = wallet.listunspent(minconf=1)
        self.log.info(f"After parent: {len(utxos)} total UTXOs, {len(confirmed_utxos)} confirmed")

        self.log.info("Create child transaction spending parent's change/output")
        child_addr = wallet.getnewaddress()
        child_txid = wallet.sendtoaddress(
            address=child_addr,
            amount=8,  # Spend the unconfirmed funds
            fee_rate=1,  # Low fee
        )
        self.log.info(f"Child tx: {child_txid}")

        # Verify both transactions are in mempool and form a chain
        mempool = node.getrawmempool(verbose=True)
        assert parent_txid in mempool, "Parent tx should be in mempool"
        assert child_txid in mempool, "Child tx should be in mempool"

        # Verify child depends on parent
        child_entry = mempool[child_txid]
        self.log.info(f"Child depends on: {child_entry['depends']}")
        assert parent_txid in child_entry['depends'], "Child should depend on parent"

        self.log.info("Get initial mempool entry info")
        parent_entry = node.getmempoolentry(parent_txid)
        self.log.info(f"Parent fees: {parent_entry['fees']}")

        # Check available UTXOs - should all be unconfirmed
        utxos = wallet.listunspent(minconf=0)
        confirmed_utxos = wallet.listunspent(minconf=1)
        self.log.info(f"Before spend: {len(utxos)} total UTXOs, {len(confirmed_utxos)} confirmed")
        for u in utxos:
            self.log.info(f"  UTXO: {u['txid'][:8]}... amount={u['amount']} confs={u['confirmations']}")

        # Event to signal when prioritisation should happen
        prioritise_done = threading.Event()
        send_started = threading.Event()

        # Create a separate RPC connection for the background thread
        # (AuthServiceProxy is not thread-safe - it reuses HTTP connections)
        bg_node = AuthServiceProxy(node.url, timeout=60)

        def prioritise_during_sleep():
            """
            Wait for the sendtoaddress to start and hit the sleep, then call
            prioritisetransaction to modify the parent's effective fee.
            """
            # Wait for main thread to signal it started the send
            send_started.wait(timeout=5)
            # Give it a moment to enter the sleep
            time.sleep(0.1)
            self.log.info("Background: Calling prioritisetransaction to reduce parent fee")
            # Reduce the parent's effective fee significantly
            # This makes the combined bump fee calculation see a lower ancestor fee
            # than what was used for the individual calculation
            bg_node.prioritisetransaction(parent_txid, 0, -100000)
            self.log.info("Background: prioritisetransaction completed")
            prioritise_done.set()

        # Start the background thread
        bg_thread = threading.Thread(target=prioritise_during_sleep)
        bg_thread.start()

        self.log.info("Attempting to spend unconfirmed output (this triggers coin selection)")
        self.log.info(f"Node will sleep for {DEBUG_SLEEP_MS}ms during coin selection")

        dest_addr = wallet.getnewaddress()
        try:
            # Signal that we're starting the send
            send_started.set()
            # This sendtoaddress will:
            # 1. Calculate individual bump fees (with original parent fee)
            # 2. Sleep for DEBUG_SLEEP_MS (during which prioritisetransaction runs)
            # 3. Calculate combined bump fee (with reduced parent fee)
            # 4. Hit the assertion if combined > summed
            txid = wallet.sendtoaddress(
                address=dest_addr,
                amount=7,  # Spend the unconfirmed child output
                fee_rate=10,  # Higher fee to trigger bump fee calculation
            )
            self.log.info(f"Transaction succeeded: {txid}")
            # If we reach here, either:
            # - The bug is fixed (handles negative discount gracefully)
            # - The race didn't trigger for some reason
        except Exception as e:
            self.log.info(f"Exception: {e}")
            # If the node crashed due to assertion, we'll get a connection error
            raise

        # Wait for background thread to complete
        bg_thread.join(timeout=5)

        # Check that prioritisation actually happened
        assert prioritise_done.is_set(), "Background prioritisation should have completed"

        self.log.info("Test completed")


if __name__ == '__main__':
    WalletBumpFeeDiscountRaceTest(__file__).main()
