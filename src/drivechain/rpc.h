// Copyright (c) 2025 L2L
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php

// This file is for RPC functions a drivechain requires from:
// - Bitcoin patched mainchain
// - The CUSF Enforcer

#ifndef L2L_DRIVECHAIN_RPC_H
#define L2L_DRIVECHAIN_RPC_H

#include <uint256.h>
#include <vector>
#include <string>

// Structure to hold sidechain deposit transaction data
struct SidechainDeposit {
    int id;
    std::string txid;
    uint64_t fee;
    uint64_t received;
    uint64_t sent;
    int block_height;
    std::string block_hash;
    uint64_t confirmation_time;
    
    SidechainDeposit() : id(0), fee(0), received(0), sent(0), block_height(0), confirmation_time(0) {}
};

// Structure to hold CTip (current tip) data
struct CTip {
    std::string outpoint;  // Format: "txid:output_index"
    uint64_t value;        // Value in satoshis
    
    CTip() : value(0) {}
};

// Bitcoin-patched RPC client interface

bool RPCGetBTCBlockCount(int& nBlocks);

bool RPCGetBTCBlockHash(const int& nHeight, uint256& hash);

// CUSF Enfocer RPC client interface

bool RPCVerifyBMM(const uint256& hashMainBlock, const uint256& hashHStar, uint256& txid, int nTime);

bool RPCGetDeposits(/* Maybe: std::vector<DrivechainDeposit>& vDeposit*/);

bool RPCGetSidechainDeposits(std::vector<SidechainDeposit>& deposits);

bool RPCGetCTip(const int& sidechain_number, CTip& ctip);

#endif // L2L_DRIVECHAIN_RPC_H
