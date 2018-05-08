from socket import gethostname
import getpass

default_ports = {'TCR': {'port': 8733,
                         'rpcport': 8732,
                         'explorerport': 8729},
                 'CBF': {'rpcport': 8181},
                 'EGT': {'rpcport': 8182},
                 'CBT': {'paymentprocessor': 8183,
                         'notary': 8184,
                         'notaryipn': 8185,
                         'notaryproof': 8186,
                         'rpcport': 8167},
                 'OBK': {'rpcport': 8168,
                         'web': 8169,
                         'notary': 8184,
                         'notaryipn': 8185,
                         'notaryproof': 8186}
                }

other_defaults = {'CBT-getbalance': 'balance ' + getpass.getuser(),
                  'CBT-sendfiat': 'send MY_DATA THEIR_DATA AMOUNT',
                  'CBT-decimalplaces': 2,
                  'CBT-fiat': 'true',
                  'OBK-payment_url': 'http://' + gethostname() + ':8169/',
                  'OBK-decimalplaces': 2,
                  'OBK-fiat': 'true',
                  'CBF-decimalplaces': 0,
                  'EGT-decimalplaces': 0}
