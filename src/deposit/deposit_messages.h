// Distributed under version 3 of the Gnu Affero GPL software license, 
// see the accompanying file COPYING for details.

#ifndef TELEPORT_DEPOSIT_MESSAGES
#define TELEPORT_DEPOSIT_MESSAGES

#include "crypto/teleportcrypto.h"
#include "credits/creditutil.h"
#include "deposit/deposit_secret.h"
#include "mining/work.h"

#include "log.h"
#define LOG_CATEGORY "transfer_messages.h"


class DepositAddressRequest
{
public:
    Point depositor_key;
    uint8_t curve;
    vch_t currency_code;
    TwistWorkProof proof_of_work;
    Signature signature;

    DepositAddressRequest() { }

    DepositAddressRequest(uint8_t curve, vch_t currency_code):
        curve(curve),
        currency_code(currency_code)
    {
        SetDepositorKey();
    }

    void SetDepositorKey()
    {
        CBigNum depositor_privkey = RandomPrivateKey(SECP256K1);
        depositor_key = Point(SECP256K1, depositor_privkey);
        keydata[depositor_key]["privkey"] = depositor_privkey;
    }

    static string_t Type() { return string_t("deposit_request"); }

    DEPENDENCIES();

    uint256 PreWorkHash()
    {
        TwistWorkProof original_proof = proof_of_work;
        Signature original_signature = signature;
        signature = Signature();
        proof_of_work = TwistWorkProof();
        uint256 hash = GetHash();
        proof_of_work = original_proof;
        signature = original_signature;
        return hash;
    }

    uint160 Target()
    {
        uint160 target(1);
        return target << DEPOSIT_LOG_TARGET;
    }

    uint160 LinkThreshold()
    {
        uint160 link_threshold(DEPOSIT_PROOF_LINKS);
        return link_threshold << DEPOSIT_LOG_TARGET;
    }

    void DoWork()
    {
        bool done = false;
        while (!done)
        {
            proof_of_work = TwistWorkProof(PreWorkHash(),
                                           DEPOSIT_MEMORY_FACTOR,
                                           Target(),
                                           LinkThreshold(),
                                           DEPOSIT_NUM_SEGMENTS);
            uint8_t keep_working = 1;
            proof_of_work.DoWork(&keep_working);
            if (proof_of_work.DifficultyAchieved() > 0)
                done = true;
            else
                SetDepositorKey();
        }
    }

    bool CheckWork()
    {
        return proof_of_work.memoryseed == PreWorkHash() &&
               proof_of_work.N_factor == DEPOSIT_MEMORY_FACTOR &&
               proof_of_work.target == Target() &&
               proof_of_work.link_threshold == LinkThreshold() &&
               proof_of_work.DifficultyAchieved() != 0 &&
               proof_of_work.SpotCheck().Valid();
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(depositor_key);
        READWRITE(curve);
        READWRITE(currency_code);
        READWRITE(proof_of_work);
        READWRITE(signature);
    )

    Point VerificationKey()
    {
        return depositor_key;
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};

std::vector<Point> GetRelaysForAddressRequest(uint160 request_hash,
                                              uint160 encoding_credit_hash);

std::vector<Point> GetRelaysForAddress(Point address);

class DepositAddressPartMessage
{
public:
    uint160 address_request_hash;
    uint160 relay_list_hash;
    uint32_t position;
    AddressPartSecret address_part_secret;
    Signature signature;

    DepositAddressPartMessage() { }

    DepositAddressPartMessage(uint160 address_request_hash,
                              uint160 encoding_credit_hash,
                              uint32_t position):
        address_request_hash(address_request_hash),
        position(position)
    {
        uint160 relay_chooser = (encoding_credit_hash * (1 + position)) ^
                                    address_request_hash;
        address_part_secret = AddressPartSecret(GetRequest().curve,
                                                encoding_credit_hash,
                                                relay_chooser);
    }

    static string_t Type() { return string_t("deposit_part"); }

    DEPENDENCIES(address_request_hash);

    Point PubKey()
    {
        return address_part_secret.PubKey();
    }

    std::vector<Point> Relays()
    {
        return address_part_secret.Relays();
    }

    DepositAddressRequest GetRequest()
    {
        return msgdata[address_request_hash]["deposit_request"];
    }

    CBigNum GetSecret(uint32_t position)
    {
        Point point_value = address_part_secret.point_values[position];
        return keydata[point_value]["privkey"];
    }

    Point GetPoint(uint32_t position)
    {
        return address_part_secret.point_values[position];
    }
    
    bool CheckSecretAtPosition(CBigNum secret, uint32_t position)
    {
        return Point(GetRequest().curve, secret)
                == address_part_secret.point_values[position];
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(address_request_hash);
        READWRITE(relay_list_hash);
        READWRITE(position);
        READWRITE(address_part_secret);
        READWRITE(signature);
    )

    Point VerificationKey()
    {
        std::vector<Point> relays;
        relays = GetRelaysForAddressRequest(address_request_hash,
                                            address_part_secret.credit_hash);
        if (position >= relays.size())
            return Point(SECP256K1, 0);
        return relays[position];
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};


class DepositAddressPartComplaint
{
public:
    uint160 part_msg_hash;
    uint32_t number_of_secret;
    Signature signature;

    DepositAddressPartComplaint() { }

    DepositAddressPartComplaint(uint160 part_msg_hash,
                               uint32_t number_of_secret):
        part_msg_hash(part_msg_hash),
        number_of_secret(number_of_secret)
    { }

    static string_t Type() { return string_t("deposit_part_complaint"); }

    DEPENDENCIES(part_msg_hash);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(part_msg_hash);
        READWRITE(number_of_secret);
        READWRITE(signature);
    )

    DepositAddressPartMessage GetPartMessage()
    {
        return msgdata[part_msg_hash]["deposit_part"];
    }

    Point VerificationKey()
    {
        return GetPartMessage().Relays()[number_of_secret];
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};


class DepositAddressPartRefutation
{
public:
    uint160 complaint_hash;
    CBigNum secret;
    Signature signature;

    DepositAddressPartRefutation() { }

    DepositAddressPartRefutation(uint160 complaint_hash):
        complaint_hash(complaint_hash)
    {
        secret = GetPartMessage().GetSecret(GetComplaint().number_of_secret);
    }

    static string_t Type() 
    {
        return string_t("deposit_part_refutation");
    }

    DEPENDENCIES(complaint_hash);

    bool Validate()
    {
        uint32_t number_of_secret = GetComplaint().number_of_secret;
        DepositAddressPartMessage part_msg = GetPartMessage();
        Point point = part_msg.GetPoint(number_of_secret);
        if (point != Point(point.curve, secret))
            return false;

        CBigNum shared_secret = Hash(secret * GetComplaint().VerificationKey());
        vector<CBigNum> secrets_xor_shared_secrets
            = part_msg.address_part_secret.secrets_xor_shared_secrets;
        if (secrets_xor_shared_secrets[number_of_secret] !=
            (shared_secret ^ secret))
            return false;
        return part_msg.CheckSecretAtPosition(secret, number_of_secret);
    }

    DepositAddressPartMessage GetPartMessage()
    {
        return GetComplaint().GetPartMessage();
    }

    DepositAddressPartComplaint GetComplaint()
    {
        return msgdata[complaint_hash]["deposit_part_complaint"];
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(complaint_hash);
        READWRITE(secret);
        READWRITE(signature);
    )

    Point VerificationKey()
    {
        return GetPartMessage().VerificationKey();
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};

#endif