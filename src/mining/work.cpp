#include "mining/work.h"


#include "log.h"
#define LOG_CATEGORY "work.cpp"

using namespace std;


/******************
 * TwistWorkProof
 */

    SimpleWorkProof::SimpleWorkProof()
    : seed(0),
      nonce(0),
      target(0)
    { }

    SimpleWorkProof::SimpleWorkProof(uint256 seed_256, uint160 difficulty)
    {
        seed = seed_256;
        target = TargetFromDifficulty(difficulty);
    }

    uint160 SimpleWorkProof::DivideIntoMaxUint160(uint160 number)
    {
        uint160 two_power_159 = 1;
        two_power_159 <<= 159;
        uint160 max_uint160 = (two_power_159 - 1) + two_power_159;
        return max_uint160 / number;
    }

    uint160 SimpleWorkProof::TargetFromDifficulty(uint160 difficulty)
    {
        return DivideIntoMaxUint160(difficulty);
    }

    uint160 SimpleWorkProof::DifficultyFromTarget(uint160 target)
    {
        return DivideIntoMaxUint160(target);
    }

    uint160 SimpleWorkProof::WorkDone()
    {
        if (IsValidProofOfWork())
            return DifficultyFromTarget(target);
        else
            return 0;
    }

    uint256 SimpleWorkProof::GetHash() const
    {
        CDataStream ss(SER_NETWORK, CLIENT_VERSION);
        ss << *this;
        return Hash(ss.begin(), ss.end());
    }

    uint160 GetHash160Value(uint256 seed, uint160 nonce)
    {
        unsigned char hash_input[52];
        memcpy(&hash_input[0], seed.begin(), 32);
        memcpy(&hash_input[0] + 32, nonce.begin(), 20);
        return Hash160(&hash_input[0], &hash_input[0] + 52);
    }

    int SimpleWorkProof::DoWork(uint8_t *working)
    {
        bool succeeded = false;
        uint160 trial_nonce = 0;

        while (bool(*working) and not succeeded)
        {
            trial_nonce += 1;
            if (GetHash160Value(seed, trial_nonce) <= target)
            {
                nonce = trial_nonce;
                succeeded = true;
            }
        }

        return succeeded;
    }

    bool SimpleWorkProof::IsValidProofOfWork()
    {
        return GetHash160Value(seed, nonce) <= target;
    }

/*
 * SimpleWorkProof
 ******************/


