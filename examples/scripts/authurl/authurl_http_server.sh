#!/bin/sh
#
# This is an example HTTP server responding to AUTH URL requests.
#
# turnutils_uclient (which is used by authurl_udp_client.sh) makes
# multiple requests simultaneouly.  We thus need a HTTP server that
# can support parallel HTTP requests.  This rules out nc (netcat).
# Python is used as the next most portable candidate.

if [ -d examples ] ; then
    cd examples
fi

python <<EOF
import BaseHTTPServer
import hashlib
import json
from urlparse import urlparse, parse_qsl

def ha1(user, realm, password):
    return hashlib.md5(user + ":" + realm + ":" + password).hexdigest()

class MyHandler(BaseHTTPServer.BaseHTTPRequestHandler):
    def do_GET(self):
        params = dict(parse_qsl(urlparse(self.path).query))
        result = self.ha1(params)
        self.send_json({"result": result})

    def ha1(self, params):
        user = params["user"]
        realm = params["realm"]
        h = ha1(user, realm, "youhavetoberealistic")
        return h

    def send_json(self, dict):
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.end_headers()
        body = json.dumps(dict)
        self.request.sendall(body)


BaseHTTPServer.HTTPServer(("127.0.0.1", 9876), MyHandler).serve_forever()
EOF
