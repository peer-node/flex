Installation instructions for Ubuntu
-----------

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

    cd ../webapp
    python setup.py install

Then you'll need to configure the installation:

    teleport_control autoconfigure

Then start the binaries and the python server:

    teleport_control start clean

You can subsequently stop the binaries and server by doing:

    teleport_control stop

