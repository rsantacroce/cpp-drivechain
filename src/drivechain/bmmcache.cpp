// Copyright (c) 2025 L2L
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php

#include <drivechain/bmmcache.h>

#include <drivechain/rpc.h>

#include <common/args.h>
#include <clientversion.h>
#include <primitives/block.h>
#include <streams.h>
#include <util/fs.h>
#include <util/fs_helpers.h>
#include <logging.h>

BMMCache::BMMCache()
{

}

bool BMMCache::StoreBMMBlock(const CBlock& block)
{
    if (!block.vtx.size())
        return false;

    uint256 hashMerkleRoot = block.hashMerkleRoot;

    // Already have block stored
    if (mapBMMBlocks.find(hashMerkleRoot) != mapBMMBlocks.end())
        return false;

    mapBMMBlocks[hashMerkleRoot] = block;

    return true;
}

bool BMMCache::GetBMMBlock(const uint256& hashMerkleRoot, CBlock& block)
{
    if (mapBMMBlocks.find(hashMerkleRoot) == mapBMMBlocks.end())
        return false;

    block = mapBMMBlocks[hashMerkleRoot];

    return true;
}

std::vector<CBlock> BMMCache::GetBMMBlockCache() const
{
    std::vector<CBlock> vBlock;
    for (const auto& b : mapBMMBlocks) {
        vBlock.push_back(b.second);
    }
    return vBlock;
}

std::vector<uint256> BMMCache::GetMainBlockHashCache() const
{
    return vMainBlockHash;
}

std::vector<uint256> BMMCache::GetRecentMainBlockHashes() const
{
    // Return up to three of the most recent mainchain block hashes
    std::vector<uint256> vHash;
    std::vector<uint256>::const_reverse_iterator rit = vMainBlockHash.rbegin();
    for (; rit != vMainBlockHash.rend(); rit++) {
        vHash.push_back(*rit);
        if (vHash.size() == 3)
            break;
    }
    std::reverse(vHash.begin(), vHash.end());
    return vHash;
}

void BMMCache::ClearBMMBlocks()
{
    mapBMMBlocks.clear();
}

void BMMCache::CacheMainBlockHash(const uint256& hash)
{
    // Don't re-cache the genesis block
    if (vMainBlockHash.size() == 1 && hash == vMainBlockHash.front())
        return;

    // Add to ordered vector of main block hashes
    vMainBlockHash.push_back(hash);

    // Add to index of hashes
    MainBlockIndex index;
    index.hash = hash;
    index.index = vMainBlockHash.size() - 1;

    mapMainBlock[hash] = index;
}

bool BMMCache::VerifyMainBlockCache(std::string& strError)
{
    // TODO create a new RPC to verify main block cache,
    // and re-write this from the deprecated testchain
    /* TODO
    SidechainClient client;

    const std::vector<uint256> vHash = bmmCache.GetMainBlockHashCache();
    if (!vHash.size()) {
        strError = "No mainchain blocks in cache!";
        return false;
    }

    // Compare cached hash at height with mainchain block hash at height
    for (size_t i = 0; i < vHash.size(); i++) {
        uint256 hashBlock;

        if (!client.GetBlockHash(i, hashBlock)) {
            strError = "Failed to request mainchain block hash!";
            return false;
        }

        if (hashBlock != vHash[i]) {
            strError = "Invalid hash cached: ";
            strError += vHash[i].ToString();
            strError += " height: ";
            strError += std::to_string(i);

            return false;
        }
    }
    */

    return true;
}

