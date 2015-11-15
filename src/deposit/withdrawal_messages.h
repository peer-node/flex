// Distributed under version 3 of the Gnu Affero GPL software license, 
// see the accompanying file COPYING for details.

#ifndef FLEX_WITHDRAWAL_MESSAGES
#define FLEX_WITHDRAWAL_MESSAGES

#include "deposit/deposit_secret.h"
#include "deposit/deposit_messages.h"
#include "deposit/disclosure_messages.h"
#include "deposit/transfer_messages.h"


#include "log.h"
#define LOG_CATEGORY "withdrawal_messages.h"

extern void AddAndRemoveMyAddresses(uint160 transfer_hash);

class WithdrawalRequestMessage
{
public:
    Point deposit_address;
    uint160 previous_transfer_hash;
    Point authorized_key;
    Point recipient_key;
    Signature signature;

    WithdrawalRequestMessage() { }

    WithdrawalRequestMessage(uint160 deposit_address_hash,
                             Point recipient_key_):
        recipient_key(recipient_key_)
    {
        log_ << "deposit address hash is " << deposit_address_hash << "\n";
        deposit_address = depositdata[deposit_address_hash]["address"];
        log_ << "deposit address is " << deposit_address << "\n";

        previous_transfer_hash
            = depositdata[deposit_address]["latest_transfer"];
        
        log_ << "WithdrawalRequestMessage: previous_transfer_hash is "
             << previous_transfer_hash << "\n";

        uint160 authorized_key_hash
            = GetPreviousTransfer().recipient_key_hash;

        authorized_key = keydata[authorized_key_hash]["pubkey"];

        log_ << "authorized_key = " << authorized_key << "\n";
        log_ << "authorized_key has privkey: "
             << keydata[authorized_key].HasProperty("privkey") << "\n";
        log_ << "WithdrawalRequestMessage: recipient_key is "
             << recipient_key << "\n";
        if (recipient_key.curve == 0 || recipient_key.IsAtInfinity())
        {
            log_ << "invalid recipient_key: using authorized_key\n";
            recipient_key = authorized_key;
        }
        log_ << "WithdrawalRequestMessage: recipient_key is "
             << recipient_key << "\n";
    }

    static string_t Type() { return string_t("withdraw_request"); }

    DEPENDENCIES(previous_transfer_hash);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(deposit_address);
        READWRITE(previous_transfer_hash);
        READWRITE(authorized_key);
        READWRITE(recipient_key);
        READWRITE(signature);
    )

    DepositTransferMessage GetPreviousTransfer()
    {
        return msgdata[previous_transfer_hash]["transfer"];
    }

    Point VerificationKey()
    {
        log_ << "WithdrawalRequestMessage: VerificationKey()\n";
        uint160 latest_transfer_hash
            = depositdata[deposit_address]["latest_transfer"];
        log_ << "latest_transfer_hash is " << latest_transfer_hash << "\n";

        if (latest_transfer_hash != previous_transfer_hash)
        {
            log_ << "latest_transfer_hash does not match "
                 << "previous_transfer_hash " << previous_transfer_hash
                 << "\n";
            return Point(SECP256K1, 0);
        }
        if (previous_transfer_hash != 0)
        {
            DepositTransferMessage previous_transfer = GetPreviousTransfer();
            if (previous_transfer.deposit_address == deposit_address &&
                previous_transfer.recipient_key_hash
                    == KeyHash(authorized_key))
            {
                log_ << "ok; returning authorized_key "
                     << authorized_key << "\n";
                return authorized_key;
            }
                
            return Point(SECP256K1, 0);
        }
        Point key = depositdata[deposit_address]["depositor_key"];
        log_ << "VerificationKey(): depositor_key for " << deposit_address 
             << " is " << key << "\n";
        return key;
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};


class WithdrawalMessage
{
public:
    uint160 withdraw_request_hash;
    uint32_t position;
    CBigNum secret_xor_shared_secret;
    Signature signature;

    WithdrawalMessage() { }

    WithdrawalMessage(uint160 withdraw_request_hash, uint32_t position):
        withdraw_request_hash(withdraw_request_hash),
        position(position)
    {
        SetSecret();
    }

