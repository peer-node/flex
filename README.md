Teleport: A New Way to Move and Trade Cryptocurrencies
=======================================================

Copyleft (c) 2015-2016 Teleport Developers

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
and have the funds back in your wallet in seconds, or you could
transfer the deposit (i.e. give the right to withdraw the secret
parts or to transfer again) to another Teleport address in seconds. 

There are no fees other than the fees for sending the cryptocurrencies
themselves.

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

Quick Start
-----------
On Ubuntu 14.04, install the dependencies:

    sudo apt-get install git libssl-dev openssl libdb6.0++-dev \
         libboost1.55-all-dev make gcc automake pkg-config autoconf \
         python-ecdsa python-crypto python-lockfile python-setuptools \
         bsdmainutils python-virtualenv python-pip

Then create and activate a virtualenv:

    virtualenv venv
    . venv/bin/activate

Then install Teleport:

    tar zxf Teleport.tgz
    cd Teleport/python
    python setup.py install

It may take a long time to compile if you don't have many cores.

Then you'll need to configure the installation:

    teleportcontrol autoconfigure

Then start:

    teleportcontrol start

Adding a Currency
-----------------

Communication between Teleport and other currencies is conducted via RPC.
Each currency listens on a specific port. To add a currency, specify
its rpc port in the configuration file.

Test currencies, CBF (cyberfarthings) and EGT (electrogroats) are included.

`cbf` and `egt` have a similar interface to other currencies, e.g.:

    cbf getbalance

`daemonize` assigns a port to an existing command:

    daemonize cbf 8181 2> ~/.cbf_log &
    daemonize egt 8182 2> ~/.egt_log &

This will set cyberfarthings running on port 8181 and electrogroats
on 8182. If you're using a currency like bitcoin which is already listening
on an rpc port, then you don't need to use `daemonize`. If, however, you're
using a light wallet like electrum, you will.

You can then add the currencies to Teleport with the configuration lines:

    CBF-rpcport=8181
    CBF-decimalplaces=0
    EGT-rpcport=8182
    EGT-decimalplaces=0

Or by doing:

    teleportcontrol configure CBF-rpcport 8181
    teleportcontrol configure CBF-decimalplaces 0
    teleportcontrol configure EGT-rpcport 8182
    teleportcontrol configure EGT-decimalplaces 0    

For most currencies, the number of decimal places will be 8. CBF and EGT
are not divisible below 1 unit.

