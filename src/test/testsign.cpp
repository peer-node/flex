#include "crypto/signature.h"

CLevelDBWrapper teleportdb("ccdb", 1 << 29);
CDataStore transactiondata(&teleportdb, "t");
CDataStore scheduledata(&teleportdb, "S");
CDataStore currencydata(&teleportdb, "R");
CDataStore calendardata(&teleportdb, "L");
CDataStore displaydata(&teleportdb, "d");
CDataStore cipherdata(&teleportdb, "c");
CDataStore marketdata(&teleportdb, "x");
CDataStore walletdata(&teleportdb, "w");
CDataStore relaydata(&teleportdb, "r");
CDataStore creditdata(&teleportdb, "C");
CDataStore tradedata(&teleportdb, "T");
CDataStore initdata(&teleportdb, "i");
CDataStore hashdata(&teleportdb, "h");
CDataStore userdata(&teleportdb, "u");
CDataStore workdata(&teleportdb, "W");
CDataStore keydata(&teleportdb, "k");
CDataStore msgdata(&teleportdb, "m");
CDataStore netdata(&teleportdb, "n");
CDataStore mydata(&teleportdb, "y");

inline uint256 Rand256()
{
    uint256 r;
    RAND_bytes((uint8_t*)&r, 32);
    return r;
}

int main()
{
    CBigNum hash(Rand256());
    
    for (uint8_t curve = 1; curve < 4; curve++)
    {
        CBigNum privkey(Rand256());
        privkey = privkey / 8;
        Point pubkey(curve, privkey);
        Signature sig = SignHashWithKey(hash.getuint256(), privkey, curve);
        printf("Signature verified: %d\n", VerifySignatureOfHash(sig, hash.getuint256(), pubkey));
        printf("Bad Signature verified: %d\n",
            VerifySignatureOfHash(sig, hash.getuint256() + 1, pubkey));
        printf("Bad Signature verified: %d\n",
            VerifySignatureOfHash(sig, hash.getuint256(), pubkey + Point(curve, 10)));
    }

    return 0;
}