    void SetSecret()
    {
        CBigNum secret =  keydata[GetAddressPart()]["privkey"];
        CBigNum shared_secret = Hash(secret * GetRequest().recipient_key);
        secret_xor_shared_secret = secret ^ shared_secret;
    }

    static string_t Type() { return string_t("withdraw"); }

    DEPENDENCIES(withdraw_request_hash);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(withdraw_request_hash);
        READWRITE(position);
        READWRITE(secret_xor_shared_secret);
        READWRITE(signature);
    )

    bool CheckAndSaveSecret()
    {
        Point recipient_key = GetRequest().VerificationKey();
        log_ << "CheckAndSaveSecret: recipient_key is " << recipient_key << "\n";
        CBigNum recipient_privkey = keydata[recipient_key]["privkey"];
        log_ << "CheckAndSaveSecret: recipient_privkey is "
             << recipient_privkey << "\n";
        Point address_part = GetAddressPart();
        log_ << "address_part = " << address_part << "\n";
        CBigNum shared_secret = Hash(address_part * recipient_privkey);
        log_ << "shared_secret is " << shared_secret << "\n";
        log_ << "secret_xor_shared_secret is " << secret_xor_shared_secret << "\n";
        CBigNum secret = secret_xor_shared_secret ^ shared_secret;
        log_ << "secret is " << secret << "\n";
        if (Point(address_part.curve, secret) != address_part)
        {
            log_ << "address_part.curve is " << address_part.curve
                 << " and Point(address_part.curve, secret) is "
                 << Point(address_part.curve, secret) << "\n";
            return false;
        }
        keydata[address_part]["privkey"] = secret;
        return true;
    }

    Point GetAddressPart()
    {
        DepositAddressPartMessage msg = GetPartMessage();
        log_ << "GetAddressPart: part message is " << msg.GetHash160() << "\n"
             << "and pubkey is " << msg.address_part_secret.PubKey() << "\n";
        return msg.address_part_secret.PubKey();
    }

    Point GetDepositAddress()
    {
        return GetRequest().deposit_address;
    }

    DepositAddressPartMessage GetPartMessage()
    {
        uint160 part_msg_hash = GetPartMessageHashes()[position];
        log_ << "GetPartMessage: partmessage hashes are: "
             << GetPartMessageHashes() << "\n"
             << "part msg hash is: " << part_msg_hash << "\n";

        return msgdata[part_msg_hash]["deposit_part"];
    }

    std::vector<uint160> GetPartMessageHashes()
    {
        Point address = GetRequest().deposit_address;
        uint160 address_request_hash = depositdata[address]["deposit_request"];
        return depositdata[address_request_hash]["parts"];
    }

    WithdrawalRequestMessage GetRequest()
    {
        return msgdata[withdraw_request_hash]["withdraw_request"];
    }

    Point VerificationKey()
    {
        Point address = GetDepositAddress();
        std::vector<Point> relays = GetRelaysForAddress(address);

        if (position >= relays.size())
            return Point(SECP256K1, 0);
        return relays[position];
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};


class WithdrawalComplaint
{
public:
    uint160 withdraw_msg_hash;
    Signature signature;

    WithdrawalComplaint() { }

    WithdrawalComplaint(uint160 withdraw_msg_hash):
        withdraw_msg_hash(withdraw_msg_hash)
    { }

    static string_t Type() { return string_t("withdraw_complaint"); }

    DEPENDENCIES(withdraw_msg_hash);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(withdraw_msg_hash);
        READWRITE(signature);
    )

    Point GetDepositAddress()
    {
        return GetWithdraw().GetDepositAddress();
    }

    WithdrawalMessage GetWithdraw()
    {
        return msgdata[withdraw_msg_hash]["withdraw"];
    }

    Point VerificationKey()
    {
        return GetWithdraw().GetRequest().recipient_key;
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};


class WithdrawalRefutation
{
public:
    uint160 complaint_hash;
    CBigNum secret;
    Signature signature;

    WithdrawalRefutation() { }

