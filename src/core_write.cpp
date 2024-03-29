// Copyright (c) 2009-2019 The Bitcoin Core developers
// Copyright (c) 2018-2022 The Veil developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <core_io.h>

#include <veil/ringct/blind.h>
#include <veil/zerocoin/zchain.h>
#include <consensus/consensus.h>
#include <consensus/validation.h>
#include <key_io.h>
#include <script/script.h>
#include <script/standard.h>
#include <serialize.h>
#include <streams.h>
#include <univalue.h>
#include <util/system.h>
#include <util/moneystr.h>
#include <util/strencodings.h>
#include <veil/ringct/rctindex.h>

UniValue ValueFromAmount(const CAmount& amount)
{
    bool sign = amount < 0;
    int64_t n_abs = (sign ? -amount : amount);
    int64_t quotient = n_abs / COIN;
    int64_t remainder = n_abs % COIN;
    return UniValue(UniValue::VNUM,
            strprintf("%s%d.%08d", sign ? "-" : "", quotient, remainder));
}

std::string FormatScript(const CScript& script)
{
    std::string ret;
    CScript::const_iterator it = script.begin();
    opcodetype op;
    while (it != script.end()) {
        CScript::const_iterator it2 = it;
        std::vector<unsigned char> vch;
        if (script.GetOp(it, op, vch)) {
            if (op == OP_0) {
                ret += "0 ";
                continue;
            } else if ((op >= OP_1 && op <= OP_16) || op == OP_1NEGATE) {
                ret += strprintf("%i ", op - OP_1NEGATE - 1);
                continue;
            } else if (op >= OP_NOP && op <= OP_NOP10) {
                std::string str(GetOpName(op));
                if (str.substr(0, 3) == std::string("OP_")) {
                    ret += str.substr(3, std::string::npos) + " ";
                    continue;
                }
            }
            if (vch.size() > 0) {
                ret += strprintf("0x%x 0x%x ", HexStr(it2, it - vch.size()), HexStr(it - vch.size(), it));
            } else {
                ret += strprintf("0x%x ", HexStr(it2, it));
            }
            continue;
        }
        ret += strprintf("0x%x ", HexStr(it2, script.end()));
        break;
    }
    return ret.substr(0, ret.size() - 1);
}

const std::map<unsigned char, std::string> mapSigHashTypes = {
    {static_cast<unsigned char>(SIGHASH_ALL), std::string("ALL")},
    {static_cast<unsigned char>(SIGHASH_ALL|SIGHASH_ANYONECANPAY), std::string("ALL|ANYONECANPAY")},
    {static_cast<unsigned char>(SIGHASH_NONE), std::string("NONE")},
    {static_cast<unsigned char>(SIGHASH_NONE|SIGHASH_ANYONECANPAY), std::string("NONE|ANYONECANPAY")},
    {static_cast<unsigned char>(SIGHASH_SINGLE), std::string("SINGLE")},
    {static_cast<unsigned char>(SIGHASH_SINGLE|SIGHASH_ANYONECANPAY), std::string("SINGLE|ANYONECANPAY")},
};

std::string SighashToStr(unsigned char sighash_type)
{
    const auto& it = mapSigHashTypes.find(sighash_type);
    if (it == mapSigHashTypes.end()) return "";
    return it->second;
}

/**
 * Create the assembly string representation of a CScript object.
 * @param[in] script    CScript object to convert into the asm string representation.
 * @param[in] fAttemptSighashDecode    Whether to attempt to decode sighash types on data within the script that matches the format
 *                                     of a signature. Only pass true for scripts you believe could contain signatures. For example,
 *                                     pass false, or omit the this argument (defaults to false), for scriptPubKeys.
 */
