// Distributed under version 3 of the Gnu Affero GPL software license, 
// see the accompanying file COPYING for details.

#include "flexnode/flexnode.h"
#include "crypto/crypter.h"

#include "log.h"
#define LOG_CATEGORY "wallet.cpp"

#define PASSWORD_MEMORY_FACTOR 15     // 256 Mb
#define PASSWORD_NUMBER_OF_LINKS 2048
#define PASSWORD_NUMBER_OF_SEGMENTS 1
#define LOG_OF_PASSWORD_TARGET 109

using namespace std;

vch_t GetSalt()
{
    vch_t salt = keydata["salt"]["salt"];
    if (salt.size() == 0)
    {
        salt.resize(WALLET_CRYPTO_SALT_SIZE);
        RAND_bytes(&salt[0], WALLET_CRYPTO_SALT_SIZE);
        keydata["salt"]["salt"] = salt;
    }
    return salt;
}

void SetSalt(vch_t salt)
{
    keydata["salt"]["salt"] = salt;
}

uint256 TwistPasswordHash(vch_t input)
{
    vch_t salt = GetSalt();
    input.insert(input.end(), salt.begin(), salt.end());
    uint256 memory_seed = Hash(input.begin(), input.end());
    uint160 target(1), link_threshold(PASSWORD_NUMBER_OF_LINKS);
    target <<= LOG_OF_PASSWORD_TARGET;
    link_threshold <<= LOG_OF_PASSWORD_TARGET;
    TwistWorkProof proof(memory_seed,
                         PASSWORD_MEMORY_FACTOR,
                         target,
                         link_threshold,
                         PASSWORD_NUMBER_OF_SEGMENTS);
    uint8_t keep_working = 1;
    proof.DoWork(&keep_working);
    uint256 proof_hash = proof.GetHash();
    return Hash(UBEGIN(proof_hash), UEND(proof_hash));
}



string_t FlexAddressFromPubKey(Point pubkey)
{
    uint160 keyhash = KeyHash(pubkey);
    CKeyID address(keyhash);
    return CBitcoinAddress(address).ToString();
}

