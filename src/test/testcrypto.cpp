#include "crypto/teleportcrypto.h"
#include <iostream>
#include <vector>
#include <stdio.h>
#include <stdint.h>

using namespace std;

CGroup curve_group;
uint32_t num_points=1, numnodes=0, numhashtrees=0, numregions=0,
         numbitfields=0, numelements=0;

#define NUM 25

void PrintPoint(Secp256k1Point point)
{
    CBigNum x, y;
    point.GetCoordinates(x, y);
    printf("%s,%s\n", x.ToString().c_str(), y.ToString().c_str());
}

void printvch(vch_t vch)
{
    for (uint32_t i = 0; i <vch.size(); i++)
        printf("%X", vch[i]);
    printf("\n");
}

int main()
{
    vector<Secp256k1Point> vecprelays;
    int i;

    for (i = 10; i < 10 + NUM; i++)
    {
        Secp256k1Point point(i);
        vecprelays.push_back(point);
    }

    DistributedSecret ds;

    ds = GenerateDistributedSecretGivenRelays(vecprelays);

    Secp256k1Point pubkey_ = ds.PubKey();
    bool result_ = confirm_public_key_with_N_polynomial_point_values(
                                    pubkey_,
                                    ds.vecpPolyPointValues, NUM);

    printf("confirm public key: %d\n", result_);

    vector<CBigNum> vbnSecrets;

    for (i = 0; i < NUM; i++)
    {
        CBigNum prv(i + 10);
        CBigNum bnPlaintext;
        int r = decrypt(bnPlaintext, prv, ds.vvchEncryptedSecrets[i]);
        
        vbnSecrets.push_back(bnPlaintext);
    }
    
    CBigNum privkey(0);

    bool cheated;
    std::vector<int> vnCheaters;
    int result = generate_private_key_from_N_secret_parts(privkey,
                                                          vbnSecrets,
                                                          ds.vecpPolyPointValues,
                                                          NUM,
                                                          cheated,
                                                          vnCheaters);
    Secp256k1Point pubkey(privkey);

    printf("%s vs %s\n", privkey.ToString().c_str(), 
                         ds.PrivKey().ToString().c_str());
    printf("Success: %d\n", (pubkey == ds.PubKey()));
    return 0;
}
