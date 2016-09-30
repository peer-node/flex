#include "teleportnode/teleportnode.h"

#include "log.h"
#define LOG_CATEGORY "datamessages.cpp"

using namespace std;

#define MATCHING_CREDITS_REQUEST_MEMORY_FACTOR 15     // 256 Mb
#define MATCHING_CREDITS_REQUEST_NUMBER_OF_LINKS 2048
#define MATCHING_CREDITS_REQUEST_NUMBER_OF_SEGMENTS 8
#define LOG_OF_MATCHING_CREDITS_REQUEST_TARGET 109


/***********************************
 *  MatchingCreditsRequestMessage
 */

    MatchingCreditsRequestMessage::MatchingCreditsRequestMessage(
        uint64_t since_when):
        since_when(since_when)
    {
        now = GetTimeMicros();
        SetFilter();
    }

    void MatchingCreditsRequestMessage::SetFilter()
    {
        vector<uint160> key_hashes = teleportnode.wallet.GetKeyHashes();
        stubs.resize(0);

        foreach_(uint160 key_hash, key_hashes)
        {
            uint32_t stub = GetStub(key_hash);
            if (!VectorContainsEntry(stubs, stub))
                stubs.push_back(stub);
        }
    }

    void MatchingCreditsRequestMessage::DoWork()
    {
        proof = TwistWorkProof();
        uint160 target(1), 
                link_threshold(MATCHING_CREDITS_REQUEST_NUMBER_OF_LINKS);
        target <<= LOG_OF_MATCHING_CREDITS_REQUEST_TARGET;
        link_threshold <<= LOG_OF_MATCHING_CREDITS_REQUEST_TARGET;
        proof = TwistWorkProof(GetHash(),
                               MATCHING_CREDITS_REQUEST_MEMORY_FACTOR,
                               target,
                               link_threshold,
                               MATCHING_CREDITS_REQUEST_NUMBER_OF_SEGMENTS);
        uint8_t keep_working = 1;
        while(!proof.DoWork(&keep_working))
        {
            proof = TwistWorkProof();
            SetFilter();
            now = GetTimeMicros();
            proof = TwistWorkProof(GetHash(),
                               MATCHING_CREDITS_REQUEST_MEMORY_FACTOR,
                               target,
                               link_threshold,
                               MATCHING_CREDITS_REQUEST_NUMBER_OF_SEGMENTS);
        }
    }

    bool MatchingCreditsRequestMessage::CheckWork()
    {
        TwistWorkProof proof_ = proof;
        proof = TwistWorkProof();
        uint256 memory_seed = GetHash();
        uint160 target(1), link_threshold(MATCHING_CREDITS_REQUEST_NUMBER_OF_LINKS);
        target <<= LOG_OF_MATCHING_CREDITS_REQUEST_TARGET;
        link_threshold <<= LOG_OF_MATCHING_CREDITS_REQUEST_TARGET;

        bool result;
        if (proof_.memoryseed != memory_seed ||
            proof_.target != target ||
            proof_.link_threshold != link_threshold ||
            proof_.N_factor != MATCHING_CREDITS_REQUEST_MEMORY_FACTOR)
        {
            log_ << "CheckWork(): parameters failed\n"
                 << proof_.memoryseed << " vs " << memory_seed << "\n"
                 << proof_.target << " vs " << target << "\n"
                 << proof_.link_threshold << " vs " << link_threshold << "\n";
            result = false;
            log_ << "request is " << *this;
        }
        else
        {
            result = (proof_.DifficultyAchieved() > 0);
        }
        proof = proof_;
        return result;
    }

/*
 *  MatchingCreditsRequestMessage
 ***********************************/


/****************************
 *  MatchingCreditsMessage
 */

    MatchingCreditsMessage::MatchingCreditsMessage(
        MatchingCreditsRequestMessage& request)
    {
        request_hash = request.GetHash160();
        
        credits = GetCreditsPayingToRecipient(request.stubs,
                                              request.since_when);
    }

/*
 *  MatchingCreditsMessage
 *****************************/