    WithdrawalRefutation(uint160 complaint_hash):
        complaint_hash(complaint_hash)
    { }

    static string_t Type() { return string_t("withdrawal_refutation"); }

    DEPENDENCIES(complaint_hash);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(complaint_hash);
        READWRITE(secret);
        READWRITE(signature);
    )

    bool Validate()
    {
        Point recipient = GetComplaint().VerificationKey();
        WithdrawalMessage withdraw = GetWithdraw();
        CBigNum shared_secret = Hash(secret * recipient);
        return (secret ^ shared_secret) == withdraw.secret_xor_shared_secret;
    }

    WithdrawalMessage GetWithdraw()
    {
        return GetComplaint().GetWithdraw();
    }

    WithdrawalComplaint GetComplaint()
    {
        return msgdata[complaint_hash]["withdraw_complaint"];
    }

    Point VerificationKey()
    {
        return GetWithdraw().VerificationKey();
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};


class BackupWithdrawalMessage
{
public:
    uint160 withdraw_request_hash;
    uint160 secret_revelation_message_hash;
    uint32_t position_of_secret;
    CBigNum secret_xor_shared_secret;
    Signature signature;

    BackupWithdrawalMessage() { }

    BackupWithdrawalMessage(uint160 withdraw_request_hash, 
                            uint160 secret_revelation_message_hash,
                            uint32_t position_of_secret):
        withdraw_request_hash(withdraw_request_hash),
        secret_revelation_message_hash(secret_revelation_message_hash),
        position_of_secret(position_of_secret)
    {
        SetSecret();
    }

    void SetSecret()
    {
        Point pubkey = GetPubKeyOfSecret();
        CBigNum secret = keydata[pubkey]["privkey"];
        CBigNum shared_secret = Hash(secret * GetRequest().VerificationKey());
        secret_xor_shared_secret = secret ^ shared_secret;
    }

    static string_t Type() { return string_t("backup_withdraw"); }

    DEPENDENCIES(withdraw_request_hash, secret_revelation_message_hash);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(withdraw_request_hash);
        READWRITE(secret_revelation_message_hash);
        READWRITE(position_of_secret);
        READWRITE(secret_xor_shared_secret);
        READWRITE(signature);
    )

    Point GetPubKeyOfSecret()
    {
        DepositAddressPartMessage msg = GetPartMessage();
        return msg.address_part_secret.point_values[position_of_secret];
    }

    bool CheckAndSaveSecret()
    {
        Point secret_pubkey = GetPubKeyOfSecret();

        Point recipient_key = GetRequest().VerificationKey();
        CBigNum recipient_privkey = keydata[recipient_key]["privkey"];

        CBigNum shared_secret = Hash(recipient_privkey * secret_pubkey);
        CBigNum secret = secret_xor_shared_secret ^ shared_secret;
        if (Point(secret_pubkey.curve, secret) != secret_pubkey)
            return false;
        keydata[secret_pubkey]["privkey"] = secret;
        return true;
    }

    Point GetDepositAddress()
    {
        return GetRequest().deposit_address;
    }

    DepositAddressPartMessage GetPartMessage()
    {
        string_t type = msgdata[secret_revelation_message_hash]["type"];

        if (type == "deposit_part")
        {
            return msgdata[secret_revelation_message_hash][type];
        }
        else if (type == "deposit_disclosure")
        {
            DepositAddressPartDisclosure disclosure;
            disclosure = msgdata[secret_revelation_message_hash][type];
            uint160 part_msg_hash = disclosure.address_part_message_hash;
            return msgdata[part_msg_hash]["deposit_part"];
        }
        return DepositAddressPartMessage();
    }

    std::vector<uint160> GetPartMessageHashes()
    {
        Point address = GetRequest().deposit_address;
        uint160 address_request_hash = depositdata[address]["deposit_request"];
        return depositdata[address_request_hash]["parts"];
    }

    WithdrawalRequestMessage GetRequest()
    {
        return msgdata[withdraw_request_hash]["withdraw_request"];
    }

    Point VerificationKey()
    {
        std::vector<Point> relays;
        string_t type = msgdata[secret_revelation_message_hash]["type"];
        if (type == "deposit_part")
        {
            DepositAddressPartMessage part_msg;
            part_msg = msgdata[secret_revelation_message_hash]["deposit_part"];
            relays = part_msg.Relays();
        }
        else
        {
            DepositAddressPartDisclosure disclosure;
            disclosure = msgdata[secret_revelation_message_hash][type];
            relays = disclosure.Relays();
        }
        if (position_of_secret > relays.size())
            return Point(SECP256K1, 0);
        return relays[position_of_secret];
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};


