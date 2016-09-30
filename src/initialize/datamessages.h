#ifndef TELEPORT_DATAMESSAGES
#define TELEPORT_DATAMESSAGES


#include "database/data.h"
#include "credits/creditutil.h"
#include "teleportnode/calendar.h"
#include "relays/relays.h"
#include <deque>
#include <openssl/rand.h>
#include "relays/relaystate.h"
#include "deposit/deposits.h"

#include "log.h"
#define LOG_CATEGORY "datamessages.h"


class CalendarMessage
{
public:
    Calendar calendar;

    CalendarMessage() { }

    CalendarMessage(uint160 credit_hash)
    {
        calendar = Calendar(credit_hash);
        while (credit_hash != calendar.GetLastCalendCreditHash())
        {
            MinedCreditMessage msg = creditdata[credit_hash]["msg"];
            credit_hash = msg.mined_credit.previous_mined_credit_hash;
        }
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(calendar);
    )

    string_t ToString() const
    {
        std::stringstream ss;
        ss << "\n============== Calendar Message =============" << "\n"
           << "== " << calendar.ToString()
           << "== \n"
           << "============ End Calendar Message ===========" << "\n";
        return ss.str();
    }
};


class DiurnBranchMessage
{
public:
    std::vector<uint160> credit_hashes;
    std::vector<std::vector<uint160> > branches;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(credit_hashes);
        READWRITE(branches);
    )

    string_t ToString() const
    {
        std::stringstream ss;
        ss << "\n============== Diurn Branch Message =============" << "\n"
           << "== " << "\n"
           << "== Credit Hashes:" << "\n";
        for (uint32_t i = 0; i < credit_hashes.size(); i++)
        {
            ss << "== " << credit_hashes[i].ToString() << "\n";
            if (i < branches.size())
            {
                for (uint32_t j = 0; j < branches[i].size(); j++)
                    ss << "==   " << branches[i][j].ToString() << "\n";
            }
        }
        ss << "============ End Diurn Branch Message ===========" << "\n";
        return ss.str();
    }
};

class MatchingCreditsRequestMessage
{
public:
    std::vector<uint32_t> stubs;
    uint64_t since_when;
    uint64_t now;
    TwistWorkProof proof;

    MatchingCreditsRequestMessage() { }

    MatchingCreditsRequestMessage(uint64_t since_when);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(stubs);
        READWRITE(since_when);
        READWRITE(now);
        READWRITE(proof);
    )

    string_t ToString()
    {
        std::stringstream ss;
        ss << "\n======== Matching Credits Request Message ========" << "\n"
           << "== Request Hash: " << GetHash160().ToString() << "\n"
           << "== " << "\n"
           << "== Now:" << now << "\n"
           << "== Since: " << since_when << "\n"
           << "== Proof hash: " << proof.GetHash().ToString() << "\n"
           << "======= End Matching Credits Request Message =======" << "\n";
        return ss.str();
    }

    void SetFilter();
    
    void DoWork();

    bool CheckWork();

    uint160 GetHash160() const
    {
        CDataStream ss(SER_NETWORK, CLIENT_VERSION);
        ss << *this;
        return ::Hash160(ss.begin(), ss.end());
    }

    uint256 GetHash() const
    {
        CDataStream ss(SER_NETWORK, CLIENT_VERSION);
        ss << *this;
        return ::Hash(ss.begin(), ss.end());
    }
};

class MatchingCreditsMessage
{
public:
    uint160 request_hash;
    std::vector<CreditInBatch> credits;

    string_t ToString() const
    {
        std::stringstream ss;
        ss << "\n============== Matching Credits Message =============" << "\n"
           << "== Request Hash: " << request_hash.ToString() << "\n"
           << "== " << "\n"
           << "== CreditInBatches:" << "\n";
        for (uint32_t i = 0; i < credits.size(); i++)
        {
            ss << "== " << credits[i].ToString() << "\n";
        }
        ss << "============ End Matching Credits Message ===========" << "\n";
        return ss.str();
    }

    MatchingCreditsMessage() { }

    MatchingCreditsMessage(MatchingCreditsRequestMessage& request);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(request_hash);
        READWRITE(credits);
    )
};

class DataRequestMessage
{
public:
    std::vector<uint160> list_expansion_failed_msgs;
    std::vector<uint160> unknown_hashes;
    std::vector<uint160> credit_hashes;
    std::vector<Point> deposit_addresses;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(list_expansion_failed_msgs);
        READWRITE(unknown_hashes);
        READWRITE(credit_hashes);
        READWRITE(deposit_addresses);
    )

    uint160 GetHash160() const
    {
        CDataStream ss(SER_NETWORK, CLIENT_VERSION);
        ss << *this;
        return ::Hash160(ss.begin(), ss.end());
    }

    operator bool() 
    {
        return list_expansion_failed_msgs.size()
                + unknown_hashes.size()
                + credit_hashes.size() 
                + deposit_addresses.size() > 0;
    }

    string_t ToString() const
    {
        std::stringstream ss;
        ss << "\n============== Data Request =============" << "\n"
           << "== Failed Expansion Messages:" << "\n";
        foreach_(uint160 hash, list_expansion_failed_msgs)
            ss << "== " << hash.ToString() << "\n";
        ss << "== " << "\n"
           << "== Unknown Hashes:" << "\n";
        foreach_(uint160 hash, unknown_hashes)
            ss << "== " << hash.ToString() << "\n";
        ss << "== " << "\n"
           << "== Credit Hashes:" << "\n";
        foreach_(uint160 hash, credit_hashes)
            ss << "== " << hash.ToString() << "\n";
        ss << "== " << "\n"
           << "== Deposit Addresses:" << "\n";
        foreach_(Point address, deposit_addresses)
            ss << "== " << address.ToString() << "\n";
        ss << "== " << "\n"
           << "== Hash: " << GetHash160().ToString() << "\n"
           << "============ End Data Request ===========" << "\n";
        return ss.str();
    }
};

