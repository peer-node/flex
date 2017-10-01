#include "CreditTracker.h"
#include "CreditSystem.h"

#include "log.h"
#define LOG_CATEGORY "CreditTracker"

CreditTracker::CreditTracker(CreditSystem *credit_system):
    credit_system(credit_system), data(credit_system->data)
{ }


uint32_t CreditTracker::GetStub(uint160 hash)
{
    return (uint32_t)(hash.GetLow64() % (256 * 256 * 256));
}

CreditInBatch CreditTracker::GetCreditFromBatch(uint160 batch_root, uint32_t n)
{
    uint160 msg_hash = data.creditdata[batch_root]["batch_encoding_msg"];
    MinedCreditMessage msg = data.GetMessage(msg_hash);
    CreditBatch batch = credit_system->ReconstructBatch(msg);
    log_ << "reconstructed batch is: " << batch << "\n";
    if (n >= batch.credits.size())
        return CreditInBatch();
    Credit credit = batch.credits[n];
    log_ << "credit is: " << credit << "\n";
    CreditInBatch credit_in_batch = batch.GetCreditInBatch(credit);
    log_ << "credit in batch is: " << credit_in_batch << "\n";
    return credit_in_batch;
}

uint160 CreditTracker::KeyHashFromKeyData(vch_t &keydata)
{
    Point pubkey;
    pubkey.setvch(keydata);
    return KeyHash(pubkey);
}

void CreditTracker::StoreRecipientsOfCreditsInBatch(CreditBatch &batch)
{
    uint160 batch_root = batch.Root();
    uint64_t n = 0;

    for (auto credit : batch.credits)
    {
        uint64_t now = GetTimeMicros();
        log_ << "credit.keydata is " << credit.keydata << "\n";
        Point recipient_pubkey;
        recipient_pubkey.setvch(credit.keydata);
        uint160 key_hash = KeyHashFromKeyData(credit.keydata);
        log_ << key_hash << " is key_hash for " << recipient_pubkey << "\n";
        std::pair<uint160, uint64_t> batch_entry(batch_root, n);
        uint32_t stub = GetStub(key_hash);
        log_ << stub << " is stub for " << key_hash << "\n";
        data.creditdata[batch_entry].Location(stub) = TimeAndRecipient(now, key_hash);
        log_ << "stored " << batch_entry << " at " << TimeAndRecipient(now, key_hash).data << " in " << stub << "\n";
        n += 1;
    }
}

std::vector<CreditInBatch> CreditTracker::GetCreditsPayingToRecipient(std::vector<uint32_t> stubs, uint64_t start_time)
{
    std::vector<CreditInBatch> credits;

    for (auto stub : stubs)
    {
        auto credit_scanner = data.creditdata.LocationIterator(stub);

        credit_scanner.Seek(TimeAndRecipient(start_time, 0));

        TimeAndRecipient time_and_recipient;
        std::pair<uint160,uint64_t> batch_entry;
        log_ << "searching along " << stub << "\n";
        while (credit_scanner.GetNextObjectAndLocation(batch_entry, time_and_recipient))
        {
            log_ << "found " << batch_entry << " at " << time_and_recipient.data << "\n";
            uint160 batch_root = batch_entry.first;
            auto n = (uint32_t)batch_entry.second;
            uint160 recipient = time_and_recipient.Recipient();
            log_ << "key_hash is " << recipient << "\n";
            log_ << "stub for " << recipient << " is " << GetStub(recipient) << "\n";
            if (VectorContainsEntry(stubs, GetStub(recipient)))
            {
                log_ << "adding credit " << n << " from batch with root " << batch_root << "\n";
                credits.push_back(GetCreditFromBatch(batch_root, n));
            }
        }
    }
    return credits;
}

std::vector<CreditInBatch> CreditTracker::GetCreditsPayingToRecipient(uint160 key_hash)
{
    log_ << "looking for credits paying to key_hash: " << key_hash << "\n";
    std::vector<CreditInBatch> credits;

    uint32_t stub = GetStub(key_hash);
    std::vector<uint32_t> stubs;
    stubs.push_back(stub);
    std::vector<CreditInBatch> recorded_credits;
    recorded_credits = GetCreditsPayingToRecipient(stubs, 0);
    log_ << "recorded credits are: " << recorded_credits << "\n";

    for (auto credit : recorded_credits)
    {
        log_ << "considering credit: " << credit << "\n";
        uint160 recipient_key_hash = KeyHashFromKeyData(credit.keydata);
        log_ << "recipient key_hash is " << recipient_key_hash << "\n";
        if (recipient_key_hash == key_hash)
            credits.push_back(credit);
    }
    return credits;
}

std::vector<CreditInBatch> CreditTracker::GetCreditsPayingToRecipient(Point &pubkey)
{
    return GetCreditsPayingToRecipient(KeyHash(pubkey));
}

uint64_t CreditTracker::GetKnownPubKeyBalance(Point &pubkey)
{
    uint64_t balance = 0;
    uint32_t stub = GetStub(KeyHash(pubkey));
    std::vector<uint32_t> stubs;
    stubs.push_back(stub);
    std::vector<CreditInBatch> credits = GetCreditsPayingToRecipient(stubs, 0);

    for (auto &credit : credits)
    {
//            if (not teleport_network_node.spent_chain.Get(credit.position))
            balance += credit.amount;
    }
    return balance;
}

void CreditTracker::RemoveCreditsOlderThan(uint64_t time_, uint32_t stub)
{
    auto credit_scanner = data.creditdata.LocationIterator(stub);

    credit_scanner.Seek(TimeAndRecipient(time_, 0));

    TimeAndRecipient time_and_recipient;
    std::pair<uint160,uint64_t> batch_entry;

    while (credit_scanner.GetPreviousObjectAndLocation(batch_entry, time_and_recipient))
    {
        data.creditdata.RemoveFromLocation(stub, time_and_recipient);
        credit_scanner = data.creditdata.LocationIterator(stub);
        credit_scanner.Seek(TimeAndRecipient(time_, 0));
    }
}