class BackupWithdrawalComplaint
{
public:
    uint160 backup_withdraw_msg_hash;
    Signature signature;

    BackupWithdrawalComplaint() { }

    BackupWithdrawalComplaint(uint160 backup_withdraw_msg_hash):
        backup_withdraw_msg_hash(backup_withdraw_msg_hash)
    { }

    static string_t Type() { return string_t("backup_withdraw_complaint"); }

    DEPENDENCIES(backup_withdraw_msg_hash);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(backup_withdraw_msg_hash);
        READWRITE(signature);
    )

    Point GetDepositAddress()
    {
        return GetBackupWithdraw().GetDepositAddress();
    }

    BackupWithdrawalMessage GetBackupWithdraw()
    {
        return msgdata[backup_withdraw_msg_hash]["backup_withdraw"];
    }

    Point VerificationKey()
    {
        return GetBackupWithdraw().GetRequest().recipient_key;
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};


class BackupWithdrawalRefutation
{
public:
    uint160 complaint_hash;
    CBigNum secret;
    Signature signature;

    BackupWithdrawalRefutation() { }

    BackupWithdrawalRefutation(uint160 complaint_hash):
        complaint_hash(complaint_hash)
    { }

    static string_t Type() { return string_t("backup_withdrawal_refutation"); }

    DEPENDENCIES(complaint_hash);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(complaint_hash);
        READWRITE(secret);
        READWRITE(signature);
    )

    bool Validate()
    {
        Point recipient = GetComplaint().VerificationKey();
        BackupWithdrawalMessage withdraw = GetBackupWithdraw();
        CBigNum shared_secret = Hash(secret * recipient);
        return (secret ^ shared_secret) == withdraw.secret_xor_shared_secret;
    }

    BackupWithdrawalMessage GetBackupWithdraw()
    {
        return GetComplaint().GetBackupWithdraw();
    }

    BackupWithdrawalComplaint GetComplaint()
    {
        return msgdata[complaint_hash]["backup_withdraw_complaint"];
    }

    Point VerificationKey()
    {
        return GetBackupWithdraw().VerificationKey();
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};


class DepositAddressHistory
{
public:
    Point deposit_address;
    DepositAddressRequest deposit_request;
    MinedCreditMessage encoding_msg;
    std::vector<uint160> encoding_msg_full_hashes;
    RelayState state;
    std::vector<DepositAddressPartMessage> parts;
    std::vector<DepositAddressPartDisclosure> disclosures;
    std::vector<DepositTransferMessage> transfers;
    std::vector<TransferAcknowledgement> acks;

    DepositAddressHistory() { }

    DepositAddressHistory(Point address):
        deposit_address(address)
    {
        uint160 request_hash = depositdata[address]["deposit_request"];
        deposit_request = msgdata[request_hash]["deposit_request"];
        uint160 encoding_credit_hash
            = depositdata[request_hash]["encoding_credit_hash"];
        state = GetRelayState(encoding_credit_hash);
        state.latest_credit_hash = PreviousCreditHash(encoding_credit_hash);
        encoding_msg = creditdata[encoding_credit_hash]["msg"];
        encoding_msg.hash_list.RecoverFullHashes();
        encoding_msg_full_hashes = encoding_msg.hash_list.full_hashes;
        std::vector<uint160> part_hashes = depositdata[request_hash]["parts"];
        foreach_(uint160 hash, part_hashes)
        {
            DepositAddressPartMessage part = msgdata[hash]["deposit_part"];
            parts.push_back(part);
            uint160 disclosure_hash = depositdata[hash]["disclosure"];
            DepositAddressPartDisclosure disclosure
                = msgdata[disclosure_hash]["deposit_disclosure"];
            disclosures.push_back(disclosure);
        }
        SetDisclosuresAndAcks();
    }

