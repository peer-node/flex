#include "crypto/point.h"

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