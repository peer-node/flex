#include "relays/relays.h"
#include "database/memdb.h"
#include "relays/relaystate.h"
#include "database/data.h"
#include "flexnode/wallet.h"
#include "credits/creditutil.h"
#include "flexnode/flexnode.h"

int main()
{
    printf("Initializing flexnode:...\n");
    flexnode.Initialize();
    flexnode.MineCredits(5);
    printf("Done\n");

    return 0;
}
