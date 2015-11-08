from subprocess import Popen as popen, PIPE as pipe
from os.path import expanduser, join
from flex import flexdir
import pyjsonrpc
import requests
import inspect
import sys


log = open(join(flexdir(), "obk.log"), "a+")
sys.stdout = log
sys.stderr = log


global payment_processor_url, notary_url

payment_processor_url = ''
notary_url = ''

class FakeFiatPlugin(pyjsonrpc.HttpRequestHandler):
    @pyjsonrpc.rpcmethod
    def _stop(self):
        sys.exit()

    @pyjsonrpc.rpcmethod
    def send(self, payer_data, payee_data, amount):
        form = {'sender': payer_data, 'recipient': payee_data,
                'amount': amount, 'ipn_url': notary_url}
        response = requests.post(payment_processor_url, data=form)
        return "ok"

    @pyjsonrpc.rpcmethod
    def balance(self, user):
        form = {'user': user}
        response = requests.post(payment_processor_url, data=form)
        return response.text

def main():
    try:
        port, payment_processor_url, notary_url = sys.argv[1:4]
        port = int(port)
    except:
        print ("""Usage: %s <port> <payment_processor_url> <notary_ipn_url>"""
               % sys.argv[0])
        sys.exit()

    http_server = pyjsonrpc.ThreadingHttpServer(
        server_address = ('0.0.0.0', port),
        RequestHandlerClass = FakeFiatPlugin
    )
    print "Starting FakeFiatPlugin rpc server"
    print "URL: http://localhost:" + str(port)
    http_server.serve_forever()

if __name__ == "__main__":
    main()
