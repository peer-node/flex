## Installing and spinning up the server

Install dependencies: 

```
python setup.py install
pip install -U flask-cors
```

Configure the installation:

```
teleport_control autoconfigure
```

Start the C++ binaries and the python server:
```
teleport_control start clean
```
You can subsequently stop the binaries and server by doing:
```
teleport_control stop
```

You can optionally reduce the amount of memory used during
mining (default is 4 Gb) by doing (within python):

    > from teleport import get_client
    > client = get_client()
    > client.set_megabytes_used(1)

If your machine (or virtual machine) has less than 5 gigabytes of ram,
you will need to do this or else you will run out of memory during
mining.

## Development 

The python server exposes a simple REST API
that can be used to control the C++ binaries.  

TODO: document the REST API

The REST API in turn makes use of 
a set of commands exposed by a 
python HTTP client. These commands 
make Remote Procedure Calls to the C++ binaries. 
To see 
the full list of commands, issue


    > client.help()

in the python REPL. 




