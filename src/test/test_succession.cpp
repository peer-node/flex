#include "relays/relays.h"
#include "database/memdb.h"
#include "relays/relaystate.h"
#include "database/data.h"
#include "teleportnode/wallet.h"
#include "credits/creditutil.h"
#include "teleportnode/teleportnode.h"

int main()
{
    printf("Initializing teleportnode:...\n");
    teleportnode.Initialize();
    teleportnode.MineCredits(5);
    printf("Done\n");

    return 0;
}
