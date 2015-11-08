from os.path import join, expanduser
from pyjsonrpc import HttpClient
import random
import string
import os

def flexdir():
    if 'FLEXDIR' in os.environ.keys():
        return os.environ['FLEXDIR']
    return join(expanduser("~"), ".flex")

def get_config_file():
    return join(flexdir(), "flex.conf")

def write_config_file(text):
    with open(get_config_file(), "w") as file_:
        file_.write(text)

def get_config():
    lines = open(get_config_file()).readlines()
    def split(string):
        return string.split("=", 1) if "=" in string else ("", "")
    return {key: value.strip() for key, value in map(split, lines)}

def set_config(config):
    write_config_file("\n".join([key + "=" + str(value)
                                 for key, value in config.items()]) + "\n")
    
def set_config_value(key, value):
    config = get_config()
    config[key] = value
    set_config(config)

def random_string(length):
    chars = string.lowercase + string.digits
    return ''.join(random.sample(chars, length))

def get_client():
    config = get_config()
    rpcport = int(config.get("rpcport", 8732))
    user = config.get("rpcuser")
    password = config.get("rpcpassword")
    host = config.get("rpchost", "localhost")
    return HttpClient(url="http://" + host + ":" + str(rpcport),
                      username=user, password=password)

def get_rpc():
    config = get_config()
    rpc_port = config.get("rpcport", 8732)
    rpc_user = config.get("rpcuser")
    rpc_password = config.get("rpcpassword")
    return rpc_port, rpc_user, rpc_password
