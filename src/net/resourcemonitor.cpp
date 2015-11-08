#include "flexnode/flexnode.h"
#include "net/resourcemonitor.h"

#include "log.h"
#define LOG_CATEGORY "resourcemonitor.cpp"

extern bool fRequestShutdown;

const char* trade_message_types[] = {"order",
                                     "accept_order",
                                     "cancel_order",
                                     "commit",
                                     "accept_commit",
                                     "disclosure",
                                     "payment_proof",
                                     "acknowledgement",
                                     "third_party_confirmation",
                                     "confirmation",
                                     "secret",
                                     "backup_secret",
                                     "counterparty_secret",
                                     "counterparty_complaint",
                                     "relay_complaint",
                                     "relay_complaint_refutation",
                                     "trader_complaint",
                                     "trader_complaint_refutation",
                                     "new_relays",
                                     "trader_leave"};

const char* relay_message_types[] = {"join",
                                     "join_complaint_successor",
                                     "successor_complaint_refutation",
                                     "join_complaint_executor",
                                     "executor_complaint_refutation",
                                     "succession",
                                     "succession_complaint",
                                     "succession_complaint_refutation",
                                     "successor",
                                     "complaint_future_successor",
                                     "future_successor_refutation",
                                     "relay_leave",
                                     "leave_complaint"};

const char* deposit_message_types[] = {"deposit_request",
                                       "deposit_part",
                                       "deposit_part_complaint",
                                       "deposit_part_refutation",
                                       "deposit_disclosure",
                                       "deposit_disclosure_complaint",
                                       "deposit_disclosure_refutation",
                                       "transfer",
                                       "transfer_acknowledgement",
                                       "withdraw_request",
                                       "withdraw"};

string_t Channel(string_t message_type)
{
    for (uint32_t i = 0; i < sizeof(trade_message_types) 
                        / sizeof(trade_message_types[0]); i++)
    {
        if (message_type == trade_message_types[i])
            return "trade";
    }
    for (uint32_t i = 0; i < sizeof(relay_message_types) 
                        / sizeof(relay_message_types[0]); i++)
    {
        if (message_type == relay_message_types[i])
            return "relay";
    }
    for (uint32_t i = 0; i < sizeof(deposit_message_types) 
                        / sizeof(deposit_message_types[0]); i++)
    {
        if (message_type == deposit_message_types[i])
            return "deposit";
    }
    return "";
}

#define MessageType_(ClassName, ...)                                        \
        if (type == ClassName::Type())                                      \
        {                                                                   \
            ClassName msg = msgdata[hash][ClassName::Type().c_str()];       \
            __VA_ARGS__                                                     \
        }

