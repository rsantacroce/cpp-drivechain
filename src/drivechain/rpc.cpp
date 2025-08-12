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

//get_block_info
bool RPCVerifyBMM(const uint256& hashMainBlock, const uint256& hashHStar, const unsigned int& sidechain_number)
{
    // JSON for 'validator.get_block_info' enforcer HTTP-RPC
    std::string json;
    json.append("{\"jsonrpc\": \"2.0\", \"id\":1, ");
    json.append("\"method\": \"validator.get_block_info\", \"params\": ");
    json.append("[ ");
    json.append("\"" + hashMainBlock.ToString() + "\", ");
    json.append(UniValue(sidechain_number).write());
    json.append(" ] }");
    
    LogPrintf("RPCVerifyBMM: %s\n", json.c_str());

    boost::property_tree::ptree ptree;
    if (!RPCEnforcer(json, ptree)) {
        LogPrintf("ERROR Sidechain client failed to request block info for BMM verification!\n");
        return false;
    }

    try {
        // Get the result object from the JSON-RPC response
        boost::property_tree::ptree result = ptree.get_child("result");
        
        // Get the infos array
        auto infos_array = result.get_child("infos");
        
        // Check if we have at least one info entry
        if (infos_array.empty()) {
            LogPrintf("ERROR: No block info found in BMM verification response\n");
            return false;
        }
        
        // Get the first info entry (we expect only one for a specific block)
        auto first_info = infos_array.begin()->second;
        
        // Parse header_info
        auto header_info = first_info.get_child("header_info");
        std::string block_hash = header_info.get("block_hash", "");
        std::string prev_block_hash = header_info.get("prev_block_hash", "");
        int height = header_info.get("height", 0);
        std::string work = header_info.get("work", "");
        
        // Parse block_info
        auto block_info = first_info.get_child("block_info");
        std::string bmm_commitment = block_info.get("bmm_commitment", "");
        
        // Check if we have a valid BMM commitment
        if (bmm_commitment.empty()) {
            LogPrintf("ERROR: No BMM commitment found in block info\n");
            return false;
        }

        // Check if the provided hashHStar matches the BMM commitment
        if (hashHStar.ToString() != bmm_commitment) {
            LogPrintf("ERROR: hashHStar (%s) does not match BMM commitment (%s)\n",
                      hashHStar.ToString(), bmm_commitment);
            return false;
        }
        
        LogPrintf("Successfully verified BMM for block %s (height %d) with commitment: %s\n", 
                  block_hash.c_str(), height, bmm_commitment.c_str());
        
        return true;
        
    } catch (const std::exception& e) {
        LogPrintf("ERROR parsing BMM verification response: %s\n", e.what());
        return false;
    }
}

