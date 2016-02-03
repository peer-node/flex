// Distributed under version 3 of the Gnu Affero GPL software license, 
// see the accompanying file COPYING for details.

#ifndef FLEX_DEPOSITS
#define FLEX_DEPOSITS

#define DEPOSIT_MEMORY_FACTOR 16  // 0.5 Gb
#define DEPOSIT_NUM_SEGMENTS 64   // 8 Mb
#define DEPOSIT_PROOF_LINKS 32
#define DEPOSIT_NUM_RELAYS 5
#define DEPOSIT_LOG_TARGET 109

#include "flexnode/message_handler.h"
#include "deposit/deposit_messages.h"
#include "deposit/disclosure_messages.h"
#include "deposit/transfer_messages.h"
#include "deposit/withdrawal_messages.h"

#include "log.h"
#define LOG_CATEGORY "deposits.h"

void AddAddress(Point address, vch_t currency);

void AddAndRemoveMyAddresses(uint160 transfer_hash);

void RemoveAddress(Point address, vch_t currency);

void DoScheduledDisclosureRefutationCheck(uint160 complaint_hash);

void DoScheduledPostDisclosureCheck(uint160 request_hash);

void DoScheduledDisclosureTimeoutCheck(uint160 part_msg_hash);

DepositAddressPartMessage GetPartMessageAtPosition(uint32_t position,
                                                   Point address);

void SendBackupWithdrawalsFromPartMessage(uint160 withdraw_request_hash,
                                          DepositAddressPartMessage part_msg, 
                                          uint32_t position);

void SendBackupWithdrawalsFromPartMessages(uint160 withdraw_request_hash,
                                           uint32_t position);

void SendBackupWithdrawalsFromDisclosure(
    uint160 withdraw_request_hash,
    DepositAddressPartDisclosure disclosure, 
    uint32_t position);

void SendBackupWithdrawalsFromDisclosures(uint160 withdraw_request_hash,
                                          uint32_t position);

void DoScheduledWithdrawalTimeoutCheck(uint160 withdraw_request_hash);

void MarkTaskAsCompletedByRelay(uint160 task_hash, Point relay);

void DoScheduledWithdrawalRefutationCheck(uint160 complaint_hash);

void DoScheduledBackupWithdrawalTimeoutCheck(uint160 withdraw_request_hash);

void DoScheduledBackupWithdrawalRefutationCheck(uint160 complaint_hash);

Point GetRespondingRelay(Point deposit_address);

void DoScheduledConfirmTransferCheck(uint160 ack_hash);

std::vector<Point> GetRelaysForAddress(uint160 request_hash,
                                       uint160 encoding_credit_hash);

void DoScheduledAddressRequestTimeoutCheck(uint160 request_hash);

void DoScheduledAddressPartRefutationCheck(uint160 complaint_hash);

void InitializeDepositScheduledTasks();



class DepositHandler : public MessageHandlerWithOrphanage
{
public:
    const char *channel;

    DepositHandler()
    {
        channel = "deposit";
    }

    template <typename T>
    void BroadcastMessage(T message)
    {
        CDataStream ss = GetDepositBroadcastStream(message);
        uint160 message_hash = message.GetHash160();
        log_ << "BroadcastMessage:  sending " << message_hash << "\n";
        RelayDepositMessage(ss);
        if (!depositdata[message_hash]["received"])
        {
            log_ << "BroadcastMessage: handling message\n";
            HandleMessage(ss, NULL);
        }
    }

    template <typename T>
    CDataStream GetDepositBroadcastStream(T message)
    {
        if (message.signature == Signature(0, 0))
            message.Sign();
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss.reserve(10000);
        
        string string_message_genus(channel);
        ss << string_message_genus;
        
        string string_message_species = message.Type();
        ss << string_message_species;
        
        ss << message;
        return ss;
    }

