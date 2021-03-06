Teleport: A New Way to Move and Trade Cryptocurrencies
=======================================================

Copyleft (c) 2015-2018 Teleport Developers

An Exchange for Cryptocurrencies and Fiat
-----------------------------------------
Teleport allows you to trade cryptocurrencies without the need
for a centralized exchange. It does this by breaking secrets
into many parts and distributing these parts to a large
number of miners, who are unlikely to all be colluding.

Transmit Cryptocurrencies in Seconds
------------------------------------
You can request a deposit address for a specific cryptocurrency.
Nobody knows the private key to the deposit address, but many
miners each know a part. If you deposit funds to that address,
no one entity will be able to reconstruct the key needed to
spend them.

You can then request that they send you the parts of the private key,
and then import that private key into your cryptocurrency wallet, or you
could transfer the deposit address (i.e. give the right to withdraw the
secret parts or to transfer again) to another Teleport address in seconds.

There are no fees for transferring deposit addresses or withdrawing the
secret parts of the private keys.

Free Software
-------------
The Teleport software is available under the terms of the AGPLv3.

Unstable Version for Testing Purposes Only
------------------------------------------
This is experimental software and has not yet been fully audited for
security vulnerabilities or other problems. No cryptocurrencies of any
value should be traded using this software until a security audit has
been completed. There is a significant probability that you will lose
any coins that you try to trade while the software is still unstable, so
use testnet coins or coins with negligible value.

Platform-specific installation instructions 
-----------

[Mac OS](mac_os.md)

[Ubuntu](ubuntu.md)

Install and run User Interface 
------

The User Interface is composed of 
a vue.js frontend development server that 
controls the teleport and mining C++ binaries via 
REST API calls to 
a python backend server. 

To install and run the backend, do: 
```
cd ../UI/backend
```
and follow the 
[backend instructions](UI/backend/README.md).

To install and run the frontend, do: 

```
cd ../frontend
```
and follow the
[frontend instructions](UI/frontend/README.md).
 

Using the User Interface
------------------

You can start mining by clicking on the toggle button in the 
"Mining" panel. 
You will need to have
mined 10 or more credits before you can request a deposit address.

You can refresh the interface to see how many credits have been mined
while it is mining by clicking on the "refresh" button in the 
"Mining" panel. 

#### To Do:
When you have 10 or more credits, you can request a deposit address
for a specific currency. It will take a few blocks before the deposit
address you requested becomes available, so it won't be visible on
the interface until the number of credits has increased by 2 or 3.

When a deposit address is visible, there will be "Send" and "Withdraw"
buttons beside it. 

Choosing "Withdraw" will make the private key of this address visible. For
currencies such as Bitcoin, Ethereum and so on, this private key can
be imported into your cryptocurrency wallet using the "Import Private
Key" feature. Any funds which have been sent to the address will then
be available for spending in your wallet.

By choosing "Send", you can transfer ownership of the deposit address
to another Teleport user identified by a teleport address.

An instructive exercise is to send a Bitcoin (BTC) deposit address
to a teleport (TCR) deposit address. That is, request a BTC deposit
address, and also request a TCR deposit address. When you have both,
Send the BTC deposit address to the TCR deposit address.

The BTC deposit address should disappear. However, when you Withdraw
the TCR deposit address, the BTC deposit address will reappear. This
is because, when you withdraw the TCR deposit address, you acquire
the private key of the address to which the BTC was sent, and you
therefore become the owner of the BTC deposit address (again).


Trading
-------

Trading has not yet been implemented.