std::string ScriptToAsmStr(const CScript& script, const bool fAttemptSighashDecode)
{
    std::string str;
    opcodetype opcode;
    std::vector<unsigned char> vch;
    CScript::const_iterator pc = script.begin();
    while (pc < script.end()) {
        if (!str.empty()) {
            str += " ";
        }
        if (!script.GetOp(pc, opcode, vch)) {
            str += "[error]";
            return str;
        }
        if (0 <= opcode && opcode <= OP_PUSHDATA4) {
            if (vch.size() <= static_cast<std::vector<unsigned char>::size_type>(4)) {
                str += strprintf("%d", CScriptNum(vch, false).getint());
            } else {
                // the IsUnspendable check makes sure not to try to decode OP_RETURN data that may match the format of a signature
                if (fAttemptSighashDecode && !script.IsUnspendable()) {
                    std::string strSigHashDecode;
                    // goal: only attempt to decode a defined sighash type from data that looks like a signature within a scriptSig.
                    // this won't decode correctly formatted public keys in Pubkey or Multisig scripts due to
                    // the restrictions on the pubkey formats (see IsCompressedOrUncompressedPubKey) being incongruous with the
                    // checks in CheckSignatureEncoding.
                    if (CheckSignatureEncoding(vch, SCRIPT_VERIFY_STRICTENC, nullptr)) {
                        const unsigned char chSigHashType = vch.back();
                        if (mapSigHashTypes.count(chSigHashType)) {
                            strSigHashDecode = "[" + mapSigHashTypes.find(chSigHashType)->second + "]";
                            vch.pop_back(); // remove the sighash type byte. it will be replaced by the decode.
                        }
                    }
                    str += HexStr(vch) + strSigHashDecode;
                } else {
                    str += HexStr(vch);
                }
            }
        } else {
            str += GetOpName(opcode);
        }
    }
    return str;
}

std::string EncodeHexTx(const CTransaction& tx, const int serializeFlags)
{
    CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION | serializeFlags);
    ssTx << tx;
    return HexStr(ssTx);
}

void AddRangeproof(const std::vector<uint8_t> &vRangeproof, UniValue &entry)
{
    entry.pushKV("rangeproof", HexStr(vRangeproof.begin(), vRangeproof.end()));

    if (vRangeproof.size() > 0)
    {
        int exponent, mantissa;
        CAmount min_value, max_value;
        if (0 == GetRangeProofInfo(vRangeproof, exponent, mantissa, min_value, max_value))
        {
            entry.pushKV("rp_exponent", exponent);
            entry.pushKV("rp_mantissa", mantissa);
            entry.pushKV("rp_min_value", ValueFromAmount(min_value));
            entry.pushKV("rp_max_value", ValueFromAmount(max_value));
        };
    };
}

void OutputToJSON(const uint256 &txid, const int& i,
                  const CTxOutBase *baseOut, UniValue &entry, bool isCoinBase)
{
    switch (baseOut->GetType())
    {
        case OUTPUT_STANDARD:
        {
            if (isCoinBase && i == 0) {
                entry.pushKV("type", "coinbase");
            } else {
                entry.pushKV("type", "standard");
            }
            CTxOutStandard *s = (CTxOutStandard*) baseOut;
            entry.pushKV("value", ValueFromAmount(s->nValue));
            entry.pushKV("valueSat", s->nValue);
            entry.pushKV("vout.n", i);
            UniValue o(UniValue::VOBJ);
            ScriptPubKeyToUniv(s->scriptPubKey, o, true);
            entry.pushKV("scriptPubKey", o);
        }
            break;
        case OUTPUT_DATA:
        {
            CTxOutData *s = (CTxOutData*) baseOut;
            entry.pushKV("type", "data");
            entry.pushKV("data_hex", HexStr(s->vData.begin(), s->vData.end()));
            entry.pushKV("vout.n", i);
            CAmount nValue;
            if (s->GetCTFee(nValue))
                entry.pushKV("ct_fee", ValueFromAmount(nValue));
        }
            break;
        case OUTPUT_CT:
        {
            CTxOutCT *s = (CTxOutCT*) baseOut;
            entry.pushKV("type", "blind");
            entry.pushKV("valueCommitment", HexStr(&s->commitment.data[0], &s->commitment.data[0]+33));
            entry.pushKV("vout.n", i);
            UniValue o(UniValue::VOBJ);
            ScriptPubKeyToUniv(s->scriptPubKey, o, true);
            entry.pushKV("scriptPubKey", o);
            entry.pushKV("data_hex", HexStr(s->vData.begin(), s->vData.end()));

            AddRangeproof(s->vRangeproof, entry);
        }
            break;
        case OUTPUT_RINGCT:
        {
            CTxOutRingCT *s = (CTxOutRingCT*) baseOut;

            entry.pushKV("type", "ringct");
            entry.pushKV("vout.n", i);
            entry.pushKV("pubkey", HexStr(s->pk.begin(), s->pk.end()));
            entry.pushKV("pubkey_hash", CBitcoinAddress(s->pk.GetID()).ToString());
            std::vector<uint8_t> objKeyImage;
            objKeyImage.resize(33);
            memcpy(&objKeyImage[0], &s->pk[0], 33);
            entry.pushKV("key_image", HexStr(objKeyImage));

            entry.pushKV("valueCommitment", HexStr(&s->commitment.data[0], &s->commitment.data[0]+33));
            entry.pushKV("data_hex", HexStr(s->vData.begin(), s->vData.end()));


            AddRangeproof(s->vRangeproof, entry);
        }
            break;
        default:
            entry.pushKV("type", "unknown");
            break;
    }
}