/*****************
 *  DataMessage
 */

    void DataMessage::PopulateMessages(std::vector<uint160> hashes_to_send)
    {
        log_ << "PopulateMessages(): populating " << hashes_to_send << "\n";
        foreach_(const uint160& hash, hashes_to_send)
        {
            string_t type = msgdata[hash]["type"];
            if (type == "tx")
            {
                SignedTransaction tx = creditdata[hash]["tx"];
                txs.push_back(tx);
            }
            else if (type == "mined_credit")
            {
                uint160 credit_hash = hash;
                while (!IsCalend(credit_hash) && credit_hash != 0)
                {
                    MinedCreditMessage msg = creditdata[credit_hash]["msg"];
                    if (!VectorContainsEntry(mined_credit_messages, msg))
                    {
                        mined_credit_messages.push_back(msg);
                        msg.hash_list.RecoverFullHashes();
                        PopulateMessages(msg.hash_list.full_hashes);
                    }
                    credit_hash = msg.mined_credit.previous_mined_credit_hash;
                }
            }
            else
                AddMessage(hash);
        }
    }

    void DataMessage::AddMessage(uint160 message_hash)
    {
        string_t string_type = msgdata[message_hash]["type"];
        vch_t type(string_type.begin(), string_type.end());
        vch_t content = GetMessageContent(string_type, message_hash);
        if (VectorContainsEntry(relay_message_contents, content))
            return;
        relay_message_contents.push_back(content);
        relay_message_types.push_back(type);
    }

    vch_t DataMessage::GetMessageContent(string_t type, uint160 hash)
    {
        CDataStream ss(SER_NETWORK, CLIENT_VERSION);

        if (type == "join")
        {
            RelayJoinMessage msg = msgdata[hash]["join"];
            ss << msg;
        }
        if (type == "leave")
        {
            RelayLeaveMessage msg = msgdata[hash]["leave"];
            ss << msg;
        }
        if (type == "successor")
        {
            FutureSuccessorSecretMessage msg = msgdata[hash]["successor"];
            ss << msg;
        }
        if (type == "succession")
        {
            SuccessionMessage msg = msgdata[hash]["succession"];
            ss << msg;
        }
        if (type == "secret")
        {
            TradeSecretMessage msg = msgdata[hash]["secret"];
            ss << msg;
        }
        if (type == "trader_complaint")
        {
            TraderComplaint msg = msgdata[hash]["trader_complaint"];
            ss << msg;
        }
        if (type == "trader_complaint_refutation")
        {
            RefutationOfTraderComplaint msg
                = msgdata[hash]["trader_complaint_refutation"];
            ss << msg;
        }
        if (type == "relay_complaint_refutation")
        {
            RefutationOfRelayComplaint msg
                = msgdata[hash]["relay_complaint_refutation"];
            ss << msg;
        }
        if (type == "deposit_request")
        {
            DepositAddressRequest msg = msgdata[hash]["deposit_request"];
            ss << msg;
        }
        return vch_t(ss.begin(), ss.end());
    }

    void DataMessage::StoreData()
    {
        foreach_(const SignedTransaction& tx, txs)
        {
            uint160 txhash = tx.GetHash160();
            log_ << "storing transaction " << txhash << "\n";
            creditdata[txhash]["tx"] = tx;
            StoreHash(txhash);
        }
        foreach_(const MinedCreditMessage& msg, mined_credit_messages)
        {
            uint160 credit_hash = msg.mined_credit.GetHash160();
            StoreHash(credit_hash);
            initdata[credit_hash]["from_datamessage"] = true;
            if (creditdata[credit_hash].HasProperty("msg"))
            {
                log_ << "already have " << credit_hash << "\n";
                continue;
            }
            log_ << "storing mined credit msg " << credit_hash << "\n";
            StoreMinedCredit(msg.mined_credit);
            RecordBatch(msg);
            msgdata[credit_hash]["received"] = true;
        }
        foreach_(const MinedCredit mined_credit, mined_credits)
        {
            uint160 credit_hash = mined_credit.GetHash160();
            log_ << "storing mined credit " << credit_hash << "\n";
            StoreMinedCredit(mined_credit);
            msgdata[credit_hash]["received"] = true;
        }
        for(uint32_t i = 0; i < relay_message_contents.size(); i++)
        {
            string_t type(relay_message_types[i].begin(),
                          relay_message_types[i].end());

            CDataStream ss(relay_message_contents[i],
                           SER_NETWORK, CLIENT_VERSION);
            
            uint160 hash = Hash160(ss.begin(), ss.end());
            log_ << "storing relay message " << hash  
                 << " of type " << type << "\n";

            StoreHash(hash);
            
            msgdata[hash]["type"] = type;

            if (type == "join")
            {
                RelayJoinMessage msg;
                ss >> msg; 
                msgdata[hash][msg.Type().c_str()] = msg;
                relaydata[msg.relay_pubkey]["join_msg_hash"] = hash;
            }
            if (type == "leave")
            {
                RelayLeaveMessage msg;
                ss >> msg;
                msgdata[hash][msg.Type().c_str()] = msg;
            }
            if (type == "successor")
            {
                FutureSuccessorSecretMessage msg;
                ss >> msg;
                msgdata[hash][msg.Type().c_str()] = msg;
            }
            if (type == "succession")
            {
                SuccessionMessage msg;
                ss >> msg;
                msgdata[hash][msg.Type().c_str()] = msg;
            }
            if (type == "secret")
            {
                TradeSecretMessage msg;
                ss >> msg;
                msgdata[hash][msg.Type().c_str()] = msg;
            }
            if (type == "trader_complaint")
            {
                TraderComplaint msg;
                ss >> msg;
                msgdata[hash][msg.Type().c_str()] = msg;
            }
            if (type == "trader_complaint_refutation")
            {
                RefutationOfTraderComplaint msg;
                ss >> msg;
                msgdata[hash][msg.Type().c_str()] = msg;
            }
            if (type == "relay_complaint_refutation")
            {
                RefutationOfRelayComplaint msg;
                ss >> msg;
                msgdata[hash][msg.Type().c_str()] = msg;
            }
            if (type == "deposit_request")
            {
                DepositAddressRequest msg;
                ss >> msg;
                msgdata[hash][msg.Type().c_str()] = msg;
            }
            msgdata[hash]["received"] = true;
        }
    }

