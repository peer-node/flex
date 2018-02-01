#ifndef TELEPORT_CREDITTRACKER_H
#define TELEPORT_CREDITTRACKER_H

#include <src/credits/creditsign.h>
#include <test/teleport_tests/node/Data.h>
#include <src/credits/CreditBatch.h>
#include <test/teleport_tests/node/credit/messages/MinedCreditMessage.h>
#include "TimeAndRecipient.h"

#include "log.h"
#define LOG_CATEGORY "CreditTracker"



class CreditSystem;


class CreditTracker
{
public:
    Data data;
    CreditSystem *credit_system{nullptr};

    explicit CreditTracker(CreditSystem *credit_system);

    uint32_t GetStub(uint160 hash);

    uint160 KeyHashFromKeyData(vch_t &keydata);

    void StoreRecipientsOfCreditsInBatch(CreditBatch &batch);

    CreditInBatch GetCreditFromBatch(uint160 batch_root, uint32_t n);

    std::vector<CreditInBatch> GetCreditsPayingToRecipient(std::vector<uint32_t> stubs, uint64_t start_time);

    std::vector<CreditInBatch> GetCreditsPayingToRecipient(uint160 keyhash);

    std::vector<CreditInBatch> GetCreditsPayingToRecipient(Point &pubkey);

    void RemoveCreditsOlderThan(uint64_t time_, uint32_t stub);
};


#endif //TELEPORT_CREDITTRACKER_H