bool BMMCache::UpdateMainBlockCache()
{
    //
    // Note: bitcoin core does not count genesis block towards block count but
    // we will cache it.
    //

    // Get the current mainchain block height
    int nMainBlocks = 0;
    if (!RPCGetBTCBlockCount(nMainBlocks)) {
        LogPrintf("%s: Failed to update - cannot get block count from mainchain. (connection issue?)\n", __func__);
        return false;
    }

    LogPrintf("%s: Reported mainchain block count: %u", __func__, nMainBlocks);

    uint256 hashMainTip;
    if (!RPCGetBTCBlockHash(nMainBlocks, hashMainTip)) {
        LogPrintf("%s: Failed to get to mainchain tip block hash!\n", __func__);
        return false;
    }

    uint256 hashCachedTip = GetLastMainBlockHash();

    // If the block height hasn't changed, check that if cached chain tip is the
    // same as the current mainchain tip. If it is we don't need to do anything
    // else. If it isn't we will continue to update / reorg handling.
    int nCachedBlocks = GetCachedBlockCount();
    if (nMainBlocks + 1 == nCachedBlocks && hashCachedTip == hashMainTip) {
        LogPrintf("%s: Already synced!", __func__);
        return true;
    }

    // Otherwise;
    // From the new mainchain tip, start looping back through mainchain blocks
    // while keeping track of them in order until we find one that connects to
    // one of our cached blocks by prevblock.
    uint256 hashPrevBlock;
    std::deque<uint256> deqHashNew;
    for (int i = nMainBlocks; i > 0; i--) {
        // Get the prevblockhash
        if (!RPCGetBTCBlockHash(i - 1, hashPrevBlock)) {
            LogPrintf("%s: Failed to get to mainchain block: %u\n", __func__, i - 1);
            return false;
        }

        // Check if the prevblock is in our cache. Once we find a prevblock in
        // our cache we can update our cache from that block up to the new
        // mainchain tip.
        if (HaveMainBlock(hashPrevBlock)) {
            deqHashNew.push_front(hashPrevBlock);
            break;
        }

        deqHashNew.push_front(hashPrevBlock);
    }
    // Also add the new mainchain tip
    deqHashNew.push_back(hashMainTip);

    if (deqHashNew.empty()) {
        LogPrintf("%s: Error - empty list of new block hashes!\n", __func__);
        return false;
    }

    // If the main block cache doesn't have the genesis block yet, add it first
    if (vMainBlockHash.empty())
        CacheMainBlockHash(deqHashNew.front());

    // Figure out the block in our cache that we will append the new blocks to
    MainBlockIndex index;
    if (mapMainBlock.count(deqHashNew.front())) {
        index = mapMainBlock[deqHashNew.front()];
    } else {
        LogPrintf("%s: Error - New blocks do not connect to cached chain!\n", __func__);
        return false;
    }

    // TODO
    // Until the enforcer handles reorgs we should just ignore reorgs
    bool fReorg = false;
    std::vector<uint256> vOrphan;

    // If there were any blocks in our cache after the block we will be building
    // on, remove them, add them to vOrphan as they were disconnected, set
    // fReorg true.
    if (index.index != vMainBlockHash.size() - 1) {
        LogPrintf("%s: Mainchain reorg detected!\n", __func__);
        fReorg = true;
    }

    for (size_t i = vMainBlockHash.size() - 1; i > index.index; i--) {
        vOrphan.push_back(vMainBlockHash[i]);
        vMainBlockHash.pop_back();
    }

    // Remove disconnected blocks from index map
    for (const uint256& u : vOrphan)
        mapMainBlock.erase(u);

    // It's possible that the first block in the list of new blocks (which
    // connects to our cached chain by a prevblock) was already cached.
    // The first block that connected by prevblock to one of our cached blocks
    // could either be a new different block or a block we already know
    // depending on how the fork / reorg happened.
    //
    // Check if we already know the first block in the deque and remove it if
    // we do.
    if (HaveMainBlock(deqHashNew.front()))
        deqHashNew.pop_front();

    // Append new blocks
    for (const uint256& u : deqHashNew)
        CacheMainBlockHash(u);

    LogPrintf("%s: Updated cached mainchain tip to: %s.\n", __func__, deqHashNew.back().ToString());

    // TODO (Read other notes about enforcer reorg handling)
    //if (fReorg) {
    //    HandleMainchainReorg(vOrphan);
    //}


    return true;
}