/*
 *  DataMessage
 *****************/

/************************
 *  InitialDataMessage
 */

    void InitialDataMessage::StoreData()
    {
        DataMessage::StoreData();
        log_ << "InitialDataMessage::StoreData: credit hash is " 
             << credit_hash << " calend hash is " << calend_hash << "\n";
        MinedCredit calend = creditdata[calend_hash]["mined_credit"];

        BitChain chain(calend_spent_chain);

        uint160 chain_hash = chain.GetHash160();

        if (calend.spent_chain_hash != chain_hash)
        {
            log_ << "InitialDataMessage::StoreData(): bad spent chain\n"
                 << chain_hash << " vs " << calend.spent_chain_hash << "\n";
        }
        else
        {
            creditdata[chain_hash]["chain"] = calend_spent_chain;
        }
        
        uint160 calend_relay_state_hash = calend_relay_state.GetHash160();
        
        if (calend.relay_state_hash != calend_relay_state_hash)
        {
            log_ << "InitialDataMessage::StoreData(): bad relay state\n"
                 << calend_relay_state_hash << " vs "
                 << calend.relay_state_hash << "\n"
                 << "calend relay state is " << calend_relay_state
                 << "and calend is" << calend;
        }    
        else
            relaydata[calend_hash]["state"] = calend_relay_state;

        if (calendardata[calend.previous_mined_credit_hash]["is_calend"])
        {
            uint160 prev_calend_hash = calend.previous_mined_credit_hash;
            relaydata[prev_calend_hash]["state"] = preceding_relay_state;
        }
    }

/*
 *  InitialDataMessage
 ************************/

vector<uint160> GetTransfersAndAcks(Point address)
{
    vector<uint160> transfer_and_ack_hashes;
    uint160 transfer_hash = depositdata[address]["latest_transfer"];
    while (transfer_hash != 0)
    {
        transfer_and_ack_hashes.push_back(transfer_hash);
        uint160 ack_hash = depositdata[transfer_hash]["acknowledgement"];
        transfer_and_ack_hashes.push_back(ack_hash);
        DepositTransferMessage transfer = msgdata[transfer_hash]["transfer"];
        transfer_hash = transfer.previous_transfer_hash;
    }
    reverse(transfer_and_ack_hashes.begin(),
            transfer_and_ack_hashes.end());
    return transfer_and_ack_hashes;
}

/**************************
 *  RequestedDataMessage
 */

    RequestedDataMessage::RequestedDataMessage(DataRequestMessage request)
    {
        std::vector<uint160> hashes_to_send;
        data_request_hash = request.GetHash160();

        foreach_(const uint160 msg_hash, request.list_expansion_failed_msgs)
        {
            log_ << "DataMessage: expanding " <<  msg_hash << "\n"
                 << "have msg: " 
                 << creditdata[msg_hash].HasProperty("msg") << "\n"
                 << "have mined credit: "
                 << creditdata[msg_hash].HasProperty("mined_credit") << "\n";
            log_ << "calendar is " << teleportnode.calendar;
            MinedCreditMessage msg = creditdata[msg_hash]["msg"];
            log_ << "recovery succeeded: " 
                 << msg.hash_list.RecoverFullHashes() << "\n";
            log_ << "num full hashes: " << msg.hash_list.full_hashes.size()
                 << " num short hashes: " << msg.hash_list.short_hashes.size()
                 << "\n";
            hashes_to_send.insert(hashes_to_send.end(),
                                  msg.hash_list.full_hashes.begin(),
                                  msg.hash_list.full_hashes.end());
        }
        foreach_(const uint160 msg_hash, request.credit_hashes)
        {
            MinedCredit mined_credit = creditdata[msg_hash]["mined_credit"];
            mined_credits.push_back(mined_credit);
        }
        hashes_to_send.insert(hashes_to_send.end(),
                              request.unknown_hashes.begin(),
                              request.unknown_hashes.end());
        foreach_(Point address, request.deposit_addresses)
        {
            log_ << "processing request for deposit address " << address
                 << "\n";
            address_histories.push_back(DepositAddressHistory(address));
        }
        PopulateMessages(hashes_to_send);
    }

    void RequestedDataMessage::StoreData()
    {
        foreach_(DepositAddressHistory history, address_histories)
        {
            history.StoreData();
        }
        DataMessage::StoreData();
    }

/*
 *  RequestedDataMessage
 **************************/