    template <typename T>
    void PushDirectlyToPeers(T message)
    {
        if (message.signature == Signature(0, 0))
            message.Sign();
        
        uint160 message_hash = message.GetHash160();
        log_ << "PushDirectlyToPeers: sending " << message_hash << "\n";
        string_t type = message.Type();

        msgdata[message_hash]["type"] = type;

        msgdata[message_hash][type] = message;

        std::vector<uint32_t> peers_who_know = depositdata[message_hash]["peers"];

        LOCK(cs_vNodes);
        foreach_(CNode* peer, vNodes)
        {
            if (VectorContainsEntry(peers_who_know, (uint32_t)peer->id))
                continue;
            peer->PushMessage(channel, type.c_str(), message);
            peers_who_know.push_back((uint32_t)peer->id);
        }
        depositdata[message_hash]["peers"] = peers_who_know;
        if (!depositdata[message_hash]["received"])
        {
            log_ << "PushDirectlyToPeers: handling message\n";
            depositdata[message_hash]["received"] = true;
            HandleMessage(message_hash);
        }
    }

    HandleClass_(DepositAddressRequest);
    HandleClass_(DepositAddressPartMessage);
    HandleClass_(DepositAddressPartComplaint);
    HandleClass_(DepositAddressPartRefutation);
    HandleClass_(DepositAddressPartDisclosure);
    HandleClass_(DepositDisclosureComplaint);
    HandleClass_(DepositDisclosureRefutation);
    HandleClass_(DepositTransferMessage);
    HandleClass_(TransferAcknowledgement);
    HandleClass_(WithdrawalRequestMessage);
    HandleClass_(WithdrawalMessage);
    HandleClass_(WithdrawalComplaint);
    HandleClass_(WithdrawalRefutation);
    HandleClass_(BackupWithdrawalMessage);
    HandleClass_(BackupWithdrawalComplaint);
    HandleClass_(BackupWithdrawalRefutation);

    void HandleMessage(CDataStream ss, CNode *peer)
    {
        string message_type;
        ss >> message_type;
        
        uint160 message_hash = Hash160(ss.begin(), ss.end());
        if (message_type == "transfer" || message_type == "transfer_ack")
        {
            std::vector<uint32_t> peers_who_know = depositdata[message_hash]["peers"];
            if (!VectorContainsEntry(peers_who_know, (uint32_t)peer->id))
                peers_who_know.push_back((uint32_t)peer->id);
            depositdata[message_hash]["peers"] = peers_who_know;
        }
        else
        {
            ss >> message_type;
            message_hash = Hash160(ss.begin(), ss.end());
        }
        
        if (depositdata[message_hash]["received"])
            return;

        depositdata[message_hash]["received"] = true;
        log_ << "handling message with hash " 
             << message_hash << " and type " << message_type << "\n";
        should_forward = true;

        handle_stream_(DepositAddressRequest, ss, peer);
        handle_stream_(DepositAddressPartMessage, ss, peer);
        handle_stream_(DepositAddressPartComplaint, ss, peer);
        handle_stream_(DepositAddressPartRefutation, ss, peer);
        handle_stream_(DepositAddressPartDisclosure, ss, peer);
        handle_stream_(DepositDisclosureComplaint, ss, peer);
        handle_stream_(DepositDisclosureRefutation, ss, peer);
        handle_stream_(DepositTransferMessage, ss, peer);
        handle_stream_(TransferAcknowledgement, ss, peer);
        handle_stream_(WithdrawalRequestMessage, ss, peer);
        handle_stream_(WithdrawalMessage, ss, peer);
        handle_stream_(WithdrawalComplaint, ss, peer);
        handle_stream_(WithdrawalRefutation, ss, peer);
        handle_stream_(BackupWithdrawalMessage, ss, peer);
        handle_stream_(BackupWithdrawalComplaint, ss, peer);
        handle_stream_(BackupWithdrawalRefutation, ss, peer);

        log_ << "finished handling " << message_hash << "\n";
    }

