// Copyright (c) 2025 L2L
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php


#include <drivechain/drivechain.h>

#include <drivechain/rpc.h>
#include <chainparams.h>
#include <primitives/block.h>
#include <util/strencodings.h>
#include <common/system.h>
#include <uint256.h>
#include <logging.h>

// BMM validation & cache:


bool VerifyBMM(const CBlock& block)
{
    // Skip genesis block
    if (block.GetHash() == Params().GetConsensus().hashGenesisBlock)
        return true;

    // TODO re-implement bmm caching
    // Have we already verified BMM for this block?
    //if (bmmCache.HaveVerifiedBMM(block.GetHash()))
    //    return true;

    // h*
    const uint256 hashHStar = block.hashMerkleRoot;

    // Mainchain block hash
    const uint256 hashMainBlock = uint256(); // TODO block.hashMainchainBlock

    // Verify BMM with local CUSF Enforcer
    uint256 txid;
    uint32_t nTime = 1;
    
    if (!RPCVerifyBMM(hashMainBlock, hashHStar, txid, nTime)) {
        LogPrintf("%s: Enforcer did not find BMM h*: %s in mainchain block: %s!\n", __func__, hashHStar.ToString(), hashMainBlock.ToString());
        return false;
    }

    // TODO
    // Cache that we have verified BMM for this block
    // bmmCache.CacheVerifiedBMM(block.GetHash());

    return true;
}