With these currencies, you will by default only be able to send coins to other
users on the same machine. If you want to use them between machines,
say host1 and host2, you'll need to do something like:

    user1@host1$ cbf getbalance ; egt getbalance   # this puts the wallet files on host1

    ...

    user2@host2$ rm -f /tmp/*_wallet.pkl

    user2@host2$ mkdir tmptmp

    user2@host2$ sshfs user1@host1:/tmp tmptmp

    user2@host2$ ln -s tmptmp/cyberfarthings_wallet.pkl /tmp/cyberfarthings_wallet.pkl

    user2@host2$ ln -s tmptmp/electrogroats_wallet.pkl /tmp/electrogroats_wallet.pkl


Electrum
--------

To use electrum with Teleport, the daemon needs to be running, so you need to exit
the gui and do:

    $ electrum daemon start

Now you can set up an rpc process for it using the included daemonize script:

    $ daemonize electrum 8181 &

Then tell Teleport about the currency code and the port:

    $ teleportcontrol configure BTC-rpcport 8181

You can have a look at ~/.teleport/teleport.conf to see what's there if you want.

Depositing and Withdrawing Some Bitcoins
----------------------------------------

Presuming you've configured electrum as explained above, start Teleport:

    $ teleportcontrol start clean

You have to wait for a few seconds after starting it before it will
be responsive to commands:

    $ teleport-cli getinfo

Check that electrum is working:

    $ electrum getbalance
    0

    $ teleport-cli currencygetbalance BTC
    0

If Teleport says your BTC balance is -1, then it means electrum isn't
responding on the port specified as BTC-rpcport in the configuration
file. 

Once electrum is working with Teleport, you can start mining:

    $ teleport-cli setgenerate true

You'll need to wait about ten minutes or so until there are enough
relays to create a deposit address. When you see that the number
of relays reported by `teleport-cli getinfo` is about 9 or so, you can
request a deposit address:

    $ teleport-cli requestdepositaddress BTC

You can see that you don't get one straight away by doing:

    $ teleport-cli listdepositaddresses BTC
    [
    ]

After two more batches, you'll get a deposit address:

    $ teleport-cli listdepositaddresses BTC
    [
        {
            "address" : "1MUjir2tv3onkE1Bq46sTEaWdEqEeEXb4J",
            "balance" : "0.00000000"
        }
    ]

Then you can send a small number of bitcoins that you don't mind
losing in the event that something goes wrong to the deposit
address.

Once it's there, you can do:
    
    $ teleport-cli listdepositaddresses BTC
    [
        {
            "address" : "1MUjir2tv3onkE1Bq46sTEaWdEqEeEXb4J",
            "balance" : "0.02000000"
        }
    ]

Then you can withdraw the bitcoins to your electrum wallet:
    
    $ teleport-cli withdrawdeposit 1MUjir2tv3onkE1Bq46sTEaWdEqEeEXb4J

Then you can list the addresses in your electrum wallet:

    $ electrum listaddresses
    [
        "1MUjir2tv3onkE1Bq46sTEaWdEqEeEXb4J"
    ]

And you can check the balance of the address in your wallet:

    $ electrum getaddressbalance 1MUjir2tv3onkE1Bq46sTEaWdEqEeEXb4J
    {
        "confirmed": "0.02", 
        "unconfirmed": "0"
    }

You can also open the gui and see your imported bitcoins:

    $ electrum daemon stop
    $ electrum

You can spend them straight away if you like.

Transfers
---------

If, instead of withdrawing, you want to transfer control over the 
bitcoin deposit address to another Teleport user, you can do:

    $ teleport-cli transferdeposit 1MUjir2tv3onkE1Bq46sTEaWdEqEeEXb4J 19z6LKYqNXRYMgAnb1gSpJHAHcopfrZLTo

where `19z6LKYqNXRYMgAnb1gSpJHAHcopfrZLTo` is a Teleport address owned
by the recipient (e.g. an address which was the result of doing
`teleport-cli getnewaddress`).

The recipient will also need to be connected and synchronized with
your chain:

    user2@host2$ teleport-cli addnode 123.456.789.10:8733 onetry
    user2@host2$ teleport-cli startdownloading

It may take a five or ten seconds for user2 to get his or her chain
synchronized with the first user's.

Once they're reporting the same output from `teleport-cli getinfo`,
user1 can transfer the deposit to user2:

    user2@host2$ teleport-cli getnewaddress
    19z6LKYqNXRYMgAnb1gSpJHAHcopfrZLTo

    user1@host1$ teleport-cli transferdeposit 1MUjir2tv3onkE1Bq46sTEaWdEqEeEXb4J 19z6LKYqNXRYMgAnb1gSpJHAHcopfrZLTo

Eight seconds later:

    user2@host2$ teleport-cli listdepositaddresses BTC
    [
        {
            "address" : "1MUjir2tv3onkE1Bq46sTEaWdEqEeEXb4J",
            "balance" : "0.02000000"
        }
    ]

    user1@host1$ teleport-cli listdepositaddresses BTC
    [
    ]

user2 can then withdraw and spend the coins.

Connecting to Another Node
--------------------------

    ./teleport-cli addnode 123.456.789.10:8733 onetry
    ./teleport-cli startdownloading

Mining for Credits
------------------

    ./teleport-cli setgenerate true

Trading
------------------


Presuming you've daemonized the test currency, cbf, on port 8181 for
user1 and 9191 for user2, the following sequence of actions will execute
a trade.

First, you'll need to make configuration files for user1 and user2:


<table width=100%>
<tr>
<th>user1                                                                               </th><th>  user2 </th>
</tr>
<tr>
<td>
<pre>
user1@host$ mkdir .teleport
user1@host$ cat > .teleport/teleport.conf
rpcuser=teleportrpc
rpcpassword=AAak8SBifo8ogLZUGBua3qvG
walletpassword=23794827394jof42398rjo
CBF-rpcport=8181
CBF-decimalplaces=0


user1@host$ chmod 600 .teleport/teleport.conf
</pre>
</td>
<td>
<pre>
user2@host$ mkdir .teleport
user2@host$ cat > .teleport/teleport.conf
rpcuser=teleportrpc
rpcpassword=2qfkTF5Csm6cWwRdg7ya42QuP
walletpassword=kjsdfhskfjhdsk
port=8011
rpcport=8010
CBF-rpcport=9191
CBF-decimalplaces=0
user2@host$ chmod 600 .teleport/teleport.conf
</pre>
</td>
</tr>
</table>

Then you can start the servers


<table width=100%>
<tr>
<th>user1                                                                               </th><th>  user2 </th>
</tr>
<tr>
<td>
<pre>
user1@host$ Teleportd -debug > .log &amp;             
</pre>
</td>
<td>
<pre>
user2@host$ Teleportd -debug > .log &amp;           
</pre>
</td>
</tr>
</table>


Check that cyberfarthings are working:

<table>
<tr>
<th>user1                                                                               </th><th>  user2 </th>
</tr>
<tr>
<td>
<pre>
user1@host$ teleport-cli listcurrencies   
[         
    "CBF"
]
</pre>
</td>
<td>
<pre>
user2@host$ teleport-cli listcurrencies   
[         
    "CBF"
]
</pre>
</td>
</tr>
</table>

Check the initial balances:

<table>
<tr>
<th>user1                                                                               </th><th>  user2 </th>
</tr>
<tr>
<td>
<pre>
user1@host$ cbf getbalance       
10000                              
user1@host$ teleport-cli getbalance       
0.00000000                           
</pre>
</td>
<td>
<pre>
user2@host$ cbf getbalance        
10000                              
user2@host$ teleport-cli getbalance   
0.00000000                            
</pre>
</td>
</tr>
</table>


user1 can start mining:

<table>
<tr>
<th>user1                                                                               </th><th>  user2 </th>
</tr>
<tr>
<td>
<pre>
user1@host$ teleport-cli setgenerate true  
</pre>
</td>
<td>
<pre>
                                    
</pre>
</td>
</tr>
</table>

Then wait until a block has been mined, so that user2 has something to synchronize with.

It shouldn't take more than a minute or so. You can check using the getinfo command:
<table>
<tr>
<th>user1                                                                               </th><th>  user2 </th>
</tr>
<tr>
<td>
<pre>
user1@host$ teleport-cli getinfo
{                           
    "latest mined credit hash" :
        "3ba939a273b9140d7bfe47610d9e37ab0e042959",
    "batch number" : 2, 
    "offset" : 0,       
    "relays" : 1, 
    "balance" : 5000000   
}
</pre>
</td>
<td>
<pre>
                      &nbsp;
</pre>
</td>
</tr>
</table>                                                                                

Once the batch number is greater than or equal to 1, user2 can start downloading:
<table>
<tr>
<th>user1                                                                               </th><th>  user2 </th>
</tr>
<tr>
<td>
<pre>
                    &nbsp;
</pre>
</td>
<td>
<pre>
user2@host$ teleport-cli addnode 127.0.0.1:8333 onetry
user2@host$ teleport-cli startdownloading
user2@host$ sleep 2
user2@host$ teleport-cli getinfo
{
    "latest mined credit hash" :
        "3ba939a273b9140d7bfe47610d9e37ab0e042959",
    "batch number" : 2,
    "offset" : 0,
    "relays" : 1,  
    "balance" : 0
}
</pre>
</td>
</tr>
</table>                                                                                

Now user2 can place an order:
<table>
<tr>
<th>user1                                                                               </th><th>  user2 </th>
</tr>
<tr>
<td>
<pre>
                    &nbsp;
</pre>
</td>
<td>
<pre>
user2@host$ teleport-cli placeorder sell CBF 5000 0.0000001
b5353593611e2bc8bb13061777a4690fc581f1
</pre>
</td>
</tr>
</table>

Then wait until there are 8 or so relays. 5 are needed, but they have
to be encoded at least one block deep.

<table>
<tr>
<th>user1                                                                               </th><th>  user2 </th>
</tr>
<tr>
<td>
<pre>
user1@host$ teleport-cli getinfo
{
    "latest mined credit hash" : 
        "d07d87e9c19c6231153d1832985c629663502e8e",
    "batch number" : 10,
    "offset" : 8,
    "relays" : 6,
    "balance" : 50000000
}
</pre>
</td>
<td>
<pre>
                    &nbsp;
</pre>
</td>
</tr>
</table>


Then user1 can check the order book:
<table>
<tr>
<th>user1                                                                               </th><th>  user2 </th>
</tr>
<tr>
<td>
<pre>
user1@host$ teleport-cli book CBF                                
{                                                            
    "bids" : [                                               
    ],                                                       
    "asks" : [                                               
        {                                                    
            "size" : 5000,
            "price" : 0.00000010,
            "id" : "b5353593611e2bc8bb13061777a4690fc581f1"
        }                                                    
    ]                                                        
}                                                            
</pre>
</td>
<td>
<pre>
              &nbsp;
</pre>
</td>
</tr>
</table>


Then user1 can accept the order:

<table>
<tr>
<th>user1                                                                               </th><th>user2</th>
</tr>
<tr>
<td>
<pre>
user1@host$ ./teleport-cli acceptorder b5353593611e2bc8bb13061777a4690fc581f1
</pre>
</td>
<td>
<pre>
&nbsp;
</pre>
</td>
</tr>
</table>

Then a lot of stuff will happen in the background. Because you used
the -debug argument to Teleportd then it will create a ~/.Teleport/debug.log
file which will have a verbose but possibly incoherent description of
what's happening.

If the procedure described here doesn't work, there'll be an explanation
in the logs, which you can send to me and I'll try to reproduce and fix
the problem.

Shortly after the order is accepted, user1's TCR balance and user2's CBF balance
will decrease:
   
<table>
<tr>
<th>user1                                                                               </th><th>  user2 </th>
</tr>
<tr>
<td>
<pre>
user1@host$ cbf getbalance       
10000                              
user1@host$ teleport-cli getbalance     
0.45000000                            
</pre>
</td>
<td>
<pre>
user2@host$ cbf getbalance        
5000                              
user2@host$ teleport-cli getbalance   
0.00000000                           
</pre>
</td>
</tr>
</table>

user1's balance has dropped by 0.05, instead of 0.00050000 because the
corresponding credit has been spent but the change from the transaction
hasn't yet been encoded in a batch. user1 will receive the change in
the next batch.

Since you're using the -debug option, you can look at what's going on
in ~/.Teleport/debug.log, or else you can just wait for two more batches.

Then check the balances again:
<table>
<tr>
<th>user1                                                                               </th><th>  user2 </th>
</tr>
<tr>
<td>
<pre>
user1@host$ cbf getbalance       
15000                              
user1@host$ teleport-cli getbalance     
0.54950000                            
</pre>
</td>
<td>
<pre>
user2@host$ cbf getbalance        
5000                              
user2@host$ teleport-cli getbalance   
0.00050000                           
</pre>
</td>
</tr>
</table>

Note that user1 got the cyberfarthings and user2 got the credits. That's
all there is to it.

Binary Compilation
------------------
On Linux, download the tarball and extract it:

    tar zxvf teleport-xxxxxxxx.tgz
    cd teleport
    ./autogen.sh
    ./configure
    cd src
    make
    ./teleportd

Manual Configuration
--------------------

The configuration file is ~/.teleport/teleport.conf.
