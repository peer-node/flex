#!/usr/bin/env python

from flask import Flask, render_template, request, g
from flex import get_client, get_config
from os.path import expanduser, join
from pyjsonrpc import HttpClient
from urllib2 import HTTPError
import logging
import sys

global client

app = Flask(__name__)

log = logging.getLogger('werkzeug')
log.setLevel(100)

sys.stdout.close()
sys.stderr.close()

def link(type, hash):
    if type in ['batch', 'mined_credit', 'mined_credit_msg']:
        return "/" + hash
    else:
        return "/" + type + "/" + hash

app.jinja_env.globals.update(link=link)

@app.route("/join/<join_hash>")
def show_join(join_hash):
    join = client.getjoin(join_hash)
    join['hash'] = join_hash
    return render_template('join.html', join=join)

@app.route("/succession/<succession_hash>")
def show_succession(succession_hash):
    succession = client.getsuccession(succession_hash)
    succession['hash'] = succession_hash
    return render_template('succession.html', succession=succession)

@app.route("/credits/<key_hash>")
def show_credits(key_hash):
    credits = client.listreceivedcredits(key_hash)
    return render_template('credits.html', credits=credits,
                                           key_hash=key_hash)

@app.route("/tx/<transaction_hash>")
def show_transaction(transaction_hash):
    if transaction_hash.replace("0", "") == "":
        return show_calendar()
    transaction = client.gettransaction(transaction_hash)
    return render_template('transaction.html', transaction=transaction)

@app.route("/<credit_hash>")
def show_batch(credit_hash):
    if "/" in credit_hash:
        return
    if credit_hash.replace("0", "") == "":
        return show_calendar()
    try:
        mined_credit = client.getminedcredit(credit_hash)
        mined_credit_msg = client.getminedcreditmessage(credit_hash)
        batch = client.getbatch(credit_hash)
    except HTTPError:
        return show_calendar()
    return render_template('batch.html', mined_credit=mined_credit,
                                         mined_credit_message=mined_credit_msg,
                                         batch=batch)
@app.route("/favicon.ico")
def show_favicon():
    return ""

@app.route("/calendar")
def show_calendar():
    calendar = client.getcalendar()
    return render_template('calendar.html', calendar=calendar)

@app.route("/")
def show_default():
    return show_calendar()

def writepid():
    from flex import flexdir
    import os
    f = open(join(flexdir(), "teleport.pids"), "a+")
    f.write(str(os.getpid()) + "\n")
    f.close()

def main():
    global client
    writepid()
    port = int(get_config().get("explorerport", 8729))
    client = get_client()
    app.run(host='0.0.0.0', port=port, debug=True)

if __name__ == '__main__':
    main()