// Optional: Get detailed BMM verification data
bool RPCGetBlockInfo(const uint256& hashMainBlock, const unsigned int& sidechain_number, BlockInfoResponse& blockInfo)
{
    // JSON for 'validator.get_block_info' enforcer HTTP-RPC
    std::string json;
    json.append("{\"jsonrpc\": \"2.0\", \"id\":1, ");
    json.append("\"method\": \"validator.get_block_info\", \"params\": ");
    json.append("[ ");
    json.append("\"" + hashMainBlock.ToString() + "\", ");
    json.append(UniValue(sidechain_number).write());
    json.append(" ] }");
    
    LogPrintf("RPCGetBlockInfo: %s\n", json.c_str());

    boost::property_tree::ptree ptree;
    if (!RPCEnforcer(json, ptree)) {
        LogPrintf("ERROR Sidechain client failed to request block info!\n");
        return false;
    }

    try {
        // Get the result object from the JSON-RPC response
        boost::property_tree::ptree result = ptree.get_child("result");
        
        // Get the infos array
        auto infos_array = result.get_child("infos");
        
        // Check if we have at least one info entry
        if (infos_array.empty()) {
            LogPrintf("ERROR: No block info found in response\n");
            return false;
        }
        
        // Get the first info entry (we expect only one for a specific block)
        auto first_info = infos_array.begin()->second;
        
        // Parse header_info
        auto header_info = first_info.get_child("header_info");
        blockInfo.header_info.block_hash = header_info.get("block_hash", "");
        blockInfo.header_info.prev_block_hash = header_info.get("prev_block_hash", "");
        blockInfo.header_info.height = header_info.get("height", 0);
        blockInfo.header_info.work = header_info.get("work", "");
        
        // Parse block_info
        auto block_info = first_info.get_child("block_info");
        blockInfo.block_info.bmm_commitment = block_info.get("bmm_commitment", "");
        
        // Parse events array if present
        auto events_optional = block_info.get_child_optional("events");
        if (events_optional) {
            for (const auto& event : *events_optional) {
                blockInfo.block_info.events.push_back(event.second.get_value<std::string>());
            }
        }
        
        LogPrintf("Successfully retrieved block info for block %s (height %d)\n", 
                  blockInfo.header_info.block_hash.c_str(), blockInfo.header_info.height);
        
        return true;
        
    } catch (const std::exception& e) {
        LogPrintf("ERROR parsing block info response: %s\n", e.what());
        return false;
    }
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
        
        // Iterate through the result array - each item is now a direct deposit object
        for (const auto& deposit_item : result_array) {
            if (deposit_item.second.empty()) continue;
            
            SidechainDeposit deposit;
            
            // Parse the deposit object directly (no more array format)
            auto deposit_data = deposit_item.second;
            
            // Parse basic transaction fields
            deposit.txid = deposit_data.get("txid", "");
            deposit.fee = deposit_data.get("fee", 0);
            deposit.received = deposit_data.get("received", 0);
            deposit.sent = deposit_data.get("sent", 0);
            
            // Parse sidechain number and deposit amount
            deposit.sidechain_number = deposit_data.get("sidechain_number", 0);
            deposit.deposit_amount = deposit_data.get("deposit_amount", 0);
            
            // Parse destination address (hex string to bytes)
            std::string dest_addr_hex = deposit_data.get("destination_address", "");
            if (!dest_addr_hex.empty()) {
                // Convert hex string to bytes
                for (size_t i = 0; i < dest_addr_hex.length(); i += 2) {
                    if (i + 1 < dest_addr_hex.length()) {
                        std::string byte_str = dest_addr_hex.substr(i, 2);
                        uint8_t byte_val = static_cast<uint8_t>(std::stoi(byte_str, nullptr, 16));
                        deposit.destination_address.push_back(byte_val);
                    }
                }
            }
            
            // Parse the full wallet transaction data from the 'tx' object
            auto tx_data = deposit_data.get_child_optional("tx");
            if (tx_data) {
                deposit.wallet_tx.version = tx_data->get("version", 0);
                deposit.wallet_tx.lock_time = tx_data->get("lock_time", 0);
                
                // Parse inputs
                auto inputs = tx_data->get_child_optional("input");
                if (inputs) {
                    for (const auto& input : *inputs) {
                        std::string prev_output = input.second.get("previous_output", "");
                        deposit.wallet_tx.inputs.push_back(prev_output);
                    }
                }
                
                // Parse outputs and try to extract deposit amount and sidechain info if not already set
                auto outputs = tx_data->get_child_optional("output");
                if (outputs) {
                    for (const auto& output : *outputs) {
                        std::string script_pubkey = output.second.get("script_pubkey", "");
                        deposit.wallet_tx.outputs.push_back(script_pubkey);
                        
                        // If deposit_amount is not set, try to extract it from the first non-zero output
                        if (deposit.deposit_amount == 0) {
                            uint64_t output_value = output.second.get("value", 0);
                            if (output_value > 0) {
                                deposit.deposit_amount = output_value;
                            }
                        }
                        
                        // Try to extract sidechain number from script_pubkey if not already set
                        if (deposit.sidechain_number == 0 && !script_pubkey.empty()) {
                            // Look for sidechain-related patterns in the script
                            // This is a heuristic - adjust based on your actual script format
                            if (script_pubkey.find("6a") == 0) { // OP_RETURN
                                // Try to parse sidechain number from OP_RETURN data
                                // This would need to be customized based on your specific script format
                                LogPrintf("Found OP_RETURN script: %s\n", script_pubkey.c_str());
                            }
                        }
                    }
                }
                
                // Parse witnesses (if present)
                auto inputs_with_witness = tx_data->get_child_optional("input");
                if (inputs_with_witness) {
                    for (const auto& input : *inputs_with_witness) {
                        auto witness = input.second.get_child_optional("witness");
                        if (witness) {
                            std::string witness_data;
                            for (const auto& witness_item : *witness) {
                                if (!witness_data.empty()) witness_data += ",";
                                witness_data += witness_item.second.get_value<std::string>();
                            }
                            deposit.wallet_tx.witnesses.push_back(witness_data);
                        } else {
                            deposit.wallet_tx.witnesses.push_back("");
                        }
                    }
                }
            }
            
            // Parse chain_position data
            auto chain_position = deposit_data.get_child_optional("chain_position");
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
            
            // Generate a unique ID for the deposit (since it's no longer provided in the response)
            // Using the index in the array as a fallback ID
            static int deposit_counter = 0;
            deposit.id = ++deposit_counter;
            
            // Log the parsed deposit information for debugging
            LogPrintf("Parsed deposit %d: txid=%s, sidechain=%d, amount=%llu, fee=%llu\n", 
                     deposit.id, deposit.txid.c_str(), deposit.sidechain_number, 
                     deposit.deposit_amount, deposit.fee);
            
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

bool RPCCreateBMM(const int& sidechain_id, const int64_t& value_sats, const int& height, 
                  const std::string& critical_hash, const std::string& prev_bytes, std::string& txid)
{
    // JSON for 'validator.create_bmm' enforcer HTTP-RPC
    std::string json;
    json.append("{\"jsonrpc\": \"2.0\", \"id\":1, ");
    json.append("\"method\": \"validator.create_bmm\", \"params\": ");
    json.append("[ ");
    json.append(UniValue(sidechain_id).write());
    json.append(", ");
    json.append(UniValue(value_sats).write());
    json.append(", ");
    json.append(UniValue(height).write());
    json.append(", ");
    json.append("\"" + critical_hash + "\"");
    json.append(", ");
    json.append("\"" + prev_bytes + "\"");
    json.append(" ] }");
    
    LogPrintf("RPCCreateBMM: %s\n", json.c_str());

    boost::property_tree::ptree ptree;
    if (!RPCEnforcer(json, ptree)) {
        LogPrintf("ERROR Sidechain client failed to create BMM!\n");
        return false;
    }

    try {
        // Get the result object from the JSON-RPC response
        boost::property_tree::ptree result = ptree.get_child("result");
        
        // Get the txid from the result
        txid = result.get("txid", "");
        
        if (txid.empty()) {
            LogPrintf("ERROR: No txid found in BMM creation response\n");
            return false;
        }
        
        LogPrintf("Successfully created BMM with txid: %s\n", txid.c_str());
        return true;
        
    } catch (const std::exception& e) {
        LogPrintf("ERROR parsing BMM creation response: %s\n", e.what());
        return false;
    }
}