/* 
 * TODO
 * This is the mainchain reorg handling code from deprecated versions of drivechain. 
 * I don't think that the enforcer has any reorg detection or handling yet so we won't
 * do anything when a reorg is detected. 
 *
 * Once enforcer handles reorgs, this will be re-written for enforcer based drivechain.
 *
void HandleMainchainReorg(const std::vector<uint256>& vOrphan)
{
    std::lock_guard<std::mutex> lock(mainBlockCacheReorgMutex);

    // For mainchain blocks that were orphaned - invalidate bmm blocks with
    // commitments from them.
    //
    // vOrphan contains a list of mainchain block hashes that were orphaned

    // Before invalidating any blocks, check the sanity of the mainchain block
    // cache and then verify that the blocks to be orphaned actually are missing
    // from the mainchain.

    // Check the mainchain block cache
    std::string strError = "";
    if (!VerifyMainBlockCache(strError)) {
        LogPrintf("%s: Main block cache invalid: %s. Resyncing...\n",
                __func__, strError);
        // Reset the mainchain block cache and then re-sync it
        bmmCache.ResetMainBlockCache();

        // TODO
        // If during this call a reorg is detected and we have more orphans then
        // something bad happened and needs to be handled. Since we just reset
        // the mainchain block cache, have a mutex lock, and are updating the
        // cache from scratch now, it should be impossible.
        bool fReorg = false;
        std::vector<uint256> vOrphanIgnore;
        if (!UpdateMainBlockHashCache(fReorg, vOrphanIgnore)) {
            // TODO
            // If we make it to this point there might be a connection issue or
            // something going on. Maybe the mainchain node went down during the
            // function? There might be something better to do than just logging
            // the error here.
            LogPrintf("%s: Failed to re-update main block cache after reset!",
                    __func__);
            return;
        }
    }

    // Check that the alleged orphans actually don't exist on the mainchain
    std::vector<uint256> vOrphanFinal;
    for (const uint256& u : vOrphan) {
        if (!bmmCache.HaveMainBlock(u))
            vOrphanFinal.push_back(u);
    }

    // Check if any BMM blocks were created from commitments in this
    // orphaned mainchain block
    for (const uint256& u : vOrphanFinal) {
        CValidationState state;
        {
            LOCK(cs_main);
            // Check our map of blocks based on their mainchain BMM commit block
            if (!mapBlockMainHashIndex.count(u))
                continue;

            CBlockIndex* pindex = mapBlockMainHashIndex[u];
            if (!chainActive.Contains(pindex))
                continue;

            InvalidateBlock(state, Params(), pindex);

            LogPrintf("%s: Invalidated block: %s because mainchain block: %s was orphaned!\n",
                    __func__, pindex->GetBlockHash().ToString(), u.ToString());

            if (!state.IsValid()) {
                LogPrintf("%s: Error while invalidating blocks: %s\n",
                        __func__, FormatStateMessage(state));
                return;
            }
        }

        ActivateBestChain(state, Params());
        if (!state.IsValid()) {
            LogPrintf("%s: Error activating best chain: %s\n",
                    __func__, FormatStateMessage(state));
            return;
        }
    }
}
*/

uint256 BMMCache::GetLastMainBlockHash() const
{
    if (vMainBlockHash.empty())
        return uint256();

    return vMainBlockHash.back();
}

uint256 BMMCache::GetMainPrevBlockHash(const uint256& hashBlock) const
{
    if (vMainBlockHash.size() < 2)
        return uint256();

    if (!mapMainBlock.count(hashBlock))
        return uint256();

    const MainBlockIndex index = mapMainBlock.at(hashBlock);

    size_t indexPrev = index.index;
    if (indexPrev == 0)
        return uint256();

    indexPrev -= 1;
    if (indexPrev >= vMainBlockHash.size())
        return uint256();

    return vMainBlockHash[indexPrev];
}

int BMMCache::GetCachedBlockCount() const
{
    return vMainBlockHash.size();
}

int BMMCache::GetMainchainBlockHeight(const uint256& hash) const
{
    if (!mapMainBlock.count(hash))
        return -1;

    const MainBlockIndex index = mapMainBlock.at(hash);

    return index.index - 1;
}

bool BMMCache::HaveMainBlock(const uint256& hash) const
{
    return mapMainBlock.count(hash);
}

void BMMCache::ResetMainBlockCache()
{
    vMainBlockHash.clear();
    mapMainBlock.clear();
}

uint256 BMMCache::GetLastMainchainTipBMM()
{
    return hashMainchainTipLastBMM;
}

void BMMCache::SetLastMainchainTipBMM(const uint256& hash)
{
    LogPrintf("BMMCache %s: BMM cached new attempt for mainchain tip: %s", __func__, hash.ToString());
    hashMainchainTipLastBMM = hash;
}

