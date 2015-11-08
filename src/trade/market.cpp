#include "trade/trade.h"

#include "log.h"
#define LOG_CATEGORY "market.cpp"

using namespace std;

vector<OrderStub> GetBids(vch_t currency)
{
    vector<OrderStub> bids;
    uint160 order_hash;
    PriceAndTime price_and_time;

    CLocationIterator<BookSide> bid_scanner 
        = marketdata.LocationIterator(BidSide(currency));
    
    bid_scanner.SeekEnd();
    while (bid_scanner.GetPreviousObjectAndLocation(order_hash, 
                                                    price_and_time))
    {
        Order order = marketdata[order_hash]["order"];
        bids.push_back(OrderStub(order));
    }
    return bids;
}

vector<OrderStub> GetAsks(vch_t currency)
{
    vector<OrderStub> asks;
    uint160 order_hash;
    PriceAndTime price_and_time;

    CLocationIterator<BookSide> ask_scanner 
        = marketdata.LocationIterator(AskSide(currency));
    
    ask_scanner.SeekStart();
    while (ask_scanner.GetNextObjectAndLocation(order_hash, price_and_time))
    {
        Order order = marketdata[order_hash]["order"];
        asks.push_back(OrderStub(order));
    }
    return asks;
}

void RecordTrade(uint160 accept_commit_hash)
{
    AcceptCommit accept_commit;
    accept_commit = tradedata[accept_commit_hash]["accept_commit"];
    Order order = tradedata[accept_commit.order_hash]["order"];
    // if (marketdata[accept_commit_hash].HasProperty(order.currency))
    // {
    //     log_ << "Trade " << accept_commit_hash << " was already recorded\n";
    //     return;
    // }
    marketdata[accept_commit_hash].Location(order.currency) = GetTimeMicros();
}
