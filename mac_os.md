# Installation instructions for Mac OS X 

## C++ Dependencies

```
brew install berkeley-db

brew install jsoncpp
mkdir /usr/local/include/jsoncpp
ln -s /usr/local/include/json /usr/local/include/jsoncpp # use full paths

brew install libjson-rpc-cpp

brew install openssl
brew install boost

```

Python Dependencies
-----

install Anaconda: https://www.continuum.io/downloads

```
conda create -n teleport python=2
source activate teleport
conda install setuptools make automake pycrypto pkg-config autoconf ecdsa

pip install oslo.concurrency
```

Clone repo
-----

```
brew install git # also installs osxkeychain helper
```
If you want to contribute to the codebase, you'll want to cache your GitHub password: 
```
git config --global credential.helper osxkeychain
```
Then clone the repo: 
```
git clone https://github.com/peer-node/teleport.git
cd teleport
git checkout tdd
```

Compile the binaries
-----

```
mkdir build && cd build && cmake .. && make -j `sysctl -n hw.physicalcpu`
export BINDIR=`echo $PATH | sed "s/:.*//g"`
cp minerd teleportd $BINDIR
```

Set up the web interface 
-----

```
cd ../webapp 
python setup.py install
teleport_control autoconfigure
```

