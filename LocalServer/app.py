from urllib import request
from flask import Flask, render_template, request
import requests
import json
from pathlib import Path

backend_server = "http://192.168.178.125:80"

app = Flask(__name__)


@app.route('/')
def index():
    return render_template("index.html")


@app.route('/flowbyte')
def flowbyte():
    return render_template("flowbyte_test.html")


@ app.route('/led')
def LedGet():
    backend_response = requests.get(backend_server+'/led')
    response = app.response_class(
        response=backend_response.text,
        status=backend_response.status_code,
        mimetype='application/json'
    )
    return response


@ app.route('/led', methods=['POST'])
def LedPost():
    backend_response = requests.post(
        backend_server+'/led', json=request.json)
    response = app.response_class(
        response=backend_response.text,
        status=backend_response.status_code,
        mimetype='application/json'
    )
    return response


@ app.route('/relais')
def RelaisGet():
    backend_response = requests.get(backend_server+'/relais')
    response = app.response_class(
        response=backend_response.text,
        status=backend_response.status_code,
        mimetype='application/json'
    )
    return response


@ app.route('/relais', methods=['POST'])
def RelaisPost():
    backend_response = requests.post(
        backend_server+'/relais', json=request.json)
    response = app.response_class(
        response=backend_response.text,
        status=backend_response.status_code,
        mimetype='application/json'
    )
    return response
