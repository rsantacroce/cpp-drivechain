// Copyright (c) 2025 L2L
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php

#ifndef L2L_BMMCACHE_H
#define L2L_BMMCACHE_H

#include <uint256.h>

#include <deque>
#include <map>
#include <set>
#include <vector>

struct MainBlockIndex
{
    size_t index;
    uint256 hash;
};

class CBlock;

// BMMCache:
// * Load and save bmm.dat and hashmainblock.dat
// * Store BMM validation to avoid re-checking
// * Store mainchain block hash cache
// * Detect mainchain reorgs
// * Keep track of pending BMM attempts
//
// TODO improvements:
// * See notes in cpp about no enforcer reorg handling
//
// * Store list of mainchain blocks we have already checked for BMM,
// so that we don't re-check them.
//
// * Cache BMM we have already verified to skip re-verification
class BMMCache
{
public:
    BMMCache();

    bool StoreBMMBlock(const CBlock& block);

    bool GetBMMBlock(const uint256& hashHStar, CBlock& block);

    std::vector<CBlock> GetBMMBlockCache() const;

    std::vector<uint256> GetMainBlockHashCache() const;

    std::vector<uint256> GetRecentMainBlockHashes() const;

    void ClearBMMBlocks();

    void CacheMainBlockHash(const uint256& hash);

    void CacheMainBlockHash(const std::vector<uint256>& vHash);

    bool UpdateMainBlockCache();

    uint256 GetLastMainBlockHash() const;

    uint256 GetMainPrevBlockHash(const uint256& hashBlock) const;

    int GetCachedBlockCount() const;

    int GetMainchainBlockHeight(const uint256& hash) const;

    bool HaveMainBlock(const uint256& hash) const;

    void ResetMainBlockCache();

    bool VerifyMainBlockCache(std::string&);

    uint256 GetLastMainchainTipBMM();
    
    void SetLastMainchainTipBMM(const uint256& hash);

    /** Dump the BMM caches to disk. */
    void DumpBMMCache();

    /** Load the BMM caches from disk. */
    void LoadBMMCache();

private:
    // The most recent mainchain tip we created a BMM request for
    uint256 hashMainchainTipLastBMM;

    // BMM blocks that we have created with the intention of connecting to the
    // side blockchain once the BMM h* hash is included on the mainchain
    std::map<uint256 /* hashMerkleRoot */, CBlock> mapBMMBlocks;

    // Index of mainchain block hash in vMainBlockHash
    std::map<uint256 /* hashMainchainBlock */, MainBlockIndex> mapMainBlock;

    // List of all known mainchain block hashes in order
    std::vector<uint256> vMainBlockHash;
};


#endif // L2L_BMMCACHE_H

