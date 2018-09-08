# Installation instructions for Mac OS X 

## C++ Dependencies

```
brew install berkeley-db jsoncpp libjson-rpc-cpp openssl boost
mkdir /usr/local/include/jsoncpp
ln -s /usr/local/include/json /usr/local/include/jsoncpp # use full paths
```

Python Dependencies
-----

Install [Anaconda](https://www.continuum.io/downloads) and use it to install the following packages into a custom `conda` environment:

```
conda create -n teleport python=2
source activate teleport
conda install setuptools make automake pycrypto pkg-config autoconf ecdsa
pip install oslo.concurrency
```

Clone repo
-----

Make sure `git` is installed: 
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
export BINDIR=`echo $PATH | sed "s/:.*//g"` # before issuing this command, make sure custom conda environment is activated
cp minerd teleportd $BINDIR
```
It may take a long time to compile if you don't have many cores.

Install and run web app 
------

The web app is composed of 
a vue.js frontend development server that 
controls the teleport and mining C++ binaries via 
REST API calls to 
a python backend server. 

To install and run the backend, do: 
```
cd ../webapp/backend
```
and follow the 
[backend instructions](webapp/backend/README.md).

To install and run the frontend, do: 

```
cd ../frontend
```
and follow the
[frontend instructions](webapp/frontend/README.md).
 