#define with_msg_as_instance_of_(type, hash, ...)                           \
        MessageType_(Order, __VA_ARGS__);                                   \
        MessageType_(AcceptOrder, __VA_ARGS__);                             \
        MessageType_(CancelOrder, __VA_ARGS__);                             \
        MessageType_(OrderCommit, __VA_ARGS__);                             \
        MessageType_(AcceptCommit, __VA_ARGS__);                            \
        MessageType_(SecretDisclosureMessage, __VA_ARGS__);                 \
        MessageType_(CurrencyPaymentProof, __VA_ARGS__);                    \
        MessageType_(ThirdPartyTransactionAcknowledgement, __VA_ARGS__);    \
        MessageType_(ThirdPartyPaymentConfirmation, __VA_ARGS__);           \
        MessageType_(CurrencyPaymentConfirmation, __VA_ARGS__);             \
        MessageType_(TradeSecretMessage, __VA_ARGS__);                      \
        MessageType_(BackupTradeSecretMessage, __VA_ARGS__);                \
        MessageType_(CounterpartySecretMessage, __VA_ARGS__);               \
        MessageType_(CounterpartySecretComplaint, __VA_ARGS__);             \
        MessageType_(RelayComplaint, __VA_ARGS__);                          \
        MessageType_(RefutationOfRelayComplaint, __VA_ARGS__);              \
        MessageType_(TraderComplaint, __VA_ARGS__);                         \
        MessageType_(RefutationOfTraderComplaint, __VA_ARGS__);             \
        MessageType_(NewRelayChoiceMessage, __VA_ARGS__);                   \
        MessageType_(RelayJoinMessage, __VA_ARGS__);                        \
        MessageType_(JoinComplaintFromSuccessor, __VA_ARGS__);              \
        MessageType_(RefutationOfJoinComplaintFromSuccessor, __VA_ARGS__);  \
        MessageType_(JoinComplaintFromExecutor, __VA_ARGS__);               \
        MessageType_(RefutationOfJoinComplaintFromExecutor, __VA_ARGS__);   \
        MessageType_(SuccessionMessage, __VA_ARGS__);                       \
        MessageType_(SuccessionComplaint, __VA_ARGS__);                     \
        MessageType_(RefutationOfSuccessionComplaint, __VA_ARGS__);         \
        MessageType_(FutureSuccessorSecretMessage, __VA_ARGS__);            \
        MessageType_(ComplaintFromFutureSuccessor, __VA_ARGS__);            \
        MessageType_(RefutationOfComplaintFromFutureSuccessor, __VA_ARGS__);\
        MessageType_(RelayLeaveMessage, __VA_ARGS__);                       \
        MessageType_(RelayLeaveComplaint, __VA_ARGS__);                     \
        MessageType_(TraderLeaveMessage, __VA_ARGS__);                      \
        MessageType_(DepositAddressRequest, __VA_ARGS__);                   \
        MessageType_(DepositAddressPartMessage, __VA_ARGS__);               \
        MessageType_(DepositAddressPartComplaint, __VA_ARGS__);             \
        MessageType_(DepositAddressPartRefutation, __VA_ARGS__);            \
        MessageType_(DepositAddressPartDisclosure, __VA_ARGS__);            \
        MessageType_(DepositDisclosureComplaint, __VA_ARGS__);              \
        MessageType_(DepositDisclosureRefutation, __VA_ARGS__);             \
        MessageType_(WithdrawalRequestMessage, __VA_ARGS__);                \
        MessageType_(WithdrawalMessage, __VA_ARGS__);                       \
        MessageType_(WithdrawalComplaint, __VA_ARGS__);                     \
        MessageType_(WithdrawalRefutation, __VA_ARGS__);                    \
        MessageType_(BackupWithdrawalMessage, __VA_ARGS__);                 \
        MessageType_(BackupWithdrawalComplaint, __VA_ARGS__);               \
        MessageType_(BackupWithdrawalRefutation, __VA_ARGS__);

    void OutgoingResourceLimiter::ForwardTransaction(uint160 hash)
    {
        SignedTransaction tx = creditdata[hash]["tx"];
        RelayTransaction(tx);
    }

    void OutgoingResourceLimiter::ForwardMessage(string_t type, uint160 hash)
    {
        if (type == "tx")
            return ForwardTransaction(hash);
        string_t channel = Channel(type);
        with_msg_as_instance_of_(type, hash,
            if (channel == "trade")
                flexnode.tradehandler.BroadcastMessage(msg);
            else if (channel == "relay")
                flexnode.relayhandler.BroadcastMessage(msg);
            else if (channel == "deposit")
                flexnode.deposit_handler.BroadcastMessage(msg);
            )
    }

    void OutgoingResourceLimiter::DoForwarding()
    {
        CLocationIterator<> message_scanner;
        int64_t time_elapsed, time_to_sleep, started_handling;
        uint160 hash;

        Priority priority;

        while (running)
        {
            message_scanner = msgdata.LocationIterator("forwarding");
            message_scanner.SeekEnd();
            while (message_scanner.GetPreviousObjectAndLocation(hash, priority))
            {
                started_handling = GetTimeMicros();
                string_t type = msgdata[hash]["type"];
                log_ << "DoForwarding(): found " << hash
                     << " with type " << type << "\n";
                if (!msgdata[hash]["forwarded"])
                {
                    ForwardMessage(type, hash);
                    msgdata[hash]["forwarded"] = true;
                }
                msgdata.RemoveFromLocation("forwarding", priority);
                message_scanner = msgdata.LocationIterator("forwarding");
                message_scanner.SeekEnd();
                time_elapsed = GetTimeMicros() - started_handling;
                time_to_sleep = (1000000 / MAX_MESSAGE_RATE) - time_elapsed;
                
                if (time_to_sleep > 0)
                    SleepMilliseconds(time_to_sleep / 1000);
            }
            SleepMilliseconds(100);
            if (fRequestShutdown)
                running = false;
        }
    }

    void OutgoingResourceLimiter::ClearQueue()
    {
        CLocationIterator<> message_scanner;
        Priority priority;
        uint160 hash;
        message_scanner = msgdata.LocationIterator("forwarding");
        message_scanner.SeekEnd();
        while (message_scanner.GetPreviousObjectAndLocation(hash, priority))
        {
            msgdata.RemoveFromLocation("forwarding", priority);
            message_scanner = msgdata.LocationIterator("forwarding");
            message_scanner.SeekEnd();
        }
    }