    void SetDisclosuresAndAcks()
    {
        uint160 transfer_hash = depositdata[deposit_address]["latest_transfer"];
        uint160 ack_hash;
        DepositTransferMessage transfer;
        TransferAcknowledgement ack;
        while (transfer_hash != 0)
        {
            transfer = msgdata[transfer_hash]["transfer"];
            ack_hash = depositdata[transfer_hash]["acknowledgement"];
            ack = msgdata[ack_hash]["transfer_ack"];
            transfers.push_back(transfer);
            acks.push_back(ack);
            transfer_hash = transfer.previous_transfer_hash;
        }
        std::reverse(transfers.begin(), transfers.end());
        std::reverse(acks.begin(), acks.end());
    }

    void StoreData()
    {
        uint160 request_hash = deposit_request.GetHash160();
        
        msgdata[request_hash]["deposit_request"] = deposit_request;
        foreach_(uint160 hash, encoding_msg_full_hashes)
            StoreHash(hash);
        uint160 encoding_credit_hash = encoding_msg.mined_credit.GetHash160();

        if (!relaydata[encoding_credit_hash].HasProperty("state"))
        {
            log_ << "storing relay state " << state << " for "
                 << encoding_credit_hash << "\n";
            relaydata[encoding_credit_hash]["state"] = state;
        }
        std::vector<uint160> part_hashes;
        foreach_(DepositAddressPartMessage part, parts)
        {
            uint160 part_hash = part.GetHash160();
            msgdata[part_hash]["deposit_part"] = part;
            part_hashes.push_back(part_hash);
        }
        depositdata[request_hash]["parts"] = part_hashes;
        foreach_(DepositAddressPartDisclosure disclosure, disclosures)
        {
            msgdata[disclosure.GetHash160()]["deposit_disclosure"] = disclosure;
        }
        for (uint32_t i = 0; i < transfers.size() && i < acks.size(); i++)
        {
            uint160 transfer_hash = transfers[i].GetHash160();
            msgdata[transfer_hash]["transfer"] = transfers[i];
            msgdata[acks[i].GetHash160()]["transfer_ack"] = acks[i];
        }
    }

    void HandleData()
    {
        log_ << "DepositAddressHistory::HandleData()\n";
        uint160 request_hash = deposit_request.GetHash160();
        depositdata[deposit_address]["deposit_request"] = request_hash;
        depositdata[request_hash]["address"] = deposit_address;
        depositdata[deposit_address]["depositor_key"]
            = deposit_request.depositor_key;
        log_ << "set deposit address for " << deposit_address << " to "
             << request_hash << "\n";
        uint160 encoding_credit_hash = encoding_msg.mined_credit.GetHash160();
        depositdata[request_hash]["encoding_credit_hash"] = encoding_credit_hash;
        log_ << "set encoding_credit_hash for " << request_hash << " to "
             << encoding_credit_hash << "\n";
        for (uint32_t i = 0; i < transfers.size() && i < acks.size(); i++)
        {
            uint160 transfer_hash = transfers[i].GetHash160();
            uint160 ack_hash = acks[i].GetHash160();
            if (acks[i].transfer_hash == transfer_hash)
            {
                depositdata[transfer_hash]["acknowledgement"] = ack_hash;
                depositdata[transfer_hash]["confirmed"] = true;
                depositdata[deposit_address]["latest_transfer"] = transfer_hash;
                log_ << transfer_hash << " confirmed as latest transfer for "
                     << deposit_address << "\n";
            }
            AddAndRemoveMyAddresses(transfer_hash);
        }
    }

    bool Validate()
    {
        return true;
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(deposit_address);
        READWRITE(deposit_request);
        READWRITE(encoding_msg);
        READWRITE(encoding_msg_full_hashes);
        READWRITE(state);
        READWRITE(parts);
        READWRITE(disclosures);
        READWRITE(transfers);
        READWRITE(acks);
    )
};

#endif