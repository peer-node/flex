#ifndef TELEPORT_DEPOSITADDRESSREQUESTHANDLER_H
#define TELEPORT_DEPOSITADDRESSREQUESTHANDLER_H

#include <test/teleport_tests/node/Data.h>
#include <test/teleport_tests/node/TeleportNetworkNode.h>
#include "test/teleport_tests/node/deposits/messages/DepositAddressPartMessage.h"
#include "test/teleport_tests/node/deposits/messages/WithdrawalComplaint.h"
#include "test/teleport_tests/node/deposits/messages/TransferAcknowledgement.h"
#include "test/teleport_tests/node/deposits/messages/DepositDisclosureComplaint.h"
#include "test/teleport_tests/node/deposits/messages/DepositAddressPartComplaint.h"
#include "test/teleport_tests/node/handler_modes.h"

class DepositMessageHandler;

class DepositAddressRequestHandler
{
public:
    DepositMessageHandler *deposit_message_handler{NULL};
    TeleportNetworkNode *teleport_network_node{NULL};
    Scheduler scheduler;
    Data data;

    explicit DepositAddressRequestHandler(DepositMessageHandler *deposit_message_handler);

    void AddScheduledTasks();

    void HandleDepositAddressRequest(DepositAddressRequest request);
    bool ValidateDepositAddressRequest(DepositAddressRequest &request);
    void AcceptDepositAddressRequest(DepositAddressRequest &request);

    void HandleDepositAddressPartMessage(DepositAddressPartMessage part_message);
    bool ValidateDepositAddressPartMessage(DepositAddressPartMessage &part_message);
    void AcceptDepositAddressPartMessage(DepositAddressPartMessage &part_message);

    void HandleEncodedRequests(MinedCreditMessage &msg);

    void HandleEncodedRequest(uint160 request_hash, uint160 encoding_credit_hash);

    void HandlePostEncodedRequests(MinedCreditMessage &msg);

    void HandlePostEncodedRequest(uint160 request_hash, uint160 post_encoding_credit_hash);

    void DoScheduledAddressRequestTimeoutCheck(uint160 request_hash);

    std::vector<uint64_t> GetRelaysForAddressRequest(uint160 request_hash, uint160 encoding_credit_hash);

    std::vector<uint64_t> GetRelaysForAddress(Point address);

    void HandleDepositAddressParts(uint160 request_hash, std::vector<uint160> part_hashes);

    void ScheduleCheckForResponsesToRequest(uint160 request_hash, uint160 encoding_credit_hash);

    void SendDepositAddressPartMessages(uint160 request_hash, uint160 encoding_credit_hash);

    bool RequestHasAlreadyBeenRespondedTo(uint160 request_hash, uint160 encoding_credit_hash, uint32_t position);

    void SendDepositAddressPartMessage(uint160 request_hash, uint160 encoding_credit_hash, uint32_t position);

    bool RequestShouldBeRespondedTo(uint160 request_hash, uint160 encoding_credit_hash,
                                    std::vector<uint64_t> relay_numbers, uint32_t position);

    Point GetDepositAddressPubKey(DepositAddressRequest request, std::vector<uint160> part_hashes);

    void HandleMyDepositAddressParts(Point address, DepositAddressRequest request, uint160 request_hash);

    void RecordPubKeyForDespositAddress(Point address);

    bool AllPartsHaveBeenReceived(Point address);

    bool PartHasBeenReceived(Point address, uint32_t position);

    void DoSuccessionForNonRespondingRelays(uint160 request_hash, uint160 encoding_message_hash);

    void RecordReceiptOfPart(Point address, uint32_t position);

    void SendDepositAddressPartMessage(uint160 request_hash, uint160 encoding_credit_hash, uint32_t position,
                                       std::vector<uint64_t> relay_numbers);

    bool PartHasBeenReceived(uint160 request_hash, uint160 encoding_message_hash, uint32_t position);

    void RecordReceiptOfPart(uint160 request_hash, uint160 encoding_message_hash, uint32_t position);

    bool AllPartsHaveBeenReceived(uint160 request_hash, uint160 encoding_message_hash);
};


#endif //TELEPORT_DEPOSITADDRESSREQUESTHANDLER_H
