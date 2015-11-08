#include "crypto/point.h"

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

int main()
{
    uint8_t curves[3] = {SECP256K1, CURVE25519, ED25519};

    for (uint32_t i = 0; i < 3; i++)
    {
        for (uint32_t k = 0; k < 10; k++)
        {
            printf("%u\n", k);
            uint8_t curve = curves[i];
            Point p(curve, 45 + k);

            mydata[curve]["point"] = p;

            Point q(curve, 40);

            Point r(curve, 85 + k);

            Point l;

            l = mydata[curve]["point"];

            printf("ok: %d\n", ((l + q) == r));
        }
    }
    return 0;
}