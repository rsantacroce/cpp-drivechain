// Copyright (c) 2025 L2L
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php

#ifndef L2L_DRIVECHAIN_CONFIG_H
#define L2L_DRIVECHAIN_CONFIG_H

#include <string>

struct EnforcerConfig {
    std::string host;
    int port;
    
    EnforcerConfig() : host("127.0.0.1"), port(8123) {}
};

EnforcerConfig GetEnforcerConfig();

#endif // L2L_DRIVECHAIN_CONFIG_H