/************
 *  Wallet
 */

    
    Wallet::Wallet(uint256 seed):
        seed(seed),
        derived_seed(seed)
    {
        log_ << "Wallet(): seed is " << seed << "\n";
        std::vector<CreditInBatch> credits_ = walletdata[seed]["credits"];
        credits = credits_;
        log_ << "credits are: " << credits << "\n";
    }

    string_t Wallet::ToString() const
    {
        stringstream ss;
        ss << "\n============== Wallet =============" << "\n"
           << "== " << "\n";
        foreach_(CreditInBatch credit, credits)
            ss << "== " << credit.ToString() << "\n";
        ss << "== " << "\n"
           << "============ End Wallet ===========" << "\n";
        return ss.str();  
    }

    Point Wallet::NewKey()
    {
        derived_seed = Hash(BEGIN(derived_seed), END(derived_seed));
        CBigNum privkey = (CBigNum(Hash(BEGIN(derived_seed),
                                        END(derived_seed)))
                                    + CBigNum(derived_seed))
                            % Point(SECP256K1, 0).Modulus();
        Point pubkey(SECP256K1, privkey);

        keydata[pubkey]["privkey"] = privkey;
        uint160 keyhash = KeyHash(pubkey);
        keydata[keyhash]["privkey"] = privkey;
        keydata[keyhash]["pubkey"] = pubkey;

        log_ << "Wallet::NewKey: pubkey for " << keyhash << " is "
             << pubkey << "\n";

        walletdata[keyhash].Location("generated") = GetTimeMicros();

        return pubkey;
    }

    uint64_t Wallet::Tradeable()
    {
        uint64_t max_credit = 0;
        foreach_(const CreditInBatch& credit, credits)
        {
            if (credit.amount > max_credit)
                max_credit = credit.amount;
        }
        return max_credit;
    }

    CreditInBatch Wallet::GetCreditWithAmount(uint64_t amount)
    {
        log_ << "GetCreditWithAmount: " << amount << "\n";
        foreach_(CreditInBatch& credit, credits)
        {
            if (credit.amount >= amount)
            {
                AddDiurnBranch(credit);
                return credit;
            }
        }
        return CreditInBatch();
    }

    void Wallet::HandleTransaction(SignedTransaction tx)
    {
        log_ << "Wallet::HandleTransaction()\n";
        foreach_(const CreditInBatch& credit, tx.rawtx.inputs)
            if (VectorContainsEntry(spent, credit))
            {
                EraseEntryFromVector(credit, spent);
            }
        log_ << "wallet: balance is now " << Balance() << "\n";
        NotifyGuiOfBalance(FLX, Balance() * 1e-8);
    }

    void Wallet::UnhandleTransaction(SignedTransaction tx)
    {
        log_ << "Wallet::UnhandleTransaction()\n";
        foreach_(const CreditInBatch& credit, tx.rawtx.inputs)
            if (walletdata.HasProperty(KeyHashFromKeyData(credit.keydata), 
                                       "privkey") && 
                !VectorContainsEntry(credits, credit))
            {
                credits.push_back(credit);
            }
        log_ << "wallet: balance is now " << Balance() << "\n";
        NotifyGuiOfBalance(FLX, Balance() * 1e-8);
    }

    vector<uint160> Wallet::GetKeyHashes()
    {
        vector<uint160> key_hashes;
        CLocationIterator<> key_scanner;
        key_scanner = walletdata.LocationIterator("generated");
        key_scanner.SeekStart();
        uint64_t time_;
        uint160 keyhash;
        while (key_scanner.GetNextObjectAndLocation(keyhash, time_))
        {
            key_hashes.push_back(keyhash);
        }
        return key_hashes;
    }

    void Wallet::HandleReceivedCredits(vector<CreditInBatch> incoming_credits)
    {
        foreach_(CreditInBatch credit, incoming_credits)
        {
            if (!HaveKeyForCredit((Credit)credit))
                continue;
            if (flexnode.calendar.CreditInBatchHasValidConnection(credit))
            {
                log_ << "adding received credit with amount " << credit.amount
                     << " to wallet.\n";
                if (VectorContainsEntry(credits, credit))
                    EraseEntryFromVector(credit, credits);
                credits.push_back(credit);
            }
        }
        sort(credits.begin(), credits.end());
        walletdata[seed]["credits"] = credits;
        NotifyGuiOfBalance(FLX, Balance() * 1e-8);
    }

    bool Wallet::HaveKeyForCredit(Credit credit)
    {
        if (credit.keydata.size() > 20)
        {
            Point pubkey;
            pubkey.setvch(credit.keydata);
            return keydata[pubkey].HasProperty("privkey");
        }
        uint160 keyhash = uint160(credit.keydata);
        return keydata[keyhash].HasProperty("privkey");
        
    }

    void Wallet::HandleNewBatch(CreditBatch batch)
    {
        log_ << "handling batch with " << batch.credits.size() << " credits\n";
        for (uint32_t i = 0; i < batch.credits.size(); i++)
        {
            uint160 keyhash = KeyHashFromKeyData(batch.credits[i].keydata);

            if (HaveKeyForCredit((Credit)batch.credits[i]))
            {
                CreditInBatch credit = batch.GetWithPosition(batch.credits[i]);
                if (!VectorContainsEntry(credits, credit))
                {
                    credits.push_back(credit);
                    log_ << "added to wallet\n";
                }
            }
            else if (walletdata[keyhash].HasProperty("watched"))
            {
                log_ << "credit watched by me!\n";
                log_ << "watched credit has keyhash: " << keyhash << "\n";
                CreditInBatch credit = batch.GetWithPosition(batch.credits[i]);
                vector<CreditInBatch> watched_credits = walletdata[keyhash]["watched_credits"];
                watched_credits.push_back(credit);
                walletdata[keyhash]["watched_credits"] = watched_credits;
                log_ << "number of watched credits for "
                     << keyhash << " is " 
                     << watched_credits.size() << "\n";
            }
            else
            {
                log_ << "wallet::handlenewbatch(): key not found\n";
            }
        }
        sort(credits.begin(), credits.end());
        walletdata[seed]["credits"] = credits;
    }

    void Wallet::UnhandleBatch(CreditBatch batch)
    {
        for (uint32_t i = 0; i < batch.credits.size(); i++)
        {
            CreditInBatch credit = batch.GetWithPosition(batch.credits[i]);
            if (VectorContainsEntry(credits, credit))
                EraseEntryFromVector(credit, credits);
            uint160 keyhash = KeyHashFromKeyData(credit.keydata);
            vector<CreditInBatch> watched_credits = walletdata[keyhash]["watched_credits"];
            if (VectorContainsEntry(watched_credits, credit))
            {
                EraseEntryFromVector(credit, watched_credits);
            }
            walletdata[keyhash]["watched_credits"] = watched_credits;
            walletdata[seed]["credits"] = credits;
        }
        sort(credits.begin(), credits.end());
    }

    uint64_t Wallet::Balance()
    {
        uint64_t total = 0;
        foreach_(const CreditInBatch &credit, credits)
            total += credit.amount;

        return total;
    }

    bool Wallet::SameKey(vch_t key1, vch_t key2)
    {
        uint32_t size1 = key1.size();
        uint32_t size2 = key2.size();

        if (size1 == size2)
            return key1 == key2;

        if (size1 == 20 && size2 == 34)
        {
            Point pubkey2;
            pubkey2.setvch(key2);
            return uint160(key1) == KeyHash(pubkey2);
        }
        
        if (size1 == 34 && size2 == 20)
        {
            Point pubkey1;
            pubkey1.setvch(key1);
            return KeyHash(pubkey1) == uint160(key2);
        }

        return false;
    }

    uint64_t Wallet::Balance(vch_t keydata)
    {
        uint64_t total = 0;

        foreach_(const CreditInBatch &credit, credits)
            if (SameKey(credit.keydata, keydata))
                total += credit.amount;

        return total;
    }

    bool Wallet::AddDiurnBranch(CreditInBatch& credit)
    {
        log_ << "add diurn branch()\n";
        
        uint160 batch_root = credit.branch.back();
        log_ << "batch root is " << batch_root << "\n";
        uint160 credit_hash = creditdata[batch_root]["credit_hash"];
        log_ << "credit_hash is " << credit_hash << "\n";
        MinedCredit mined_credit = creditdata[credit_hash]["mined_credit"];
        
        credit.diurn_branch.resize(0);
        uint160 root_bridge = mined_credit.BranchBridge();
        log_ << "branch bridge is " << root_bridge << "\n";
        credit.diurn_branch.push_back(root_bridge);

        log_ << "combining branch bridge and batch root gives credit hash: \n";

        uint160 cred_hash = SymmetricCombine(batch_root, root_bridge);

        log_ << (cred_hash == credit_hash) << "\n";
        
        uint160 calend_credit_hash = GetNextCalendCreditHash(credit_hash);
        log_ << "next calend_credit_hash is " << calend_credit_hash << "\n";

        if (calend_credit_hash == 0 || calend_credit_hash == credit_hash)
        {
            log_ << "AddDiurnBranch not adding anything else\n";
            return false;
        }

        MinedCredit calend_credit
            = creditdata[calend_credit_hash]["mined_credit"];

        uint160 diurn_root
            = SymmetricCombine(calend_credit.previous_diurn_root,
                               calend_credit.diurnal_block_root);
        log_ << "diurn root is " << diurn_root << "\n"
             << "diurn is stored: "
             << calendardata[diurn_root].HasProperty("diurn") << "\n";
        Diurn diurn = calendardata[diurn_root]["diurn"];

        log_ << "retrieved diurn of size " << diurn.Size() << "\n";
        vector<uint160> branch = diurn.Branch(credit_hash);
        log_ << "got branch: " << branch << "\n";

        log_ << "evaluate branch with credit hash: "
             <<  EvaluateBranchWithHash(branch, credit_hash) << "\n";

        credit.diurn_branch.reserve(1 + branch.size());
        credit.diurn_branch.insert(credit.diurn_branch.end(), 
                                   branch.begin(), branch.end());
        return true;
    }

    bool Wallet::MakeTradeable(uint64_t amount)
    {
        if (Balance() < amount)
            return false;
        Point new_key = NewKey();
        UnsignedTransaction rawtx
            = GetUnsignedTxGivenPubKey(new_key, amount, false);
        SignedTransaction tx = SignTransaction(rawtx);
        return true;
    }

    UnsignedTransaction Wallet::GetUnsignedTxGivenKeyData(vch_t keydata,
                                                          uint64_t amount, 
                                                          bool trade=false)
    {
        uint64_t amount_in = 0;
        UnsignedTransaction rawtx;

        foreach_(CreditInBatch &credit, credits)
        {
            AddDiurnBranch(credit);

            amount_in += credit.amount;
            bool is_keyhash = (credit.keydata.size() == 20);
            log_ << "GetUnsignedTx: is_keyhash: " << is_keyhash << "\n";
            rawtx.AddInput(credit, is_keyhash);
            log_ << "added credit " << credit.position
                 << " of size " << credit.amount << "\n";
            if (amount_in >= amount)
                break;
        }

        if (amount_in < amount)
            return rawtx;

        int64_t change = amount_in - amount;
        
        rawtx.AddOutput(Credit(keydata, amount));

        if (change > 0)
        {
            Point change_pubkey = NewKey();
            rawtx.outputs.push_back(Credit(change_pubkey.getvch(), 
                                           change));
        }

        foreach_(const CreditInBatch &credit, rawtx.inputs)
        {
            spent.push_back(credit);
            EraseEntryFromVector(credit, credits);
        }
        return rawtx;
    }

    UnsignedTransaction Wallet::GetUnsignedTxGivenPubKey(Point pubkey, 
                                                         uint64_t amount,
                                                         bool trade=false)
    {
        return GetUnsignedTxGivenKeyData(pubkey.getvch(), amount, trade);
    }

    bool Wallet::ImportPrivateKey(CBigNum privkey)
    {
        log_ << "wallet::ImportPrivateKey(): " 
#ifndef _PRODUCTION_BUILD
             << privkey
#endif
             << "\n";
        Point pubkey(SECP256K1, privkey);
        uint160 keyhash = KeyHash(pubkey);
        keydata[pubkey]["privkey"] = privkey;
        keydata[keyhash]["privkey"] = privkey;
        keydata[keyhash]["pubkey"] = pubkey;
        log_ << "importing watched credits:\n";
        ImportWatchedCredits(keyhash);
        return true;
    }

    void Wallet::ImportWatchedCredits(uint160 keyhash)
    {
        vector<CreditInBatch> watched_credits = walletdata[keyhash]["watched_credits"];
        log_ << "number of watched credits for " << keyhash << " is " 
             << watched_credits.size() << "\n";

        foreach_(const CreditInBatch& credit, watched_credits)
        {
            log_ << "importing credit with amount " << credit.amount << "\n";
            credits.push_back(credit);
            log_ << "balance is now " << Balance() << "\n";
        }
        watched_credits.resize(0);
        walletdata[keyhash]["watched_credits"] = watched_credits;

        vector<CreditInBatch> known_credits;
        known_credits = GetCreditsPayingToRecipient(keyhash);
        foreach_(const CreditInBatch& credit, watched_credits)
        {
            if (VectorContainsEntry(credits, credit))
                continue;
            log_ << "importing credit with amount " << credit.amount << "\n";
            credits.push_back(credit);
            log_ << "balance is now " << Balance() << "\n";
        }

        sort(credits.begin(), credits.end());
        NotifyGuiOfBalance(FLX, Balance() * 1e-8);
    }


/*
 *  Wallet
 ************/
