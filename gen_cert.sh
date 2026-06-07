#!/bin/sh
openssl req -x509 -nodes -days 365 -newkey rsa:2048 -keyout cert.key -out cert.crt
openssl pkcs12 -export -out cert.pfx -inkey cert.key -in cert.crt -passout pass:password