class DataMessage
{
public:
    std::vector<MinedCreditMessage> mined_credit_messages;
    std::vector<MinedCredit> mined_credits;
    std::vector<SignedTransaction> txs;
    std::vector<vch_t> relay_message_types;
    std::vector<vch_t> relay_message_contents;

    DataMessage() { }

    void PopulateMessages(std::vector<uint160> hashes_to_send);

    vch_t GetMessageContent(string_t type, uint160 hash);

    void AddMessage(uint160 message_hash);

    void StoreData();
};

class InitialDataMessage : public DataMessage
{
public:
    uint160 credit_hash;
    uint160 calend_hash;
    BitChain calend_spent_chain;
    RelayState calend_relay_state;
    RelayState preceding_relay_state;

    InitialDataMessage() { }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(credit_hash);
        READWRITE(calend_hash);
        READWRITE(calend_spent_chain);
        READWRITE(calend_relay_state);
        READWRITE(preceding_relay_state);
        READWRITE(mined_credit_messages);
        READWRITE(mined_credits);
        READWRITE(txs);
        READWRITE(relay_message_types);
        READWRITE(relay_message_contents);
    )

    void StoreData();

    string_t ToString() const
    {
        stringstream ss;
        MinedCredit calend = creditdata[credit_hash]["mined_credit"];
        ss << "\n============== Initial Data Message =============" << "\n"
           << "== Credit Hash:" << credit_hash.ToString() << "\n"
           << "== " << "\n"
           << "== Calend: " << calend.ToString()
           << "== " << "\n"
           << "== Enclosed Spent Chain Hash: "
           << BitChain(calend_spent_chain).GetHash160().ToString() << "\n"
           << "== Enclosed Relay State Hash: "
           << calend_relay_state.GetHash160().ToString() << "\n"
           << "== Preceding Relay State Hash: "
           << preceding_relay_state.GetHash160().ToString() << "\n"
           << "== " << "\n"
           << "== Mined Credit Messages:" << "\n";
        foreach_(MinedCreditMessage msg, mined_credit_messages)
            ss << "== " << msg.mined_credit.GetHash160().ToString() 
               << " (" << msg.mined_credit.batch_number << ")  "
               << string_t(creditdata[
                    msg.mined_credit.GetHash160()
                            ].HasProperty("mined_credit")
                    ? "stored" : "not stored") << "\n";
        ss << "== " << "\n"
           << "== Mined Credits:" << "\n";
        foreach_(MinedCredit credit, mined_credits)
            ss << "== " << credit.GetHash160().ToString() 
               << " (" << credit.batch_number << ")\n";
        ss << "== " << "\n"
           << "== Transactions:" << "\n";
        foreach_(SignedTransaction tx, txs)
            ss << "== " << tx.GetHash160().ToString() << "\n";
        ss << "== " << "\n"
           << "== Relay Messages:" << "\n";
        for (uint32_t i = 0; i < relay_message_contents.size(); i++)
        {
            uint160 hash = Hash160(relay_message_contents[i].begin(),
                                   relay_message_contents[i].end());
            ss << "== " << hash.ToString() << " "
               << string_t(relay_message_types[i].begin(),
                           relay_message_types[i].end()) << "\n";
        }
        ss << "== " << "\n"
           << "============ End Initial Data Message ===========" << "\n";

        return ss.str();  
    }

    InitialDataMessage(uint160 credit_hash):
        credit_hash(credit_hash)
    {
        log_ << "InitialDataMessage(): " << credit_hash << "\n";
        calend_hash = GetCalendCreditHash(credit_hash);
        calend_relay_state = GetRelayState(calend_hash);
        uint160 preceding_credit_hash = PreviousCreditHash(calend_hash);
        calend_relay_state.latest_credit_hash = preceding_credit_hash;
        BitChain spent_chain;
        if (calend_hash != 0)
            spent_chain = GetSpentChain(calend_hash);
        calend_spent_chain = spent_chain;

        if (calendardata[preceding_credit_hash]["is_calend"])
        {
            preceding_relay_state = GetRelayState(preceding_credit_hash);
            preceding_relay_state.latest_credit_hash
                = PreviousCreditHash(preceding_credit_hash);
            log_ << "adding relay state of preceding calend "
                 << preceding_credit_hash << " namely " 
                 << preceding_relay_state << "\n";
        }
        else
        {
            log_ << "preceding batch " << preceding_credit_hash
                 << " was not a calend.\n";
        }

        std::set<uint160> included_msgs;

        uint160 hash = credit_hash;
        while (hash != 0)
        {
            MinedCreditMessage msg = creditdata[hash]["msg"];
            if (!VectorContainsEntry(mined_credit_messages, msg))
                mined_credit_messages.push_back(msg);
            included_msgs.insert(msg.mined_credit.GetHash160());
            log_ << "InitialDataMessage: populating with contents of "
                 << hash << "\n";
            msg.hash_list.RecoverFullHashes();
            PopulateMessages(msg.hash_list.full_hashes);
            hash = msg.mined_credit.previous_mined_credit_hash;
            
            log_ << "InitialDataMessage: populated " 
                 << msg.mined_credit.GetHash160()
                 << "; moving on to " << hash << " and waiting for " 
                 << calend_hash << "\n";
            if (hash == calend_hash)
            {
                MinedCredit calend = creditdata[calend_hash]["mined_credit"];
                uint160 prev_hash = calend.previous_mined_credit_hash;
                MinedCredit pre_calend_credit;
                pre_calend_credit = creditdata[prev_hash]["mined_credit"];
                mined_credits.push_back(pre_calend_credit);
                MinedCreditMessage msg = creditdata[hash]["msg"];
                msg.hash_list.RecoverFullHashes();
                PopulateMessages(msg.hash_list.full_hashes);
                break;
            }
        }

        MinedCredit credit;
        foreach_(HashToPointMap::value_type item,
                 calend_relay_state.join_hash_to_relay)
        {
            uint160 join_hash = item.first;
            log_ << "InitialDataMessage: adding join " << join_hash << "\n";
            AddMessage(join_hash);
            RelayJoinMessage join = msgdata[join_hash]["join"];
            if (included_msgs.count(join.credit_hash) == 0)
            {
                credit = creditdata[join.credit_hash]["mined_credit"];
                log_ << " also adding credit " << credit;
                mined_credits.push_back(credit);
            }
        }
        foreach_(PointToMsgMap::value_type item,
                 calend_relay_state.subthreshold_succession_msgs)
        {
            std::set<uint160> succession_hashes = item.second;
            
            foreach_(const uint160 succession_hash, succession_hashes)
            {
                log_ << "InitialDataMessage: adding succession " 
                     << succession_hash << "\n";
                AddMessage(succession_hash);
            }
        }
        foreach_(PointToMsgMap::value_type item,
                 calend_relay_state.disqualifications)
        {
            std::set<uint160> message_hashes = item.second;
            
            foreach_(const uint160 message_hash, message_hashes)
            {
                log_ << "InitialDataMessage: adding disqualification " 
                     << message_hash << "\n";
                AddMessage(message_hash);
            }
        }
    }
};


