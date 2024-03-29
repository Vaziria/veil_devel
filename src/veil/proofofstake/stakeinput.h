// Copyright (c) 2017-2019 The PIVX developers
// Copyright (c) 2019-2022 The Veil developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_STAKEINPUT_H
#define PIVX_STAKEINPUT_H

#include "veil/zerocoin/accumulatormap.h"
#include "chain.h"
#include "streams.h"

#include "libzerocoin/CoinSpend.h"

class CKeyStore;
class CWallet;
class CWalletTx;

class CStakeInput
{
protected:
    CBlockIndex* pindexFrom;
    libzerocoin::CoinDenomination denom = libzerocoin::CoinDenomination::ZQ_ERROR;

public:
    virtual ~CStakeInput(){};
    virtual CBlockIndex* GetIndexFrom() = 0;
    virtual bool GetTxFrom(CTransaction& tx) = 0;
    virtual CAmount GetValue() = 0;
    virtual CAmount GetWeight() = 0;
    virtual bool GetModifier(uint64_t& nStakeModifier, const CBlockIndex* pindexChainPrev) = 0;
    virtual bool IsZerocoins() = 0;
    virtual CDataStream GetUniqueness() = 0;
    libzerocoin::CoinDenomination GetDenomination() {return denom;};

    virtual bool CreateTxIn(CWallet* pwallet, CTxIn& txIn, uint256 hashTxOut = uint256()) = 0;
    virtual bool CreateTxOuts(CWallet* pwallet, std::vector<CTxOut>& vout, CAmount nTotal) = 0;
    virtual bool CompleteTx(CWallet* pwallet, CMutableTransaction& txNew) = 0;
};


// zPIVStake can take two forms
// 1) the stake candidate, which is a zcmint that is attempted to be staked
// 2) a staked zerocoin, which is a zcspend that has successfully staked
class ZerocoinStake : public CStakeInput
{
private:
    uint256 nChecksum;
    bool fMint;
    uint256 hashSerial;

public:
    explicit ZerocoinStake(libzerocoin::CoinDenomination denom, const uint256& hashSerial)
    {
        this->denom = denom;
        this->hashSerial = hashSerial;
        this->pindexFrom = nullptr;
        fMint = true;
    }

    explicit ZerocoinStake(const libzerocoin::CoinSpend& spend);

    CBlockIndex* GetIndexFrom() override;
    bool GetTxFrom(CTransaction& tx) override;
    CAmount GetValue() override;
    CAmount GetWeight() override;
    bool GetModifier(uint64_t& nStakeModifier, const CBlockIndex* pindexChainPrev) override;
    CDataStream GetUniqueness() override;
    bool CreateTxIn(CWallet* pwallet, CTxIn& txIn, uint256 hashTxOut = uint256()) override;
    bool CreateTxOuts(CWallet* pwallet, std::vector<CTxOut>& vout, CAmount nTotal) override;
    bool CompleteTx(CWallet* pwallet, CMutableTransaction& txNew) override;
    bool IsZerocoins() override { return true; }

    bool MarkSpent(CWallet* pwallet, const uint256& txid);
    int GetChecksumHeightFromMint();
    int GetChecksumHeightFromSpend();
    uint256 GetChecksum();

    static int HeightToModifierHeight(int nHeight);
};

#endif //PIVX_STAKEINPUT_H
