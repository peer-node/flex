from flask import Flask, render_template, url_for, request
from collections import defaultdict
from teleport import get_client


app = Flask(__name__)

client = get_client()

data = {'teleport_address': "",
        'deposit_addresses': defaultdict(list),
        'withdrawn_addresses': defaultdict(list),
        'currencies': [],
        'info': {},
        'first_view': True,
        'mining': False}


def update_data():
    data['info'] = client.getinfo()
    data['currencies'] = client.listcurrencies()
    for currency in data['currencies']:
        data['deposit_addresses'][currency] = client.listdepositaddresses(currency) or []
    for currency in data['currencies']:
        data['withdrawn_addresses'][currency] = client.listwithdrawals(currency) or []
    if data['first_view']:
        data['teleport_address'] = client.getnewaddress()
        data['first_view'] = False


def main_page():
    update_data()
    return render_template("main_page.html", data=data)


@app.route('/')
def show_main_page():
    return main_page()


@app.route('/new_teleport_address')
def new_teleport_address():
    data['teleport_address'] = client.getnewaddress()
    return main_page()


# new stuff
from flask_cors import CORS
CORS(app, resources={r"/api/*": {"origins": "http://localhost:8080"}})

# new stuff
from flask import jsonify

# new stuff
version = 'v1.0'
api = '/api/' + version + '/'

# new stuff
@app.route(api)
def update_and_return_data():
    update_data()
    return jsonify(data)


# new stuff
@app.route(api + 'start_mining')
def start_mining():
    client.keep_mining_asynchronously()
    data['mining'] = True
    return update_and_return_data()


# new stuff
@app.route(api + 'stop_mining')
def stop_mining():
    client.stop_mining()
    data['mining'] = False
    return update_and_return_data()


@app.route('/request_deposit_address', methods=['POST'])
def request_deposit_address():
    currency = request.form['currency']
    client.requestdepositaddress(currency)
    return main_page()


@app.route('/send_deposit_address', methods=['POST'])
def send_deposit_address():
    deposit_address = request.form['deposit_address']
    return render_template("main_send_deposit_address.html", data=data, selected_deposit_address=deposit_address)


@app.route('/send_deposit_address_complete', methods=['POST'])
def send_deposit_address_complete():
    deposit_address = request.form['deposit_address']
    destination_address = request.form['destination_address']
    client.transferdepositaddress(deposit_address, destination_address)
    return main_page()


@app.route('/withdraw_deposit_address', methods=['POST'])
def withdraw_deposit_address():
    deposit_address = request.form['deposit_address']
    client.withdrawdepositaddress(deposit_address)
    return main_page()


@app.route('/main_send_credits', methods=['POST'])
def main_send_credits():
    update_data()
    return render_template("main_send_credits.html", data=data)


@app.route('/main_send_credits_complete', methods=['POST'])
def main_send_credits_complete():
    destination_address = request.form['destination_address']
    amount = request.form['amount']
    client.sendtoaddress(destination_address, amount)
    update_data()
    return render_template("main_send_credits_complete.html", data=data)


if __name__ == '__main__':
    app.run(host='0.0.0.0', debug=True)
