#include "mining/work.h"
#include <openssl/rand.h>
#include "database/memdb.h"


#include "log.h"
#define LOG_CATEGORY "work.cpp"

using namespace std;


uint32_t RandomNumberLessThan(uint32_t n)
{
    uint32_t result;
    RAND_bytes((uint8_t*)&result, 4);
    return result % n;
}

uint128_t uint160to128(uint160 n)
{
    uint128_t n_;
    memcpy(&n_, ((char*)&n), 16);
    return n_;
}

uint160 uint128to160(uint128_t n)
{
    uint160 n_ = 0;
    memcpy(((char*)&n_), &n, 16);
    return n_;
}

/******************
 * TwistWorkProof
 */

    TwistWorkProof::TwistWorkProof()
    : memoryseed(0),
      N_factor(0),
      target(0),
      link_threshold(0),
      quick_verifier(0),
      num_links(0),
      num_segments(0),
      difficulty_achieved(0)
    { }

    TwistWorkProof::TwistWorkProof(uint256 memoryseed,
                   uint64_t N_factor, 
                   uint160 target, 
                   uint160 link_threshold,
                   uint32_t num_segments)
    : memoryseed(memoryseed),
      N_factor(N_factor),
      target(target),
      link_threshold(link_threshold),
      num_segments(num_segments),
      difficulty_achieved(0)
    { }

    uint160 TwistWorkProof::DifficultyAchieved()
    {
        if (num_links < num_segments / 4)
            return 0;
        uint128_t quick_verifier_ = uint160to128(quick_verifier);

        uint128_t hash =  twist_doquickcheck(
                                    UBEGIN(memoryseed),
                                    uint160to128(target),
                                    uint160to128(link_threshold),
                                    &quick_verifier_,
                                    N_factor, RFACTOR, num_segments, seeds,
                                    links, link_lengths);

        quick_verifier = uint128to160(quick_verifier_);

        uint160 hash160 = uint128to160(hash);

        if (hash160 > target || hash160 == 0)
            return 0;
        
        uint160 numerator = 1;
        numerator = numerator << 128;

        return difficulty_achieved = numerator / hash160;
    }

    uint64_t TwistWorkProof::Length()
    {
        uint64_t sum_of_link_lengths = 0;
        for (unsigned int i = 0; i < link_lengths.size(); i++)
            sum_of_link_lengths += link_lengths[i];
        return sum_of_link_lengths;
    }

    uint256 TwistWorkProof::GetHash()
    {
        CDataStream ss(SER_NETWORK, CLIENT_VERSION);
        ss << *this;
        return Hash(ss.begin(), ss.end());
    }

    int TwistWorkProof::DoWork(uint8_t *working)
    {
        uint64_t first_link;
        memcpy((uint8_t*)&first_link, (uint8_t*)&memoryseed, 8);

        uint128_t quick_verifier_ = uint160to128(quick_verifier);
        int result = twist_dowork(UBEGIN(memoryseed), 
                                  (uint8_t*)&first_link, 
                                  uint160to128(target), 
                                  uint160to128(link_threshold),
                                  &quick_verifier_,
                                  N_factor, RFACTOR, num_segments,
                                  seeds, links, link_lengths,
                                  working);
        quick_verifier = uint128to160(quick_verifier_);
        num_links = links.size();
        return result;
    }

    uint160 TwistWorkProof::WorkDone()
    {
        if (difficulty_achieved == 0)
            difficulty_achieved = DifficultyAchieved();
        
        uint160 numerator = 1;
        numerator = numerator << 128;

        uint160 target_difficulty = numerator / target;

        return difficulty_achieved > target_difficulty 
                ? target_difficulty : 0;
    }

    TwistWorkCheck TwistWorkProof::CheckRange(
                              uint32_t seed, uint32_t num_segments_to_check,
                              uint32_t link, uint32_t links_to_check)
    {
        TwistWorkCheck check(*this);
        check.start_link = link;
        check.end_link = link + links_to_check;
        check.valid = twist_doverify(UBEGIN(memoryseed), 
                                     uint160to128(target), 
                                     uint160to128(link_threshold),
                                     N_factor, 
                                     RFACTOR, 
                                     num_segments,
                                     seeds, 
                                     links, 
                                     link_lengths,
                                     seed, 
                                     num_segments_to_check, 
                                     link, 
                                     link + links_to_check,
                                     &check.failure_step, 
                                     &check.failure_link, 
                                     &check.failure_seed);
        return check;
    }


    TwistWorkCheck TwistWorkProof::SpotCheck()
    {
        uint32_t num_segments_to_check = num_segments > 4 ? 4 : 1;
        uint32_t num_links_to_check = num_links > 4 ? 4 : 1;

        uint32_t start_segment = RandomNumberLessThan(num_segments + 1 -
                                                      num_segments_to_check);
        uint32_t start_link = RandomNumberLessThan(num_links + 1 - 
                                                   num_links_to_check);

        return CheckRange(start_segment, num_segments_to_check,
                          start_link, num_links_to_check);
    }

/*
 * TwistWorkProof
 ******************/



/******************
 * TwistWorkCheck
 */

    TwistWorkCheck::TwistWorkCheck() {}

    TwistWorkCheck::TwistWorkCheck(TwistWorkProof &proof)
    {
        proof_hash = proof.GetHash();
    }


    uint256 TwistWorkCheck::GetHash()
    {
        CDataStream ss(SER_NETWORK, CLIENT_VERSION);
        ss << *this;
        return Hash(ss.begin(), ss.end());
    }

    int TwistWorkCheck::VerifyInvalid()
    {
        TwistWorkProof proof = workdata[proof_hash]["proof"];
        if (proof.Length() == 0)
            return false;
        TwistWorkCheck check_ = proof.CheckRange(failure_seed, 1,
                                                 failure_link, 1);
        return (!check_.valid);
    }

/*
 * TwistWorkCheck
 ******************/