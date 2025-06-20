// Copyright (c) 2025 L2L
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php
#include <drivechain/config.h>
#include <common/args.h>

extern ArgsManager gArgs;

EnforcerConfig GetEnforcerConfig()
{
    EnforcerConfig config;
    config.host = gArgs.GetArg("-enforcerhost", "127.0.0.1");
    config.port = gArgs.GetIntArg("-enforcerport", 8123);
    return config;
}