void RingCTOutputToJSON(const uint256& txid, const int& i, const int64_t& ringctIndex, const CTxOutRingCT& ringctOut, UniValue &entry)
{
    entry.pushKV("type", "anon");
    entry.pushKV("tx_hash", txid.GetHex());
    entry.pushKV("n", i);
    entry.pushKV("ringct_index", ringctIndex);
    entry.pushKV("pubkey", HexStr(ringctOut.pk.begin(), ringctOut.pk.end()));
    entry.pushKV("pubkey_hash", CBitcoinAddress(ringctOut.pk.GetID()).ToString());
    entry.pushKV("valueCommitment", HexStr(&ringctOut.commitment.data[0], &ringctOut.commitment.data[0]+33));
    entry.pushKV("data_hex", HexStr(ringctOut.vData.begin(), ringctOut.vData.end()));

//    AddRangeproof(ringctOut.vRangeproof, entry);
}
void CTOutputToJSON(const uint256& txid, const int& i, const CTxOutCT& ctOut, UniValue &entry)
{
    entry.pushKV("type", "stealth");
    entry.pushKV("tx_hash", txid.GetHex());
    entry.pushKV("n", i);
    entry.pushKV("scriptPubKey", HexStr(ctOut.scriptPubKey.begin(), ctOut.scriptPubKey.end()));
    CTxDestination dest;
    if (ExtractDestination(ctOut.scriptPubKey, dest)) {
        entry.pushKV("destination_bech32", EncodeDestination(dest, true));
        entry.pushKV("destination", EncodeDestination(dest, false));
    }
    entry.pushKV("valueCommitment", HexStr(&ctOut.commitment.data[0], &ctOut.commitment.data[0]+33));
    entry.pushKV("data_hex", HexStr(ctOut.vData.begin(), ctOut.vData.end()));

//    AddRangeproof(ctOut.vRangeproof, entry);
}


void AnonOutputToJSON(const CAnonOutput& output, const int& ringctindex, UniValue &entry)
{
    entry.pushKV("ringctindex", ringctindex);
    entry.pushKV("pubkey", HexStr(output.pubkey.begin(), output.pubkey.end()));
    entry.pushKV("commitment", HexStr(&output.commitment.data[0], &output.commitment.data[0]+33));
    entry.pushKV("tx_hash", output.outpoint.hash.GetHex());
    entry.pushKV("tx_index", std::to_string(output.outpoint.n));
    entry.pushKV("blockheight", output.nBlockHeight);
    entry.pushKV("compromised", output.nCompromised);
}

void ScriptToUniv(const CScript& script, UniValue& out, bool include_address)
{
    out.pushKV("asm", ScriptToAsmStr(script));
    out.pushKV("hex", HexStr(script));

    std::vector<std::vector<unsigned char>> solns;
    txnouttype type;
    Solver(script, type, solns);
    out.pushKV("type", GetTxnOutputType(type));

    CTxDestination address;
    if (include_address && ExtractDestination(script, address)) {
        out.pushKV("address", EncodeDestination(address));
    }
}

void ScriptPubKeyToUniv(const CScript& scriptPubKey,
                        UniValue& out, bool fIncludeHex)
{
    txnouttype type;
    std::vector<CTxDestination> addresses;
    int nRequired;

    out.pushKV("asm", ScriptToAsmStr(scriptPubKey));
    if (fIncludeHex)
        out.pushKV("hex", HexStr(scriptPubKey));

    if (!ExtractDestinations(scriptPubKey, type, addresses, nRequired)) {
        out.pushKV("type", GetTxnOutputType(type));
        return;
    }

    out.pushKV("reqSigs", nRequired);
    out.pushKV("type", GetTxnOutputType(type));

    UniValue a(UniValue::VARR);
    for (const CTxDestination& addr : addresses) {
        a.push_back(EncodeDestination(addr));
    }
    out.pushKV("addresses", a);
}

