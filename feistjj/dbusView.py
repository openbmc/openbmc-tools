#!/usr/bin/python3

# SPDX-License-Identifier: Apache-2.0
# Copyright 2020 Intel Corp.

import subprocess
from flask import Flask
from flask import send_file
import argparse

REPLACE_CHAR = '~'

app = Flask(__name__)

parser = argparse.ArgumentParser(description='Remote DBus Viewer')
parser.add_argument('-u', '--username', default='root')
parser.add_argument('-p', '--password', default='0penBmc')
parser.add_argument('-a', '--address', required=True)
parser.add_argument('-x', '--port', required=True)
args = parser.parse_args()

busctl = 'sshpass -p {} busctl -H {}@{} '.format(
    args.password, args.username, args.address)
header = '<head><link rel="icon" href="https://avatars1.githubusercontent.com/u/13670043?s=200&v=4" /></head>'


def getBusNames():
    out = subprocess.check_output(busctl + 'list --acquired', shell=True)
    out = out.split(b'\n')
    out = out[1:]
    names = []
    for line in out:
        name = line.split(b' ')[0]
        if name:
            names.append(name.decode())
    return names


def doTree(busname):
    out = subprocess.check_output(busctl + 'tree ' + busname, shell=True)
    out = out.split(b'\n')
    tree = []
    for line in out:
        path = line.split(b'/', 1)[-1].decode()
        path = '/' + path
        tree.append(path)
    return tree


def doIntrospect(busname, path):
    out = subprocess.check_output(
        busctl + 'introspect {} {}'.format(busname, path), shell=True)
    return out.decode().split('\n')


@app.route('/')
def root():
    out = header
    out += '<div><H2>Bus Names {}</H2></div>'.format(args.address)
    for name in getBusNames():
        out += '<div> '
        out += '<a href="{}"> {} </a>'.format(name, name)
        out += '</div>'
    return out


@app.route('/favicon.ico')
def favicon():
    return '<link rel="icon" type="image/png" href="https://avatars1.githubusercontent.com/u/13670043?s=200&v=4" />'


@app.route('/<name>')
def busname(name):
    out = header
    out += '<div><H2>tree {}</H2></div>'.format(name)
    for path in doTree(name):
        out += '<div> '
        out += '<a href="{}/{}"> {} </a>'.format(
            name, path.replace('/', REPLACE_CHAR), path)
        out += '</div>'
    return out


@app.route('/<name>/<path>')
def path(name, path):
    path = path.replace(REPLACE_CHAR, '/')
    out = header
    out += '<div><H2>introspect {} {}</H2></div>'.format(name, path)
    for line in doIntrospect(name, path):
        out += '<div> '
        out += '<pre> {} </pre>'.format(line)
        out += '</div>'
    return out


app.run(port=args.port, host='0.0.0.0')
