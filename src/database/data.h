// Copyright (c) 2015 Flex Developers
// Distributed under version 3 of the Gnu Affero GPL software license, see the accompanying
// file COPYING for details

#ifndef FLEX_DATA_H
#define FLEX_DATA_H

#ifndef FLEX_MOCKDATA_H

#include "database/datastore.h"

extern CDataStore calendardata;
extern CDataStore currencydata;
extern CDataStore scheduledata;
extern CDataStore commanddata;
extern CDataStore depositdata;
extern CDataStore marketdata;
extern CDataStore walletdata;
extern CDataStore creditdata;
extern CDataStore relaydata;
extern CDataStore tradedata;
extern CDataStore hashdata;
extern CDataStore initdata;
extern CDataStore taskdata;
extern CDataStore workdata;
extern CDataStore guidata;
extern CDataStore keydata;
extern CDataStore msgdata;

#else

MockDataStore calendardata;
MockDataStore currencydata;
MockDataStore scheduledata;
MockDataStore commanddata;
MockDataStore depositdata;
MockDataStore marketdata;
MockDataStore walletdata;
MockDataStore creditdata;
MockDataStore relaydata;
MockDataStore tradedata;
MockDataStore hashdata;
MockDataStore initdata;
MockDataStore taskdata;
MockDataStore workdata;
MockDataStore guidata;
MockDataStore keydata;
MockDataStore msgdata;

#endif /* FLEX_MOCKDATA_H */


#endif /* FLEX_DATA_H */
