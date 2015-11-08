#include "crypto/signature.h"

CLevelDBWrapper flexdb("ccdb", 1 << 29);
CDataStore transactiondata(&flexdb, "t");
CDataStore scheduledata(&flexdb, "S");
CDataStore currencydata(&flexdb, "R");
CDataStore calendardata(&flexdb, "L");
CDataStore displaydata(&flexdb, "d");
CDataStore cipherdata(&flexdb, "c");
CDataStore marketdata(&flexdb, "x");
CDataStore walletdata(&flexdb, "w");
CDataStore relaydata(&flexdb, "r");
CDataStore creditdata(&flexdb, "C");
CDataStore tradedata(&flexdb, "T");
CDataStore initdata(&flexdb, "i");
CDataStore hashdata(&flexdb, "h");
CDataStore userdata(&flexdb, "u");
CDataStore workdata(&flexdb, "W");
CDataStore keydata(&flexdb, "k");
CDataStore msgdata(&flexdb, "m");
CDataStore netdata(&flexdb, "n");
CDataStore mydata(&flexdb, "y");

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
