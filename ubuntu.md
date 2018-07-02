-----------

[Mac OS](mac_os.md)

On Ubuntu 14.04, install the dependencies:

    sudo apt-get install git libssl-dev openssl libdb6.0++-dev \
         libboost1.55-all-dev make gcc automake pkg-config autoconf \
         python-ecdsa python-crypto python-lockfile python-setuptools \
         bsdmainutils python-virtualenv python-pip

Then create and activate a virtualenv:

    virtualenv venv
    . venv/bin/activate

Then clone the github repository and switch to the tdd branch:

    git clone https://github.com/peer-node/teleport.git
    cd teleport
    git checkout tdd

Then compile the binaries and install them into the virtualenv's bin directory:
    
    mkdir build && cd build && cmake .. && make -j `nproc`
    export BINDIR=`echo $PATH | sed "s/:.*//g"`
    cp minerd teleportd $BINDIR

It may take a long time to compile if you don't have many cores.

Then install the python files:

    cd ../python
    python setup.py install

Then you'll need to configure the installation:

    teleport_control autoconfigure

Then start the binaries and the python server:

    teleport_control start clean

You can subsequently stop the binaries and server by doing:

    teleport_control stop

Using the Python Interface
--------------------------

At this point, you can optionally reduce the amount of memory used during
mining (default is 4 Gb) by doing (within python):

    > from teleport import get_client
    > client = get_client()
    > client.set_megabytes_used(1)

If your machine (or virtual machine) has less than 5 gigabytes of ram,
you will need to do this or else you will run out of memory during
mining.

You can get a list of commands available through the python interface
by doing:

    > client.help()


The Test Interface
------------------

Using your web browser, navigate to http://localhost:5000 .

You should see a simple interface which shows a few buttons. There should
be a teleport address starting with T visible, and you should have 0.0
credits.

You can start mining by clicking on "Start Mining"; you will need to have
mined 10 or more credits before you can request a deposit address.

You can refresh the interface to see how many credits have been mined
while it is mining by clicking on "Get New Address" or by going to
http://localhost:5000 again.

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
