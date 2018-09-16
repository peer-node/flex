Installation instructions for Ubuntu
-----------

On Ubuntu 14.04, install the dependencies:

    sudo apt-get install git libssl-dev openssl libdb6.0++-dev \
         libboost1.55-all-dev make gcc automake pkg-config autoconf \
         python-ecdsa python-crypto python-lockfile python-setuptools \
         bsdmainutils python-virtualenv python-pip

Create and activate a virtualenv:

    virtualenv venv
    . venv/bin/activate

Clone the github repository:

    git clone https://github.com/peer-node/teleport.git
    cd teleport

Compile the binaries and install them into the virtualenv's bin directory:
    
    mkdir build && cd build && cmake .. && make -j `nproc`
    export BINDIR=`echo $PATH | sed "s/:.*//g"`
    cp minerd teleportd $BINDIR

It may take a long time to compile if you don't have many cores.

