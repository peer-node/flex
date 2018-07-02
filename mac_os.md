Installation instructions for Mac OS X 
-----

brew install git # also installs osxkeychain helper
git config --global credential.helper osxkeychain

brew install berkeley-db

brew install jsoncpp
mkdir /usr/local/include/jsoncpp
ln -s /usr/local/include/json /usr/local/include/jsoncpp # use full paths

brew install libjson-rpc-cpp

brew install openssl
brew install boost

install Anaconda: https://www.continuum.io/downloads
conda create -n teleport python=2
source activate teleport
conda install setuptools make automake pycrypto pkg-config autoconf ecdsa

pip install oslo.concurrency

git clone https://github.com/peer-node/teleport.git
cd teleport
git checkout tdd

mkdir build && cd build && cmake .. && make -j `sysctl -n hw.physicalcpu`
export BINDIR=`echo $PATH | sed "s/:.*//g"`
cp minerd teleportd $BINDIR

cd ../webapp 
python setup.py install
teleport_control autoconfigure

