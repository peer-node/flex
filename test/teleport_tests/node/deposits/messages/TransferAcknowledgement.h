#ifndef TELEPORT_TRANSFERACKNOWLEDGEMENT_H
#define TELEPORT_TRANSFERACKNOWLEDGEMENT_H


#include "DepositTransferMessage.h"

class TransferAcknowledgement
{
public:
    uint160 transfer_hash;
    std::vector<std::pair<uint160, uint160> > disqualifications;
    Signature signature;

    TransferAcknowledgement() { }

    TransferAcknowledgement(uint160 transfer_hash, Data data):
            transfer_hash(transfer_hash)
    {
        Point deposit_address = GetTransfer(data).deposit_address;
        std::vector<std::pair<uint160, uint160> > d = data.depositdata[deposit_address]["disqualifications"];
        disqualifications = d;
    }

    static string_t Type() { return string_t("transfer_ack"); }

    std::vector<uint160> Dependencies()
    {
        std::vector<uint160> dependencies;
        dependencies.push_back(transfer_hash);

        for (auto disqualification : disqualifications)
        {
            dependencies.push_back(disqualification.first);
            dependencies.push_back(disqualification.second);
        }

        return dependencies;
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(transfer_hash);
        READWRITE(disqualifications);
        READWRITE(signature);
    )

    bool CheckDisqualifications(Data data)
    {
        std::vector<Point> relays = Relays(data);
        for (uint32_t i = 0; i < disqualifications.size(); i++)
        {
            uint160 ack_hash1 = disqualifications[i].first;
            uint160 ack_hash2 = disqualifications[i].second;

            TransferAcknowledgement ack1, ack2;
            ack1 = data.GetMessage(ack_hash1);
            ack2 = data.GetMessage(ack_hash2);

            if (ack1.VerificationKey(data) != relays[i] ||
                ack2.VerificationKey(data) != relays[i])
                return false;

            DepositTransferMessage transfer1 = ack1.GetTransfer(data);
            DepositTransferMessage transfer2 = ack2.GetTransfer(data);

            if (transfer1.previous_transfer_hash !=
                transfer2.previous_transfer_hash)
                return false;

            if (transfer1.deposit_address != transfer2.deposit_address)
                return false;

            if (transfer1.recipient_key_hash == transfer2.recipient_key_hash)
                return false;
        }
        return true;
    }

    std::vector<Point> Relays(Data data)
    {
        //return GetRelaysForAddressPubkey(GetDepositAddressPubkey(data));
        return std::vector<Point>{};
    }

    Point GetDepositAddress(Data data)
    {
        return GetTransfer(data).deposit_address;
    }

    DepositTransferMessage GetTransfer(Data data)
    {
        return data.msgdata[transfer_hash]["transfer"];
    }

    bool Validate(Data data)
    {
        log_ << "TransferAcknowledgement::Validate: "
             << "CheckDisqualifications: " << CheckDisqualifications(data) << "\n"
             << "VerifySignature: " << VerifySignature(data) << "\n";
        return CheckDisqualifications(data) && VerifySignature(data);
    }

    Point VerificationKey(Data data)
    {
        auto num_disqualified_relays = disqualifications.size();
        std::vector<Point> relays = Relays(data);
        log_ << "VerificationKey: relays are " << relays << "\n";
        Point key = relays[num_disqualified_relays];
        log_ << "VerificationKey is " << key << "\n";
        return key;
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};


#endif //TELEPORT_TRANSFERACKNOWLEDGEMENT_H
