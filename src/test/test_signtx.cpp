#include "credits/creditsign.h"

using namespace std;

string strFLX("FLX");
vch_t FLX(strFLX.begin(), strFLX.end());

BitChain spent_chain;


int main()
{
    UnsignedTransaction rawtx;
    CBigNum privkey(5);
    Secp256k1Point pubkey(privkey);
    keydata[pubkey]["privkey"] = privkey;

    CreditInBatch input;
    input.keydata = pubkey.getvch();
    input.amount = 200;

    rawtx.AddInput(input);

    SignedTransaction tx = SignTransaction(rawtx);
    if (!VerifyTransactionSignature(tx))
        printf("signature verification failed!\n");
    else
        printf("signature verification succeeded!\n");
    return 0;
}