class RequestedDataMessage : public DataMessage
{
public:
    uint160 data_request_hash;
    std::vector<DepositAddressHistory> address_histories;

    RequestedDataMessage() { }

    RequestedDataMessage(DataRequestMessage request);
    
    void StoreData();

    IMPLEMENT_SERIALIZE
    (
        READWRITE(data_request_hash);
        READWRITE(mined_credit_messages);
        READWRITE(mined_credits);
        READWRITE(txs);
        READWRITE(relay_message_types);
        READWRITE(relay_message_contents);
        READWRITE(address_histories);
    )

    string_t ToString() const
    {
        stringstream ss;
        ss << "\n============== Requested Data Message =============" << "\n"
           << "== Data Request Hash:" << data_request_hash.ToString() 
           << "\n"
           << "== " << "\n"
           << "== Mined Credit Messages:" << "\n";
        foreach_(MinedCreditMessage msg, mined_credit_messages)
            ss << "== " << msg.mined_credit.GetHash160().ToString() << "\n";
        ss << "== " << "\n"
           << "== Mined Credits:" << "\n";
        foreach_(MinedCredit credit, mined_credits)
            ss << "== " << credit.GetHash160().ToString() << "\n";
        ss << "== " << "\n"
           << "== Transactions:" << "\n";
        foreach_(SignedTransaction tx, txs)
            ss << "== " << tx.GetHash160().ToString() << "\n";
        ss << "== " << "\n"
           << "== Relay Messages:" << "\n";
        for (uint32_t i = 0; i < relay_message_contents.size(); i++)
        {
            uint160 hash = Hash160(relay_message_contents[i].begin(),
                                   relay_message_contents[i].end());
            ss << "== " << hash.ToString() << " "
               << string_t(relay_message_types[i].begin(),
                           relay_message_types[i].end()) << "\n";
        }
        ss << "== " << "relay states: \n";
        foreach_(DepositAddressHistory history, address_histories)
            ss << "== " << history.deposit_address.ToString() 
               << "\n";
        ss << "== " << "\n"
           << "============ End Requested Data Message ===========" << "\n";
        return ss.str();  
    }
};


#endif