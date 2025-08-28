#!/usr/bin/env python3
# Copyright (c) 2014-actual The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import re

FILE_PATH="~/.bitcoin/debug.log"

def inspect_file(file_path):
	interesting_data = {"mempool": [], "cmpctblock": []}
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
	
	extracted_data = {}
	for line in data["cmpctblock"]:
		matches = re.findall(r"\b[0-9a-f]{64}\b", line)
		block, wtxid = matches[0], matches[1]
		extracted_data[block] = {wtxid: "no-reason"}

	txns = {}
	for line in data["mempoolrej"]:
		wtxid_match = re.search(r"wtxid=([0-9a-f]{64})", line)
		wtxid = wtxid_match.group(1)

		reason_match = re.search(r"was not accepted:\s*(.+)$", line)
		reason = reason_match.group(1) if reason_match else None
		txns[wtxid] = reason

	for block, block_dict in extracted_data.items():
		for wtxid in block_dict.keys() & txns.keys():
			block_dict[wtxid] = txns[wtxid]

	return extracted_data


def main():
	data = inspect_file(FILE_PATH)
	extracted_data = map_data(data)

	print(extracted_data)