// Copyright (c) 2025 L2L
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php

#include <drivechain/rpc.h>
#include <drivechain/config.h>

#include <logging.h>
#include <util/strencodings.h>
#include <univalue.h>

#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

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

bool RPCEnforcer(const std::string& json, boost::property_tree::ptree& ptree)
{
    EnforcerConfig config = GetEnforcerConfig();

    try {
        // Setup BOOST ASIO for a synchronus call to the mainchain
        boost::asio::io_context io_service;
        boost::asio::ip::tcp::socket socket(io_service);
        boost::system::error_code error;
        socket.connect(boost::asio::ip::tcp::endpoint(
                           boost::asio::ip::make_address(config.host), config.port),
                       error);

        if (error) throw boost::system::system_error(error);

        // HTTP request (package the json for sending)
        boost::asio::streambuf output;
        std::ostream os(&output);
        os << "POST / HTTP/1.1\n";
        os << "Host: 127.0.0.1\n";
        os << "Content-Type: application/json\n";
        os << "Connection: close\n";
        os << "Content-Length: " << json.size() << "\n\n";
        os << json;

        // Send the request
        boost::asio::write(socket, output);
        
        // Read the reponse
        std::string data;
        for (;;) {
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
        
        LogPrintf("Sent request to enforcer\n");
        
        // Parse HTTP response properly
        std::stringstream ss(data);
        std::string line;
        
        // Read HTTP status line
        std::getline(ss, line);
        if (line.find("200") == std::string::npos) {
            LogPrintf("ERROR: HTTP response not 200: %s\n", line.c_str());
            return false;
        }
        
        // Skip headers until we find empty line
        while (std::getline(ss, line) && !line.empty() && line != "\r") {
            // Skip header lines
        }
        
        // Read the JSON body
        std::string jsonBody;
        while (std::getline(ss, line)) {
            jsonBody += line + "\n";
        }
        
        // Parse JSON response
        std::stringstream jss(jsonBody);
        boost::property_tree::json_parser::read_json(jss, ptree);
    } catch (std::exception& exception) {        
        LogPrintf("ERROR Sidechain client at %s:%d (sendRequestToEnforcer): %s\n", config.host, config.port, exception.what());
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

bool RPCGetSidechainDeposits(std::vector<SidechainDeposit>& deposits)
{
    std::string json;
    json.append("{\"jsonrpc\": \"2.0\", \"id\":\"Drivechain\", ");
    json.append("\"method\": \"wallet.list_sidechain_deposit_transactions\", \"params\": ");
    json.append("[] }");

    boost::property_tree::ptree ptree;
    if (!RPCEnforcer(json, ptree)) {
        LogPrintf("ERROR Sidechain client failed to request sidechain deposits!\n");
        return false;
    }

    try {
        // Get the result array from the JSON-RPC response
        boost::property_tree::ptree result_array = ptree.get_child("result");
        
        // Clear the deposits vector
        deposits.clear();
        
        // Iterate through the result array
        for (const auto& deposit_item : result_array) {
            if (deposit_item.second.empty()) continue;
            
            // Each deposit item should be an array with [id, transaction_data]
            auto deposit_array = deposit_item.second;
            auto it = deposit_array.begin();
            
            if (it == deposit_array.end()) continue;
            
            SidechainDeposit deposit;
            
            // Get the ID (first element)
            deposit.id = it->second.get_value<int>();
            ++it;
            
            if (it == deposit_array.end()) continue;
            
            // Get the transaction data (second element)
            auto tx_data = it->second;
            
            // Parse transaction fields
            deposit.txid = tx_data.get("txid", "");
            deposit.fee = tx_data.get("fee", 0);
            deposit.received = tx_data.get("received", 0);
            deposit.sent = tx_data.get("sent", 0);
            
            // Parse chain_position data
            auto chain_position = tx_data.get_child_optional("chain_position");
            if (chain_position) {
                auto confirmed = chain_position->get_child_optional("Confirmed");
                if (confirmed) {
                    auto anchor = confirmed->get_child_optional("anchor");
                    if (anchor) {
                        auto block_id = anchor->get_child_optional("block_id");
                        if (block_id) {
                            deposit.block_height = block_id->get("height", 0);
                            deposit.block_hash = block_id->get("hash", "");
                        }
                        deposit.confirmation_time = anchor->get("confirmation_time", 0);
                    }
                }
            }
            
            deposits.push_back(deposit);
        }
        
        LogPrintf("Successfully parsed %d sidechain deposits\n", deposits.size());
        return true;
        
    } catch (const std::exception& e) {
        LogPrintf("ERROR parsing sidechain deposits response: %s\n", e.what());
        return false;
    }
}

bool RPCGetCTip(const int& sidechain_number, CTip& ctip)
{
    std::string json;
    json.append("{\"jsonrpc\": \"2.0\", \"id\":1, ");
    json.append("\"method\": \"validator.ctip\", \"params\": ");
    json.append("[ ");
    json.append(UniValue(sidechain_number).write());
    json.append(" ] }");
    
    LogPrintf("RPCGetCTip: %s\n", json.c_str());

    boost::property_tree::ptree ptree;
    if (!RPCEnforcer(json, ptree)) {
        LogPrintf("ERROR Sidechain client failed to request CTip!\n");
        return false;
    }

    try {
        // Get the result object from the JSON-RPC response
        boost::property_tree::ptree result = ptree.get_child("result");
        
        // Parse the CTip data
        ctip.outpoint = result.get("outpoint", "");
        ctip.value = result.get("value", 0);
        
        LogPrintf("Successfully retrieved CTip: outpoint=%s, value=%lu\n", ctip.outpoint.c_str(), ctip.value);
        return true;
        
    } catch (const std::exception& e) {
        LogPrintf("ERROR parsing CTip response: %s\n", e.what());
        return false;
    }
}