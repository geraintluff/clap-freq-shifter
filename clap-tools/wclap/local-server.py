#!/usr/bin/env python3
from http.server import HTTPServer, SimpleHTTPRequestHandler, test
import sys

class SimpleCORSRequestHandler(SimpleHTTPRequestHandler):
	def end_headers(self):
		self.send_header('Access-Control-Allow-Origin', '*')
		SimpleHTTPRequestHandler.end_headers(self)

if __name__ == '__main__':
	port = 8000
	if len(sys.argv) > 1:
		port = sys.argv[1]
	test(SimpleCORSRequestHandler, HTTPServer, port=port)
