from flask import Flask, render_template, request, jsonify
import json

app = Flask(__name__)

notifications = []

@app.route('/', methods=['POST', 'GET'])
def top_page():
    data = request.get_data()
    print "received:", data
    if len(data) == 0:
        return jsonify(notifications=notifications)
    array = json.loads(data)
    notifications.append(array)
    return render_template('ok.html')

@app.route('/history')
def history():
    return jsonify(notifications=notifications)


if __name__ == '__main__':
    app.run(host='0.0.0.0', port=8731, debug=True)
