// Copyright (c) 2025 L2L
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php

#ifndef L2L_DRIVECHAIN_H
#define L2L_DRIVECHAIN_H

class CBlock;

// The assigned sidechain number
const unsigned int THIS_SIDECHAIN = 255;

// BMM validation & cache

bool VerifyBMM(const CBlock& block);

#endif // L2L_DRIVECHAIN_H
