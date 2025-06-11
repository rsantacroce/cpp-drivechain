// Copyright (c) 2025 L2L
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php

// This file is for RPC functions a drivechain requires from:
// - Bitcoin patched mainchain
// - The CUSF Enforcer

#ifndef L2L_DRIVECHAIN_RPC_H
#define L2L_DRIVECHAIN_RPC_H

#include <uint256.h>

// Bitcoin-patched RPC client interface

bool RPCGetBTCBlockCount(int& nBlocks);

bool RPCGetBTCBlockHash(const int& nHeight, uint256& hash);

// CUSF Enfocer RPC client interface

bool RPCVerifyBMM(const uint256& hashMainBlock, const uint256& hashHStar, uint256& txid, int nTime);

bool RPCGetDeposits(/* Maybe: std::vector<DrivechainDeposit>& vDeposit*/);

#endif // L2L_DRIVECHAIN_RPC_H
