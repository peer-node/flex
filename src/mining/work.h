// Copyright (c) 2014 Teleport Developers
// Distributed under version 3 of the Gnu Affero GPL software license, see the accompanying
// file COPYING for details
#ifndef TELEPORT_WORK_H
#define TELEPORT_WORK_H

#include "crypto/teleportcrypto.h"
#include "crypto/uint256.h"

#include "log.h"
#define LOG_CATEGORY "work.h"


class TwistWorkCheck;

class SimpleWorkProof
{
public:
    SimpleWorkProof(uint256 seed, uint160 difficulty);

    uint256 seed;
    uint160 nonce;
    uint160 target;

    SimpleWorkProof();

    uint160 WorkDone();

    IMPLEMENT_SERIALIZE(
        READWRITE(seed);
        READWRITE(nonce);
        READWRITE(target);
    )

    JSON(seed, nonce, target);

    uint256 GetHash() const;

    int DoWork(unsigned char *working);

    bool IsValidProofOfWork();

    static uint160 TargetFromDifficulty(uint160 difficulty);

    static uint160 DivideIntoMaxUint160(uint160 number);

    static uint160 DifficultyFromTarget(uint160 target);
};


#endif