void BMMCache::LoadBMMCache()
{
    fs::path path = gArgs.GetDataDirNet() / "bmm.dat";
    AutoFile filein{fsbridge::fopen(path, "rb")};
    if (filein.IsNull()) {
        return;
    }

    std::vector<uint256> vHashBMM;
    try {
        int nVersionRequired, nVersionThatWrote;
        filein >> nVersionRequired;
        filein >> nVersionThatWrote;
        if (nVersionRequired > CLIENT_VERSION) {
            return;
        }

        int nBMM = 0;
        filein >> nBMM;
        for (int i = 0; i < nBMM; i++) {
            uint256 hash;
            filein >> hash;
            vHashBMM.push_back(hash);
        }
    }
    catch (const std::exception& e) {
        LogPrintf("%s: Error reading BMM cache: %s", __func__, e.what());
        return;
    }

    /* TODO
    for (const uint256& u : vHashBMM) {
        bmmCache.CacheVerifiedBMM(u);
    }
    */

    // Main block hash cache
    

    path = gArgs.GetDataDirNet() / "mainblockhash.dat";
    AutoFile fileInMainHash{fsbridge::fopen(path, "rb")};
    if (fileInMainHash.IsNull()) {
        return;
    }

    std::vector<uint256> vHash;
    try {
        int nVersionRequired, nVersionThatWrote;
        fileInMainHash >> nVersionRequired;
        fileInMainHash >> nVersionThatWrote;
        if (nVersionRequired > CLIENT_VERSION) {
            return;
        }

        int count = 0;
        fileInMainHash >> count;
        for (int i = 0; i < count; i++) {
            uint256 hash;
            fileInMainHash >> hash;
            vHash.push_back(hash);
        }
        LogPrintf("%s: Loaded: %u mainchain block hashes", __func__, count); 
    }
    catch (const std::exception& e) {
        LogPrintf("%s: Error reading main block cache: %s", __func__, e.what());
        return;
    }

    for (const uint256& u : vHash)
        CacheMainBlockHash(u);
}

void BMMCache::DumpBMMCache()
{
    // BMM verification cache
    
    std::vector<uint256> vHashBMM; // TODO  = bmmCache.GetVerifiedBMMCache();

    int nBMM = vHashBMM.size();

    fs::path path = gArgs.GetDataDirNet() / "bmm.dat.new";
    AutoFile fileout{fsbridge::fopen(path, "wb")};
    if (fileout.IsNull()) {
        return;
    }

    try {
        // TODO update version required
        fileout << 160000; // version required to read: 0.16.00 or later
        fileout << CLIENT_VERSION; // version that wrote the file

        // Verified BMM hash cache
        fileout << nBMM; // Number of Withdrawal Bundle hashes in file
        for (const uint256& u : vHashBMM) {
            fileout << u;
        }
    }
    catch (const std::exception& e) {
        LogPrintf("%s: Error writing BMM cache: %s", __func__, e.what());
        return;
    }

    fileout.Commit();
    fileout.fclose();
    if (!RenameOver(gArgs.GetDataDirNet() / "bmm.dat.new", gArgs.GetDataDirNet() / "bmm.dat")) 
        LogPrintf("%s: Failed to overwrite old bmm.dat\n", __func__);

    LogPrintf("%s: Wrote BMM cache.\n", __func__);


    // Main block hash cache
    

    std::vector<uint256> vHash = GetMainBlockHashCache();
    LogPrintf("%s: %u mainchain block hashes to write.", vHash.size(), __func__);
    if (vHash.empty())
        return;

    int count = vHash.size();

    fs::path pathMainBlock = gArgs.GetDataDirNet() / "mainblockhash.dat.new";
    AutoFile fileOutMainHash{fsbridge::fopen(pathMainBlock, "wb")};
    if (fileOutMainHash.IsNull()) {
        return;
    }

    try {
        // TODO update version required
        fileOutMainHash << 160000; // version required to read: 0.16.00 or later
        fileOutMainHash << CLIENT_VERSION; // version that wrote the file
        fileOutMainHash << count; // Number of mainchain block hashes in file
                  
        for (const uint256& u : vHash) {
            fileOutMainHash << u;
        }
    }
    catch (const std::exception& e) {
        LogPrintf("%s: Error writing main block cache: %s", __func__, e.what());
        return;
    }

    fileOutMainHash.Commit();
    fileOutMainHash.fclose();
    
    if (!RenameOver(gArgs.GetDataDirNet() / "mainblockhash.dat.new", gArgs.GetDataDirNet() / "mainblockhash.dat"))
        LogPrintf("%s: Failed to overwrite old mainchainblockhash.dat\n", __func__);

    LogPrintf("%s: Wrote %u main block hashes\n", __func__, count);

}
