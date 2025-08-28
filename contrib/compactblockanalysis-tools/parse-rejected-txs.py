#!/usr/bin/env python3
# Copyright (c) 2014-actual The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import re
import os
from collections import Counter

FILE_PATH="~/Escritorio/debug.log"

def inspect_file(file_path):
    interesting_data = {"mempool": [], "cmpctblock": []}
    file_path = os.path.expanduser(file_path)
    with open(file_path) as file:
        for line in file:
            if "[mempoolrej]" in line:
                interesting_data["mempool"].append(line)
            elif "[cmpctblock] Reconstructed block" in line:
                interesting_data["cmpctblock"].append(line)
    return interesting_data



def map_data(data):
    # Maps the data in the form of
    # {block1: {tx: reason, tx: reason...}, block2: {tx: reason, tx: reason...}}
    # The data we are parsing is in the following format
    # 2025-08-28T22:45:05Z [cmpctblock] Reconstructed block 0000000ed028eada29fc462f406fa783077948b0255206001091007b37853b62 required tx 6116f45769698bdf1fa07deebffc5eb17290db999111ba30a810ecfd45ebc45d
    #2025-08-28T23:01:08Z [mempoolrej] d92185c34f804047a9b58d5279ef0009f3bf06a4c9b32ef19544f7698498a8d7 (wtxid=d92185c34f804047a9b58d5279ef0009f3bf06a4c9b32ef19544f7698498a8d7) from peer=6 was not accepted: bad-txns-inputs-missingorspent

    rejection_info = {}
    for line in data["mempool"]:
        # The first hash in the line is the txid
        txid_match = re.search(r"\b[0-9a-f]{64}\b", line)
        wtxid_match = re.search(r"wtxid=([0-9a-f]{64})", line)
        reason_match = re.search(r"was not accepted:\s*(.+)$", line)

        txid = txid_match.group(0)
        wtxid = wtxid_match.group(1)
        reason = reason_match.group(1).strip()

        rejection_info[wtxid] = {"txid": txid, "reason": reason}

    # Step 2: Iterate through the block data, using the wtxid to find the real txid and reason.
    extracted_data = {}
    for line in data["cmpctblock"]:
        matches = re.findall(r"\b[0-9a-f]{64}\b", line)
        block_hash, required_wtxid = matches[0], matches[1]

        # Check if the wtxid required by the block is in our rejection table
        if required_wtxid in rejection_info:
            # If it is, retrieve the stored txid and reason
            final_txid = rejection_info[required_wtxid]["txid"]
            final_reason = rejection_info[required_wtxid]["reason"]

            # Add the data to our final result, keyed by the BLOCK HASH and the TXID
            if block_hash not in extracted_data:
                extracted_data[block_hash] = {}
            extracted_data[block_hash][final_txid] = final_reason

            # LOOK FOR TXS WITH AN SPECIFIC REASON
            '''
            if extracted_data[block_hash][final_txid] == 'bad-txns-inputs-missingorspent':
                print(final_txid)
            '''
    return extracted_data

def group_reason(reason):
    GROUPING_PREFIXES = [
        "insufficient fee",
        "too-long-mempool-chain",
        "replacement-adds-unconfirmed",
        "too many potential replacements",
        "min relay fee not met"        

    ]

    for prefix in GROUPING_PREFIXES:
        if reason.startswith(prefix):
            return prefix

    # If no specific prefix matches, return the original reason
    return reason

def main():
    data = inspect_file(FILE_PATH)

    cmpctblock_lines = data.get("cmpctblock", [])
    if cmpctblock_lines:
        first_line = cmpctblock_lines[0]
        last_line = cmpctblock_lines[-1]

        first_match = re.search(r"\b([0-9a-f]{64})\b", first_line)
        last_match = re.search(r"\b([0-9a-f]{64})\b", last_line)

        if first_match:
            print(f"First block in log: {first_match.group(1)}")
        if last_match:
            print(f"Last block in log:  {last_match.group(1)}")
        print("-" * 40)

    extracted_data = map_data(data)

    all_reasons = (
        group_reason(reason)
        for block_data in extracted_data.values()
        for reason in block_data.values()
    )

    reason_counts = Counter(all_reasons)

    print("--- Transaction Reason Counts (Grouped) ---")
    if not reason_counts:
        print("No transaction data found to count.")
        return

    for reason, count in reason_counts.most_common():
        print(f"{reason}: {count}")

if __name__ == "__main__":
    main()
