#!/usr/bin/env python

from flask import Flask, render_template, flash, request
from os.path import join, expanduser
from collections import defaultdict
from random import random
from flex import flexdir
from hashlib import md5
import logging
import sys
import os

global proofport
proofport = 0

app = Flask(__name__)

log = logging.getLogger('werkzeug')
log.setLevel(100)

sys.stdout.close()
sys.stderr.close()


transactions = set()
balances = defaultdict(lambda: 1000)

def writepid():
    f = open(join(flexdir(), "flex.pids"), "a+")
    f.write(str(os.getpid()) + "\n")
    f.close()

@app.route('/')
def top_page():
    return render_template('login.html')

@app.route('/login', methods=['POST', 'GET'])
def login():
    user = request.form['user']
    balance = balances[user]
    return render_template("account.html", user=user, balance=balance)

@app.route('/account/<user>')
def account_page(user):
    balance = balances[user]
    return render_template("account.html", user=user, balance=balance)

@app.route('/pay/<payer>')
def payment_page(payer):
    balance = balances[payer]
    return render_template("pay.html", payer=payer, balance=balance)

@app.route('/payment_details/<payer>', methods=['POST'])
def payment_details_page(payer):
    payee = request.form['payee']
    amount = float(request.form['amount'])
    transaction = (payer, payee, amount)
    host = request.url_root.split("://")[1].split(":")[0]
    if not transaction in transactions:
        transactions.add(transaction)
        balances[payer] -= amount
        balances[payee] += amount
        return render_template("payment_details.html",
                               payer=payer, payee=payee, amount=amount,
                               proofport=proofport, host=host,
                               code=md5(payer + payee + str(amount)).hexdigest())
    return render_template("duplicate_transaction.html",
                           payer=payer, payee=payee, amount=amount,
                           proofport=proofport, host=host,
                           code=md5(payer + payee + str(amount)).hexdigest())

def main():
    try:
        writepid()
        port, proofport = map(int, sys.argv[1:])
    except:
        print "Usage: %s <port> <proofport>" % sys.argv[0]
        sys.exit()
    app.run(host='0.0.0.0', port=port, debug=True)


if __name__ == '__main__':
    main()
