#ifndef TELEPORT_DEPOSITMESSAGEHANDLER_H
#define TELEPORT_DEPOSITMESSAGEHANDLER_H


#include <test/teleport_tests/node/MessageHandlerWithOrphanage.h>
#include <test/teleport_tests/node/deposits/messages/DepositAddressRequest.h>
#include <test/teleport_tests/node/config/TeleportConfig.h>
#include <test/teleport_tests/node/calendar/Calendar.h>
#include <test/teleport_tests/node/Data.h>
#include "test/teleport_tests/node/deposits/messages/DepositAddressPartMessage.h"
#include "test/teleport_tests/node/deposits/messages/WithdrawalComplaint.h"
#include "test/teleport_tests/node/deposits/messages/TransferAcknowledgement.h"
#include "test/teleport_tests/node/deposits/messages/DepositDisclosureComplaint.h"
#include "test/teleport_tests/node/deposits/messages/DepositAddressPartComplaint.h"
#include "test/teleport_tests/node/handler_modes.h"
#include "DepositAddressRequestHandler.h"
#include "DepositAddressDisclosureHandler.h"
#include "DepositAddressTransferHandler.h"
#include "DepositAddressWithdrawalHandler.h"


class TeleportNetworkNode;
class MinedCreditMessageBuilder;


class DepositMessageHandler : public MessageHandlerWithOrphanage
{
public:
    std::string channel{"deposit"};
    uint8_t mode{LIVE};

    CreditSystem *credit_system{NULL};
    Calendar *calendar{NULL};
    BitChain *spent_chain{NULL};
    MinedCreditMessageBuilder *builder{NULL};
    TeleportNetworkNode *teleport_network_node{NULL};
    TeleportConfig config;
    Data data;

    DepositAddressRequestHandler address_request_handler;
    DepositAddressDisclosureHandler address_disclosure_handler;
    DepositAddressTransferHandler address_transfer_handler;
    DepositAddressWithdrawalHandler address_withdrawal_handler;

    bool do_spot_checks{true}, using_internal_tip_controller{true}, using_internal_builder{true};

    explicit DepositMessageHandler(Data data):
        data(data), MessageHandlerWithOrphanage(data.msgdata),
        address_request_handler(this),
        address_disclosure_handler(this),
        address_transfer_handler(this),
        address_withdrawal_handler(this)
    { }

    void SetConfig(TeleportConfig& config_);

    void SetCalendar(Calendar& calendar_);

    void SetSpentChain(BitChain& spent_chain_);

    void SetNetworkNode(TeleportNetworkNode *teleport_network_node_);

    void SetMinedCreditMessageBuilder(MinedCreditMessageBuilder *builder);

    void HandleMessage(CDataStream ss, CNode* peer)
    {
        HANDLESTREAM(DepositAddressRequest);
        HANDLESTREAM(DepositAddressPartMessage);
        HANDLESTREAM(DepositAddressPartDisclosure);
        HANDLESTREAM(DepositDisclosureComplaint);
        HANDLESTREAM(DepositTransferMessage);
        HANDLESTREAM(TransferAcknowledgement);
        HANDLESTREAM(WithdrawalRequestMessage);
        HANDLESTREAM(WithdrawalMessage);
        HANDLESTREAM(WithdrawalComplaint);
    }

    void HandleMessage(uint160 message_hash)
    {
        HANDLEHASH(DepositAddressRequest);
        HANDLEHASH(DepositAddressPartMessage);
        HANDLEHASH(DepositAddressPartComplaint);
        HANDLEHASH(DepositAddressPartDisclosure);
        HANDLEHASH(DepositDisclosureComplaint);
        HANDLEHASH(DepositTransferMessage);
        HANDLEHASH(TransferAcknowledgement);
        HANDLEHASH(WithdrawalRequestMessage);
        HANDLEHASH(WithdrawalMessage);
        HANDLEHASH(WithdrawalComplaint);
    }
    
    HANDLECLASS(DepositAddressRequest);
    HANDLECLASS(DepositAddressPartMessage);
    HANDLECLASS(DepositAddressPartComplaint);
    HANDLECLASS(DepositAddressPartDisclosure);
    HANDLECLASS(DepositDisclosureComplaint);
    HANDLECLASS(DepositTransferMessage);
    HANDLECLASS(TransferAcknowledgement);
    HANDLECLASS(WithdrawalRequestMessage);
    HANDLECLASS(WithdrawalMessage);
    HANDLECLASS(WithdrawalComplaint);

    void HandleDepositAddressRequest(DepositAddressRequest request);

    void HandleDepositAddressPartMessage(DepositAddressPartMessage part_message);

    void HandleDepositAddressPartComplaint(DepositAddressPartComplaint complaint);

    void HandleDepositAddressPartDisclosure(DepositAddressPartDisclosure disclosure);

    void HandleDepositDisclosureComplaint(DepositDisclosureComplaint complaint);

    void HandleDepositTransferMessage(DepositTransferMessage transfer);

    void HandleTransferAcknowledgement(TransferAcknowledgement acknowledgement);

    void HandleWithdrawalRequestMessage(WithdrawalRequestMessage request);

    void HandleWithdrawalMessage(WithdrawalMessage withdrawal_message);

    void HandleWithdrawalComplaint(WithdrawalComplaint complaint);

    void HandleNewTip(MinedCreditMessage &msg);

    void CancelRequest(uint160 request_hash);

    uint64_t GetRespondingRelay(Point deposit_address);

    void AddAddress(Point address_pubkey, vch_t currency);

    void RemoveAddress(Point address, vch_t currency);

    std::vector<Point> MyDepositAddressPublicKeys(std::string currency_code);

    std::vector<uint64_t> GetRelaysForAddressRequest(uint160 request_hash, uint160 encoding_message_hash);

    std::vector<uint64_t> GetRelaysForAddressPubkey(Point address_pubkey);

    std::vector<Point> MyDepositAddressPoints(std::string currency_code);

    RelayState GetRelayStateOfEncodingMessage(Point address_pubkey);

    vch_t GetCurrencyCode(Point deposit_address_pubkey);

    void AddAddress(Point address);

    void RemoveAddress(Point address);

    void StoreRecipientOfDepositAddress(Point &recipient_pubkey, Point &deposit_address_pubkey);

    std::vector<Point> DepositAddressPubkeysOwnedByRecipient(Point &recipient_pubkey);

    void ChangeStoredRecipientOfDepositAddress(Point &old_recipient_pubkey, Point &deposit_address_pubkey,
                                               Point &new_recipient_pubkey);

    void RemoveStoredRecipientOfDepositAddress(Point &recipient_pubkey, Point &deposit_address_pubkey);

    void AddDepositAddressesOwnedBySpecifiedPubKey(Point public_key);
};


#endif //TELEPORT_DEPOSITMESSAGEHANDLER_H