void TxToUniv(const CTransaction& tx, const uint256& hashBlock, const std::vector<std::vector<COutPoint>>& vTxRingCtInputs, UniValue& entry, bool include_hex, int serialize_flags)
{
    entry.pushKV("txid", tx.GetHash().GetHex());
    entry.pushKV("hash", tx.GetWitnessHash().GetHex());
    entry.pushKV("version", tx.nVersion);
    entry.pushKV("size", (int)::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION));
    entry.pushKV("vsize", (GetTransactionWeight(tx) + WITNESS_SCALE_FACTOR - 1) / WITNESS_SCALE_FACTOR);
    entry.pushKV("weight", GetTransactionWeight(tx));
    entry.pushKV("locktime", (int64_t)tx.nLockTime);

    UniValue vin(UniValue::VARR);
    for (unsigned int i = 0; i < tx.vin.size(); i++) {
        const CTxIn& txin = tx.vin[i];
        UniValue in(UniValue::VOBJ);
        if (tx.IsCoinBase())
            in.pushKV("coinbase", HexStr(txin.scriptSig));
        if (txin.IsAnonInput()) {
            in.pushKV("type", "anon");
            uint32_t nSigInputs, nSigRingSize;
            txin.GetAnonInfo(nSigInputs, nSigRingSize);
            in.pushKV("num_inputs", (int) nSigInputs);
            in.pushKV("ring_size", (int) nSigRingSize);

            //Add ring ct inputs
            if (vTxRingCtInputs.size() > i) {
                std::vector<COutPoint> vRingCtInputs = vTxRingCtInputs[i];
                UniValue arrRing(UniValue::VARR);
                for (const COutPoint& outpoint : vRingCtInputs) {
                    UniValue obj(UniValue::VOBJ);
                    obj.pushKV("txid", outpoint.hash.GetHex());
                    obj.pushKV("vout.n", (uint64_t) outpoint.n);
                    arrRing.push_back(obj);
                }
                in.pushKV("ringct_inputs", arrRing);
                const std::vector<uint8_t> vKeyImages = txin.scriptData.stack[0];
                uint32_t nInputs, nRingSize;
                txin.GetAnonInfo(nInputs, nRingSize);

                UniValue arrKeyImages(UniValue::VARR);
                for (unsigned int k = 0; k < nSigInputs; k++) {
                    const CCmpPubKey &ki = *((CCmpPubKey*)&vKeyImages[k*nSigInputs]);
                    UniValue objKeyImage(UniValue::VOBJ);
                    objKeyImage.pushKV(std::to_string(k), HexStr(ki));
                    arrKeyImages.push_back(objKeyImage);
                }
                in.pushKV("key_images", arrKeyImages);
            }
        } else {
            in.pushKV("txid", txin.prevout.hash.GetHex());
            if (txin.IsZerocoinSpend()) {
                in.pushKV("type", "zerocoinspend");
                in.pushKV("denomination", FormatMoney(txin.GetZerocoinSpent()));
                std::vector<char, zero_after_free_allocator<char> > dataTxIn;
                dataTxIn.insert(dataTxIn.end(), txin.scriptSig.begin() + 4, txin.scriptSig.end());
                CDataStream serializedCoinSpend(dataTxIn, SER_NETWORK, PROTOCOL_VERSION);
                libzerocoin::CoinSpend spend(Params().Zerocoin_Params(), serializedCoinSpend);
                in.pushKV("serial", spend.getCoinSerialNumber().GetHex());
                if (spend.getVersion() >= 4) {
                    in.pushKV("pubcoin", spend.getPubcoinValue().GetHex());
                }
            }
            in.pushKV("vout", (int64_t)txin.prevout.n);
            UniValue o(UniValue::VOBJ);
            o.pushKV("asm", ScriptToAsmStr(txin.scriptSig, true));
            o.pushKV("hex", HexStr(txin.scriptSig));
            in.pushKV("scriptSig", o);
            if (!tx.vin[i].scriptWitness.IsNull()) {
                UniValue txinwitness(UniValue::VARR);
                for (const auto& item : tx.vin[i].scriptWitness.stack) {
                    txinwitness.push_back(HexStr(item.begin(), item.end()));
                }
                in.pushKV("txinwitness", txinwitness);
            }
        }
        in.pushKV("sequence", (int64_t)txin.nSequence);
        vin.push_back(in);
    }
    entry.pushKV("vin", vin);

    UniValue vout(UniValue::VARR);
    auto txid = tx.GetHash();
    for (unsigned int i = 0; i < tx.vpout.size(); i++) {
        const auto& pout = tx.vpout[i];
        UniValue out(UniValue::VOBJ);
        OutputToJSON(txid, i, pout.get(), out, tx.IsCoinBase());
        vout.push_back(out);
    }

    entry.pushKV("vout", vout);

    if (!hashBlock.IsNull())
        entry.pushKV("blockhash", hashBlock.GetHex());

    if (include_hex) {
        entry.pushKV("hex", EncodeHexTx(tx, serialize_flags)); // The hex-encoded transaction. Used the name "hex" to be consistent with the verbose output of "getrawtransaction".
    }
}
