// Copyright (c) 2025 L2L
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php

#include <drivechain/rpc.h>

//#include <common/system.h>
#include <logging.h>
#include <util/strencodings.h>
#include <univalue.h>

//#include <deque>
#include <iostream>
//#include <map>
#include <optional>
//#include <set>
#include <sstream>
//#include <stdlib.h>
#include <string>
//#include <vector>

#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

using boost::asio::ip::tcp;

const std::string BITCOIN_RPC_USER = "user"; // TODO currently hardcoded by enforcer
const std::string BITCOIN_RPC_PASS = "password"; // TODO currently hardcoded by enforcer
const std::string BITCOIN_RPC_HOST = "127.0.0.1";
const int BITCOIN_RPC_PORT = 38332;

// Bitcoin-patched RPC client:
// TODO The client function for enforcer should be pretty much the same?
// If it is possible to use one function for btc and enforcer that would be
// cool but having two is fine if they end up being different
bool RPCBitcoinPatched(const std::string& json, boost::property_tree::ptree &ptree)
{
    // Format user:pass for authentication
    std::string auth = BITCOIN_RPC_USER + ":" + BITCOIN_RPC_PASS;
    if (auth == ":")
        return false;

    try {
        // Setup BOOST ASIO for a synchronus call to the mainchain
        boost::asio::io_context io_service;
        boost::asio::ip::tcp::socket socket(io_service);
        boost::system::error_code error;
        socket.connect(boost::asio::ip::tcp::endpoint(
                    boost::asio::ip::make_address(BITCOIN_RPC_HOST), BITCOIN_RPC_PORT), error);

        if (error) throw boost::system::system_error(error);

        // HTTP request (package the json for sending)
        boost::asio::streambuf output;
        std::ostream os(&output);
        os << "POST / HTTP/1.1\n";
        os << "Host: 127.0.0.1\n";
        os << "Content-Type: application/json\n";
        os << "Authorization: Basic " << EncodeBase64(auth) << std::endl;
        os << "Connection: close\n";
        os << "Content-Length: " << json.size() << "\n\n";
        os << json;

        // Send the request
        boost::asio::write(socket, output);

        // Read the reponse
        std::string data;
        for (;;)
        {
            boost::array<char, 256> buf;

            // Read until end of file (socket closed)
            boost::system::error_code e;
            size_t sz = socket.read_some(boost::asio::buffer(buf), e);

            data.insert(data.size(), buf.data(), sz);

            if (e == boost::asio::error::eof)
                break; // socket closed
            else if (e)
                throw boost::system::system_error(e);
        }

        std::stringstream ss;
        ss << data;

        // Get response code
        ss.ignore(std::numeric_limits<std::streamsize>::max(), ' ');
        int code;
        ss >> code;

        // Check response code
        if (code != 200)
            return false;

        // Skip the rest of the header
        for (size_t i = 0; i < 5; i++)
            ss.ignore(std::numeric_limits<std::streamsize>::max(), '\r');

        // Parse json response;
        std::string JSON;
        ss >> JSON;
        std::stringstream jss;
        jss << JSON;
        boost::property_tree::json_parser::read_json(jss, ptree);
    } catch (std::exception &exception) {
        LogPrintf("ERROR Sidechain client at %s:%d (sendRequestToMainchain): %s\n", BITCOIN_RPC_HOST, BITCOIN_RPC_PORT, exception.what());
        return false;
    }
    return true;
}

//
// Bitcoin-patched rpc requests:
//

bool RPCGetBTCBlockCount(int& nBlocks)
{
    // JSON for 'getblockcount' bitcoin HTTP-RPC
    std::string json;
    json.append("{\"jsonrpc\": \"1.0\", \"id\":\"Drivechain\", ");
    json.append("\"method\": \"getblockcount\", \"params\": ");
    json.append("[] }");

    // Try to request mainchain block count
    boost::property_tree::ptree ptree;
    if (!RPCBitcoinPatched(json, ptree)) {
        LogPrintf("ERROR failed to request block count\n");
        return false;
    }

    // Process result
    nBlocks = ptree.get("result", 0);

    return nBlocks >= 0;
}

bool RPCGetBTCBlockHash(const int& nHeight, uint256& hash)
{
    // JSON for 'getblockhash' mainchain HTTP-RPC
    std::string json;
    json.append("{\"jsonrpc\": \"1.0\", \"id\":\"Drivechain\", ");
    json.append("\"method\": \"getblockhash\", \"params\": ");
    json.append("[");
    json.append(UniValue(nHeight).write());
    json.append("] }");

    // Try to request mainchain block hash
    boost::property_tree::ptree ptree;
    if (!RPCBitcoinPatched(json, ptree)) {
        LogPrintf("ERROR Sidechain client failed to request block hash!\n");
        return false;
    }

    std::string strHash = ptree.get("result", "");
    std::optional hash_rv = uint256::FromHex(strHash);

    if (!hash_rv.has_value())
        return false;

    hash = hash_rv.value();

    return true;
}

//
// Enforcer rpc requests:
//

bool RPCVerifyBMM(const uint256& hashMainBlock, const uint256& hashHStar, uint256& txid, int nTime)
{
    // TODO this should ask the enforcer to verify h* is in the mainchain block
    // and give us the txid for the txid of the BMM request (If the enforcer 
    // can which I'm not sure of), and the block time of the block that mined
    // the bmm request and has the bmm commit coinbase output. 
    return true;
}

bool RPCGetDeposits(/* Maybe: std::vector<DrivechainDeposit>& vDeposit*/)
{
    // TODO
    // The enforcer doesn't give a list of sidechain deposits like the
    // deprecated L1. Instead we will use GetTwoWayPegData or GetBlockInfo
    // for every L1 block to collect valid deposits and store them. (I think??)
    //
    // Then we will need a function that the miner can use to include 
    // any unpaid deposits when creating new blocks
    //
    // TODO how are they ordered? 
    //
    // TODO can we verify a specific deposit with the enforcer?
    //
    return true;
}
