#ifndef TELEPORT_MARKET
#define TELEPORT_MARKET


#include "trade/trade_messages.h"


typedef std::pair<uint8_t,vch_t> BookSide;
typedef std::pair<uint64_t,uint64_t> PriceAndTime;

class OrderStub
{
public:
    uint8_t side;
    uint64_t size;
    uint64_t integer_price;
    uint160 order_hash;
    bool funds_confirmed;

    OrderStub(Order order):
        side(order.side),
        size(order.size),
        integer_price(order.integer_price),
        order_hash(order.GetHash160())
    { }
};

std::vector<OrderStub> GetBids(vch_t currency);
std::vector<OrderStub> GetAsks(vch_t currency);

class Book
{
public:
    std::vector<OrderStub> bids;
    std::vector<OrderStub> asks;

    Book() {}

    Book(vch_t currency)
    {
        bids = GetBids(currency);
        asks = GetAsks(currency);
    }
};

void RecordTrade(uint160 accept_commit_hash);

#endif