#ifndef TELEPORT_TRANSFERACKNOWLEDGEMENT_H
#define TELEPORT_TRANSFERACKNOWLEDGEMENT_H


#include "DepositTransferMessage.h"
#include "../../relays/structures/RelayState.h"

#include "log.h"
#define LOG_CATEGORY "TransferAcknowledgement"

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
        Point deposit_address = GetTransfer(data).deposit_address_pubkey;
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
        std::vector<uint64_t> relays = Relays(data);
//        for (uint32_t i = 0; i < disqualifications.size(); i++)
//        {
//            uint160 ack_hash1 = disqualifications[i].first;
//            uint160 ack_hash2 = disqualifications[i].second;
//
//            TransferAcknowledgement ack1, ack2;
//            ack1 = data.GetMessage(ack_hash1);
//            ack2 = data.GetMessage(ack_hash2);
//
//            if (ack1.VerificationKey(data) != relays[i] or
//                ack2.VerificationKey(data) != relays[i])
//                return false;
//
//            DepositTransferMessage transfer1 = ack1.GetTransfer(data);
//            DepositTransferMessage transfer2 = ack2.GetTransfer(data);
//
//            if (transfer1.previous_transfer_hash !=
//                transfer2.previous_transfer_hash)
//                return false;
//
//            if (transfer1.deposit_address_pubkey != transfer2.deposit_address_pubkey)
//                return false;
//
//            if (transfer1.recipient_pubkey == transfer2.recipient_pubkey)
//                return false;
//        }
        return true;
    }

    std::vector<uint64_t> Relays(Data data)
    {
        return data.depositdata[GetDepositAddressPubkey(data)]["relay_numbers"];
    }

    Point GetDepositAddressPubkey(Data data)
    {
        return GetTransfer(data).deposit_address_pubkey;
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
        std::vector<uint64_t> relay_numbers = Relays(data);
        log_ << "VerificationKey: relay_numbers are " << relay_numbers << "\n";
        Relay *relay = data.relay_state->GetRelayByNumber(relay_numbers[0]);
        if (relay == NULL)
        {
            log_ << "can't find relay " << relay_numbers[0] << "\n";
            return Point(SECP256K1, 0);
        }
        return relay->public_signing_key;
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};


#endif //TELEPORT_TRANSFERACKNOWLEDGEMENT_H
