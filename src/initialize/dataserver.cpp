#include "flexnode/flexnode.h"

using namespace std;

#include "log.h"
#define LOG_CATEGORY "downloader.cpp"



/****************
 *  DataServer
 */

    void DataServer::SendTip(CNode *peer)
    {
        MinedCreditMessage msg
            = creditdata[flexnode.previous_mined_credit_hash]["msg"];

        log_ << "DataServer(): sending tip" << msg;
        peer->PushMessage("initdata", "tip", msg);
    }

 /*
 *  DataServer
 ****************/
