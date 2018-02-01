#!/usr/bin/env python

from flask import Flask, render_template, request, g
from flask.ext.babel import gettext, ngettext, Babel
from defaults import default_ports, other_defaults
from flex import get_config, get_config_file
from subprocess import Popen, PIPE
from time import sleep
import sys

app = Flask(__name__)

babel = Babel(app)

_ = gettext

languages_ = {'en': _("English"), 
              'es': _("Spanish")}

chosen_settings = {}

miscellaneous_settings = ['CBT-actaspaymentprocessor',
                          'CBT-actasnotary',
                          'CBT-confirmationkey',
                          'OBK-actaspaymentprocessor',
                          'OBK-actasnotary',
                          'OBK-confirmationkey']

def get_default_locale():
    if 'locale' in chosen_settings:
        return chosen_settings['locale']
    return request.accept_languages.best_match(languages_.keys())

@babel.localeselector
def get_locale():
    return get_default_locale()

@babel.timezoneselector
def get_timezone():
    user = getattr(g, 'user', None)
    if user is not None:
        return user.timezone

@app.route('/')
def select_language():
    default_language = request.accept_languages.best_match(languages_.keys())
    return render_template('select_language.html', languages=languages_.items(),
                                                   default=default_language)

@app.route('/configfile', methods=['POST'])
def configfile():
    language = request.form['language']
    chosen_settings['locale'] = language
    return render_template('configfile.html', default=get_config_file())


@app.route('/usernamepassword', methods=['POST', 'GET'])
def usernamepassword():
    configfile = request.form['configurationfile']
    chosen_settings['configurationfile'] = configfile
    return render_template('usernamepassword.html', 
                           defaultusername="123a",
                           defaultpassphrase="123b",
                           defaultwalletpassphrase="123c")

@app.route('/testcurrencies', methods=['POST', 'GET'])
def testcurrencies():
    chosen_settings['rpcuser'] = request.form['username']
    chosen_settings['rpcpassword'] = request.form['passphrase']
    chosen_settings['walletpassword'] = request.form['walletpassphrase']
    return render_template('testcurrencies.html')

def get_testcurrency_details(request):
    print ['installtestcurrencies'] + miscellaneous_settings
    print [(key, request.form.get(key))
                             for key in ['installtestcurrencies'] +
                                          miscellaneous_settings]
    new_settings = dict([(key, request.form.get(key, ""))
                             for key in ['installtestcurrencies'] +
                                          miscellaneous_settings])
    chosen_settings.update(new_settings)
    print chosen_settings

@app.route('/cryptocurrencies', methods=['POST', 'GET'])
def cryptocurrencies():
    get_testcurrency_details(request)
    return render_template('cryptocurrencies.html')

def get_chosen_currencies(request):
    currencies = [request.form[x] for x in request.form
                  if x.startswith("install_")]
    return currencies

def select_default_ports():
    chosen_settings['currencies'].append("TCR")
    if chosen_settings['installtestcurrencies'] == 'yes':
        chosen_settings['currencies'].extend(["CBF", "EGT", "CBT", "OBK"])
    selected_ports = {}
    for currency in default_ports:
        if currency in chosen_settings['currencies']:
            port_mapping = default_ports[currency]
            for service, port in port_mapping.items():
                selected_ports[currency + "-" + service] = port
    return selected_ports

@app.route('/ports', methods=['POST', 'GET'])
def ports():
    chosen_settings['currencies'] = get_chosen_currencies(request)
    guessed_ports = select_default_ports()
    warning_ = ""
    if chosen_settings['CBT-actasnotary'] == 'yes':
        warning_ = _("It may take several seconds "
                     "to generate a confirmation key.")
    return render_template('ports.html', 
                           assigned_ports=guessed_ports.items(),
                           warning=warning_)

def get_assigned_ports(request):
    assigned_ports = dict([(x, request.form[x]) for x in request.form])
    return assigned_ports

def make_config_directory(configfile):
    directory = os.path.dirname(configfile)
    if not os.path.exists(directory):
        os.makedirs(directory)

def write_configuration(chosen_settings, assigned_ports):
    configfile = chosen_settings['configurationfile']
    make_config_directory(configfile)
    f = open(configfile, "w")
    settings_to_write = ['rpcpassword', 'rpcuser', 'walletpassword']
    if 'CBT' in chosen_settings['currencies']:
        settings_to_write += other_defaults.keys()
        chosen_settings.update(other_defaults)
        for miscellaneous_setting in miscellaneous_settings:
            if miscellaneous_setting in chosen_settings:
                settings_to_write.append(miscellaneous_setting)
    settings = [(s, chosen_settings[s]) for s in settings_to_write]
    settings += [(service, port) for (service, port) in assigned_ports.items()]
    for key, value in settings:
        if key.startswith("TCR-"):
            key = key.replace("TCR-", "")
        f.write("%s=%s\n" % (key, value))
    f.close()

def get_confirmation_key(currency_code):
    process = Popen(["flexd", "-debug"])
    pid = process.pid
    tries = 0
    while tries < 5:
        process = Popen(["teleport-cli", "getconfirmationkey"],
                        stdout=PIPE)
        result = process.stdout.read().strip()
        if len(result) > 0:
            chosen_settings[currency_code + '-confirmationkey'] = result
            process = Popen(["teleport-cli", "stop"])
            return
        sleep(5)
        tries += 1

@app.route('/finish', methods=['POST', 'GET'])
def finish():
    assigned_ports = get_assigned_ports(request)
    write_configuration(chosen_settings, assigned_ports)
    key_messages = {}
    
    for currency in ['CBT', 'OBK']:
        if chosen_settings[currency + '-actasnotary'] == 'yes':
            get_confirmation_key(currency)
            sleep(3)
        write_configuration(chosen_settings, assigned_ports)

        if chosen_settings.get(currency + '-confirmationkey') is not None:
            key_messages[currency] = _(
                "Your " + currency + " notary confirmation key is: "
                ) + chosen_settings[currency + '-confirmationkey']

    return render_template('finish.html', messages=key_messages.items())

def main():
    app.run(host='0.0.0.0', port=8730, debug=True)

if __name__ == '__main__':
    main()