    void HandleMessage(uint160 message_hash)
    {
        string_t message_type = msgdata[message_hash]["type"];
        log_ << "handling message with hash " << message_hash << "\n";
            
        handle_hash_(DepositAddressRequest, message_hash);
        handle_hash_(DepositAddressPartMessage, message_hash);
        handle_hash_(DepositAddressPartComplaint, message_hash);
        handle_hash_(DepositAddressPartRefutation, message_hash);
        handle_hash_(DepositAddressPartDisclosure, message_hash);
        handle_hash_(DepositDisclosureComplaint, message_hash);
        handle_hash_(DepositDisclosureRefutation, message_hash);
        handle_hash_(DepositTransferMessage, message_hash);
        handle_hash_(TransferAcknowledgement, message_hash);
        handle_hash_(WithdrawalRequestMessage, message_hash);
        handle_hash_(WithdrawalMessage, message_hash);
        handle_hash_(WithdrawalComplaint, message_hash);
        handle_hash_(WithdrawalRefutation, message_hash);
        handle_hash_(BackupWithdrawalMessage, message_hash);
        handle_hash_(BackupWithdrawalComplaint, message_hash);
        handle_hash_(BackupWithdrawalRefutation, message_hash);
    }

    void AddBatchToTip(MinedCreditMessage& msg);

    void RemoveBatchFromTip();

    void InitiateAtCalend(uint160 calend_hash);

    void SendDepositAddressRequest(vch_t currency, uint8_t curve, bool secret);

    void HandleDepositAddressRequest(DepositAddressRequest request);

    void HandleDepositAddressPartMessage(DepositAddressPartMessage msg);
    
    bool CheckAndSaveSharesFromAddressPart(DepositAddressPartMessage msg,
                                           uint32_t position);
    
    void HandleDepositAddressParts(uint160 request_hash,
                                   std::vector<uint160> part_hashes);

    void HandleDepositAddressPartComplaint(DepositAddressPartComplaint
                                           complaint);

    void HandleDepositAddressPartRefutation(DepositAddressPartRefutation
                                            refutation);

    void HandleEncodedRequests(MinedCreditMessage& msg);

    void HandleEncodedRequest(uint160 request_hash,
                              uint160 encoding_credit_hash);

    void HandlePostEncodedRequests(MinedCreditMessage& msg);

    void HandlePostEncodedRequest(uint160 request_hash,
                                  uint160 encoding_credit_hash);

    void HandleDepositAddressPartDisclosure(DepositAddressPartDisclosure
                                            disclosure);
    
    void CheckForFinalDisclosure(DepositAddressPartDisclosure disclosure);

    void CheckAndSaveDisclosureSecrets(
        DepositAddressPartDisclosure disclosure);
    
    void HandleDepositDisclosureComplaint(DepositDisclosureComplaint
                                          complaint);

    void HandleDepositDisclosureRefutation(DepositDisclosureRefutation
                                           refutation);

    void HandleDepositTransferMessage(DepositTransferMessage transfer);
    
    void HandleConflictingAcknowledgement(TransferAcknowledgement ack,
                                          uint160 prev_transfer_hash);
    
    void SendAnotherAckIfAppropriate(TransferAcknowledgement ack);
    
    void AddDisqualification(TransferAcknowledgement ack1,
                             TransferAcknowledgement ack2);

    void HandleTransferAcknowledgement(TransferAcknowledgement
                                       acknowledgement);
    
    void SendWithdrawalRequestMessage(uint160 deposit_address_hash);

    void HandleWithdrawalRequestMessage(WithdrawalRequestMessage request);

    void HandleWithdrawalMessage(WithdrawalMessage withdrawal_msg);
    
    void HandleWithdrawalComplaint(WithdrawalComplaint complaint);

    void HandleWithdrawalRefutation(WithdrawalRefutation refutation);

    void HandleBackupWithdrawalMessage(
        BackupWithdrawalMessage backup_withdraw);

    void CheckForEnoughSecretsOfAddressPart(
        BackupWithdrawalMessage backup_withdraw);

    void RecoverSecretPart(DepositAddressPartMessage part_msg);

    void CheckForAllSecretsOfAddress(uint160 address_request_hash);

    void HandleBackupWithdrawalComplaint(BackupWithdrawalComplaint complaint);

    void HandleBackupWithdrawalRefutation(
        BackupWithdrawalRefutation refutation);

    void CancelRequest(uint160 request_hash);
};

#endif