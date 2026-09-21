#!/usr/bin/env python3
# Copyright (c) 2026-present The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Test the bip39() key expression in output descriptors.

A bip39() key expression takes a BIP 39 English mnemonic word list and an
optional passphrase and resolves to the BIP 32 master extended private key
derived from them. This test checks that:
"""

from test_framework.descriptors import descsum_create
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error

# BIP 39 mnemonics:
MNEMONIC_12 = "legal, winner, thank, year, wave, sausage, worth, useful, legal, winner, thank, yellow"
MNEMONIC_24 = (
    "fancy, behave, cement, feel, gas, super, dutch, juice, cream, tape, bronze, increase, "
    "minor, meadow, rescue, someone, banana, recipe, orient, copy, blossom, team, chase, initial"
)
# Invalid mnemonics:
MNEMONIC_11 = "legal, winner, thank, year, wave, sausage, worth, useful, legal, winner, thank"
MNEMONIC_UNKNOWN_WORD = "legal, winner, thank, year, wave, sausage, worth, useful, legal, winner, thank, notaword"
MNEMONIC_BAD_CHECKSUM = "legal, wonder, thank, year, wave, sausage, worth, useful, legal, winner, thank, yellow"

# Master extended key derived from MNEMONIC_12 with passphrase "SATOSHI"
# (tprv: the BIP 32 testnet/regtest serialization of the draft's xprv):
TPRV_SATOSHI = "tprv8ZgxMBicQKsPeKwQuxVjw73PuY2xfjPcEihBaNhKYxqDzf7DaWzwLFaeJQYTWCWNJHt74mAhrg9CMXopedMsbVsXoWKSvH87YLCRmVuTMp4"
TPUB_SATOSHI = "tpubD6NzVbkrYhZ4XnyCocALLWhWUZYtq4aWp2HxrtjcyEdcq9MzCupXWkCWUZ5mFTKKXtXjtaYWDnjgjpgRQHhpBpGniA5V6dFyqwJ5kXqJZLX"
# Master extended key derived from MNEMONIC_24 with an empty passphrase:
TPRV_NOPASS = "tprv8ZgxMBicQKsPdEn9m2u4ZbBo4bTMsuWpccLui4JG53EjKjB73bcFcM1pMxdcEq5Z3KCeLmWV3VkipwYFSuqtxm87bb3YW22iHVF6oep66Pm"
TPUB_NOPASS = "tpubD6NzVbkrYhZ4WhowegZexzqudcyJ3EhjBuwgzaLZVK38ADRsfzRqnqdgY7kF2JL6pxyGCGuVpfkU3wJAytMQ1TMzhpYR1MvpPwv51qhT6k9"
# Master extended key derived from MNEMONIC_12 with passphrase "caf\u{e9}" (café)
# BIP 39 NFKD-normalizes the passphrase before hashing, so the precomposed
# (U+00E9) and combining (e + U+0301) spellings below derive the same keys.
PASSPHRASE_CAFE_ESCAPED = "caf\\u{e9}"
TPRV_CAFE = "tprv8ZgxMBicQKsPdvMdDpoqh1acivVhJwtTBmReogYwJnkG1KZxxLx69APUEForK2xVcRx3gqVvTmWms8QPaggQFQey3LEPdrdohx3mLfsPKHs"
TPUB_CAFE = "tpubD6NzVbkrYhZ4XPPR7UUS6REjHx1dUH5Mm52S6CbEj4YeqopjajmgKf1LQRzCqG8f4Wjay4bPr6hf2zHNhqQUQPafj5YQxDQgdhoH6zdNPDL"
# Same passphrase text spelled with the combining acute accent (U+0301):
# different raw bytes, but NFKD-normalizes to the same string as "caf\u{e9}".
PASSPHRASE_COMBINING_ESCAPED = "cafe\\u{301}"
# Master extended key derived from MNEMONIC_12 with passphrase "foo#bar"
# The '#' must be escaped as \u{23}, otherwise it terminates the descriptor
# checksum.
PASSPHRASE_HASH_ESCAPED = "foo\\u{23}bar"
TPRV_HASH = "tprv8ZgxMBicQKsPfRG4NCfqRCS2yoKdd6qpdWf4WWEyAVmaXmac1zjTvNkibLvWc3NLs968eZ6PcuMaDWUZ8A4KAyhVDePEkBLRnXUNaJin5Yw"
TPUB_HASH = "tpubD6NzVbkrYhZ4YtHrFrLRpc69YpqZnS2jCpFqo2HGamZyNFqNePZ46sNamVVC3azn4oqZzFj69qEULroECLzABC4m1yt4mNNpp5jEDL9YD7V"
# Official BIP 39 test vector (trezor/python-mnemonic vectors.json, the file
# linked from the BIP): the MNEMONIC_12 words with the passphrase "TREZOR"
# derive the seed 2e8905819b8723fe2c1d161860e5ee1830318dbf49a83bd451cfb8440c28bd6f
# a457fe1296106559a3c80937a1c1069be3a3a5bd381ee6260e8d9739fce1f607.
PASSPHRASE_TREZOR = "TREZOR"
TPRV_TREZOR = "tprv8ZgxMBicQKsPdVPegAPkTV65T6QDr34Aj8MwxKJYqDvpS8XHJrMB1DgL4Rg2TyFBERBY4fEWXVKroiZ2E1HajGjXK8LHttb5Cn95Sb7WxK3"
TPUB_TREZOR = "tpubD6NzVbkrYhZ4WxRSZp4LrtkC27vA1NF5JRxjEqLrFVjDGcn3wFAmBiJCEYNDjfd4YuXrwEh2MK7gwKD7rUe3ohMb7pHgs83R6niDEJFscst"

# Account-level (m/84'/0'/0') extended public key and master key fingerprint
# of TPRV_SATOSHI, as they appear in watch-only exported descriptors:
ACCOUNT_FINGERPRINT = "cc432ad6"
ACCOUNT_TPUB = "tpubDDXUEorf56wdFNvC6rDWqcHwyBH2kn3D2d9W2cK7sX7rynSUEa2hhKtVfmw4qwh3Fs5EwWk5fgpGwv9EVkFACAaqza6zEcr47oH2ykBnnzT"

# Addresses at m/84'/0'/0'/0/{0..3} and m/84'/0'/0'/1/{0,1} for the
# TPRV_SATOSHI vector, independently computed in Python:
EXTERNAL_ADDRESSES = [
    "bcrt1qsxrqxjmf45lks9qc6v9d3fnffc9srkvqy4yeye",
    "bcrt1qzu5v02569fpfann52xzh0ceag5avl069p5hy7m",
    "bcrt1qxn5axjp6fs3hxu2626hyqlu8377wp9xtyaz5c5",
    "bcrt1qwnerkpx8ryeutx45cn76zp2pr4m2mradtj3p5n",
]
CHANGE_ADDRESSES = [
    "bcrt1q9cnrwevcec3gg3jqwcpzpmhp5m4mt0ne9p24zv",
    "bcrt1qna8zavfc4mewzgrsecka8yrms6aet9yjh0qza4",
]


def bip39_key_expr(mnemonic=MNEMONIC_12, passphrase=None, path=""):
    """Build a "bip39(...)" key expression with an optional derivation path."""
    passphrase_part = f', "{passphrase}"' if passphrase is not None else ""
    return f"bip39([{mnemonic}]{passphrase_part}){path}"


class WalletBip39DescriptorTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.wallet_names = []

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def test_descriptor_analysis(self):
        node = self.nodes[0]
        self.log.info("Test that getdescriptorinfo resolves bip39() expressions")
        vectors = [
            # (bip39 descriptor, equivalent tprv descriptor, canonical public descriptor, isrange)
            (f"wpkh({bip39_key_expr(passphrase='SATOSHI')})", f"wpkh({TPRV_SATOSHI})", f"wpkh({TPUB_SATOSHI})", False),
            # Official BIP 39 test vector: the "TREZOR" passphrase is used
            # by the vectors in the trezor/python-mnemonic vectors.json file
            # linked from the BIP.
            (f"wpkh({bip39_key_expr(passphrase=PASSPHRASE_TREZOR)})", f"wpkh({TPRV_TREZOR})", f"wpkh({TPUB_TREZOR})", False),
            (
                f"wpkh({bip39_key_expr(passphrase='SATOSHI', path='/84h/0h/0h/0/*')})",
                f"wpkh({TPRV_SATOSHI}/84h/0h/0h/0/*)",
                f"wpkh({TPUB_SATOSHI}/84h/0h/0h/0/*)",
                True,
            ),
            (
                f"wpkh({bip39_key_expr(mnemonic=MNEMONIC_24, path='/84h/0h/0h/<0;1>/*')})",
                f"wpkh({TPRV_NOPASS}/84h/0h/0h/<0;1>/*)",
                f"wpkh({TPUB_NOPASS}/84h/0h/0h/0/*)",
                True,
            ),
        ]
        for bip39_desc, xprv_desc, canonical, isrange in vectors:
            info = node.getdescriptorinfo(descsum_create(bip39_desc))
            # bip39() resolves to the same public-only descriptor as the
            # equivalent extended private key expression.
            assert_equal(info["descriptor"], descsum_create(canonical))
            assert_equal(info["descriptor"], node.getdescriptorinfo(descsum_create(xprv_desc))["descriptor"])
            assert_equal(info["isrange"], isrange)
            assert_equal(info["issolvable"], True)
            assert_equal(info["hasprivatekeys"], True)

        self.log.info("Test multipath expansion of bip39() expressions")
        info = node.getdescriptorinfo(descsum_create(f"wpkh({bip39_key_expr(passphrase='SATOSHI', path='/84h/0h/0h/<0;1>/*')})"))
        assert_equal(info["multipath_expansion"], [descsum_create(f"wpkh({TPUB_SATOSHI}/84h/0h/0h/{i}/*)") for i in (0, 1)])

        self.log.info("Test bip39() passphrase escaping")
        # "caf\u{e9}" and "cafe\u{301}" decode to different raw byte
        # sequences, but BIP 39 NFKD-normalizes the passphrase before
        # hashing, so both spellings derive the same keys.
        info_cafe = node.getdescriptorinfo(descsum_create(f"wpkh({bip39_key_expr(passphrase=PASSPHRASE_CAFE_ESCAPED)})"))
        assert_equal(info_cafe["descriptor"], descsum_create(f"wpkh({TPUB_CAFE})"))
        info_combining = node.getdescriptorinfo(descsum_create(f"wpkh({bip39_key_expr(passphrase=PASSPHRASE_COMBINING_ESCAPED)})"))
        assert_equal(info_combining["descriptor"], descsum_create(f"wpkh({TPUB_CAFE})"))
        assert_equal(info_cafe["descriptor"], info_combining["descriptor"])
        # A raw '#' would terminate the descriptor checksum early and must be
        # escaped as \u{23}.
        info_hash = node.getdescriptorinfo(descsum_create(f"wpkh({bip39_key_expr(passphrase=PASSPHRASE_HASH_ESCAPED)})"))
        assert_equal(info_hash["descriptor"], descsum_create(f"wpkh({TPUB_HASH})"))
        assert_equal(node.getdescriptorinfo(descsum_create(f"wpkh({TPRV_HASH})"))["descriptor"], info_hash["descriptor"])

        self.log.info("Test that invalid bip39() descriptors are rejected")
        errors = [
            (
                f"wpkh({bip39_key_expr(mnemonic=MNEMONIC_11, passphrase='SATOSHI')})",
                "wpkh(): bip39(): Invalid mnemonic: expected 12, 15, 18, 21, or 24 words",
            ),
            (
                f"wpkh({bip39_key_expr(mnemonic=MNEMONIC_UNKNOWN_WORD)})",
                'wpkh(): bip39(): Invalid mnemonic: unknown word "notaword"',
            ),
            (
                f"wpkh({bip39_key_expr(mnemonic=MNEMONIC_BAD_CHECKSUM)})",
                "wpkh(): bip39(): Invalid mnemonic: checksum mismatch",
            ),
            # A key origin is not allowed before bip39(); the origin parser
            # trips over the mnemonic's closing bracket.
            (
                f"wpkh([d34db33f]{bip39_key_expr()})",
                "wpkh(): Multiple ']' characters found for a single pubkey",
            ),
        ]
        for desc, message in errors:
            assert_raises_rpc_error(-5, message, node.getdescriptorinfo, descsum_create(desc))
        # A raw '#' in the passphrase is not part of the word list or
        # passphrase grammar: it terminates the descriptor checksum, which
        # fails before the descriptor itself is parsed.
        raw_hash_desc = f'wpkh({bip39_key_expr(passphrase="foo#bar")})'
        assert_raises_rpc_error(-5, "Expected 8 character checksum, not 6 characters", node.getdescriptorinfo, raw_hash_desc)

    def test_deriveaddresses(self):
        node = self.nodes[0]
        self.log.info("Test address derivation of bip39() descriptors")
        assert_equal(
            node.deriveaddresses(descsum_create(f"wpkh({bip39_key_expr(passphrase='SATOSHI', path='/84h/0h/0h/0/*')})"), [0, 3]),
            EXTERNAL_ADDRESSES,
        )
        assert_equal(
            node.deriveaddresses(descsum_create(f"wpkh({bip39_key_expr(passphrase='SATOSHI', path='/84h/0h/0h/1/*')})"), [0, 1]),
            CHANGE_ADDRESSES,
        )

    def test_wallet_import_and_signing(self):
        node = self.nodes[0]
        self.log.info("Import bip39() and equivalent tprv descriptors into wallets")
        node.createwallet(wallet_name="bip39_wallet", blank=True)
        node.createwallet(wallet_name="xprv_wallet", blank=True)
        w_bip39 = node.get_wallet_rpc("bip39_wallet")
        w_xprv = node.get_wallet_rpc("xprv_wallet")

        multipath = "/84h/0h/0h/<0;1>/*"
        for wallet, desc in [
            (w_bip39, descsum_create(f"wpkh({bip39_key_expr(passphrase='SATOSHI', path=multipath)})")),
            (w_xprv, descsum_create(f"wpkh({TPRV_SATOSHI}{multipath})")),
        ]:
            result = wallet.importdescriptors([{"desc": desc, "active": True, "timestamp": "now", "range": 10}])
            assert_equal(result[0]["success"], True)

        self.log.info("Test that bip39() descriptors are normalized to xpub on export")
        expected = sorted(
            [
                (descsum_create(f"wpkh([{ACCOUNT_FINGERPRINT}/84h/0h/0h]{ACCOUNT_TPUB}/0/*)"), False),
                (descsum_create(f"wpkh([{ACCOUNT_FINGERPRINT}/84h/0h/0h]{ACCOUNT_TPUB}/1/*)"), True),
            ]
        )
        for wallet in (w_bip39, w_xprv):
            descriptors = wallet.listdescriptors()["descriptors"]
            assert_equal(len(descriptors), 2)
            assert all("bip39(" not in d["desc"] for d in descriptors)
            assert_equal(sorted((d["desc"], d["internal"]) for d in descriptors), expected)

        self.log.info("Test that the wallet resolved the mnemonic to the master key")
        hdkeys = w_bip39.gethdkeys(private=True)
        assert_equal([k["xpub"] for k in hdkeys], [TPUB_SATOSHI])
        assert all(k["has_private"] for k in hdkeys)

        self.log.info("Test address parity between the bip39() and tprv wallets")
        for _ in range(5):
            assert_equal(w_bip39.getnewaddress(address_type="bech32"), w_xprv.getnewaddress(address_type="bech32"))
            assert_equal(w_bip39.getrawchangeaddress(address_type="bech32"), w_xprv.getrawchangeaddress(address_type="bech32"))

        self.log.info("Test that the bip39() wallet can receive and sign transactions")
        addr = w_bip39.getnewaddress(address_type="bech32")
        self.generatetoaddress(node, 101, addr)  # 50 BTC, matured
        dest = w_bip39.getnewaddress(address_type="bech32")
        txid = w_bip39.sendtoaddress(address=dest, amount=10, fee_rate=2)
        rawtx = node.getrawtransaction(txid, verbose=True)
        # The input is signed with a key derived from the mnemonic:
        assert_equal(len(rawtx["vin"][0]["txinwitness"]), 2)
        self.generatetoaddress(node, 1, addr)
        assert_equal(w_bip39.getbalance(), w_xprv.getbalance())

    def test_import_errors(self):
        node = self.nodes[0]
        self.log.info("Test that invalid bip39() descriptors are rejected by importdescriptors")
        node.createwallet(wallet_name="bip39_errors", blank=True)
        wallet = node.get_wallet_rpc("bip39_errors")
        errors = [
            (
                f"wpkh({bip39_key_expr(mnemonic=MNEMONIC_11, passphrase='SATOSHI')})",
                "wpkh(): bip39(): Invalid mnemonic: expected 12, 15, 18, 21, or 24 words",
            ),
            (
                f"wpkh({bip39_key_expr(mnemonic=MNEMONIC_UNKNOWN_WORD)})",
                'wpkh(): bip39(): Invalid mnemonic: unknown word "notaword"',
            ),
            (
                f"wpkh({bip39_key_expr(mnemonic=MNEMONIC_BAD_CHECKSUM)})",
                "wpkh(): bip39(): Invalid mnemonic: checksum mismatch",
            ),
            (
                f"wpkh([d34db33f]{bip39_key_expr()})",
                "wpkh(): Multiple ']' characters found for a single pubkey",
            ),
        ]
        for desc, message in errors:
            result = wallet.importdescriptors([{"desc": descsum_create(desc), "timestamp": "now"}])
            assert_equal(result[0]["success"], False)
            assert_equal(result[0]["error"]["code"], -5)
            assert_equal(result[0]["error"]["message"], message)

        result = wallet.importdescriptors([{"desc": f'wpkh({bip39_key_expr(passphrase="foo#bar")})', "timestamp": "now"}])
        assert_equal(result[0]["success"], False)
        assert_equal(result[0]["error"]["code"], -5)
        assert_equal(result[0]["error"]["message"], "Expected 8 character checksum, not 6 characters")

        self.log.info("Test that bip39() descriptors cannot be imported into a watch-only wallet")
        node.createwallet(wallet_name="bip39_watchonly", blank=True, disable_private_keys=True)
        w_watchonly = node.get_wallet_rpc("bip39_watchonly")
        result = w_watchonly.importdescriptors([{"desc": descsum_create(f"wpkh({bip39_key_expr()})"), "timestamp": "now"}])
        assert_equal(result[0]["success"], False)
        assert_equal(result[0]["error"]["code"], -4)
        assert_equal(result[0]["error"]["message"], "Cannot import private keys to a wallet with private keys disabled")

    def run_test(self):
        self.test_descriptor_analysis()
        self.test_deriveaddresses()
        self.test_wallet_import_and_signing()
        self.test_import_errors()


if __name__ == "__main__":
    WalletBip39DescriptorTest(__file__).main()


