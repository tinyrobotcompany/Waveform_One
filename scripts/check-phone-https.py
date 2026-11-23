#!/usr/bin/env python3
"""Verify a Waveform One HTTPS identity against an address, even before public DNS is set up.

Read-only: requires authenticated TLS and a 401 from the protected status endpoint.
Does not send pairing tokens, API keys or settings. Never disables verification.
"""
import argparse
import http.client
import ipaddress
import socket
import ssl


def check(hostname, address, ca=None, port=443):
    ipaddress.ip_address(address)
    context=ssl.create_default_context(cafile=ca)
    context.minimum_version=ssl.TLSVersion.TLSv1_2
    with socket.create_connection((address,port),timeout=10) as raw:
        with context.wrap_socket(raw,server_hostname=hostname) as tls:
            tls.sendall(('GET /api/session HTTP/1.1\r\nHost: '+hostname+'\r\nConnection: close\r\n\r\n').encode('ascii'))
            response=http.client.HTTPResponse(tls)
            response.begin()
            if response.status!=401:
                raise ValueError('Expected unauthenticated status to be rejected with 401.')
            return tls.version()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--hostname',required=True)
    parser.add_argument('--address',required=True)
    parser.add_argument('--ca',help='Explicit development CA PEM; omit for public certificates')
    args=parser.parse_args()
    try:
        version=check(args.hostname,args.address,args.ca)
    except (OSError,ValueError,http.client.HTTPException):
        parser.exit(1,'HTTPS verification failed. Check identity, trust, expiry, address and authentication.\n')
    print('PASS: trusted hostname, encrypted '+version+' connection, unauthenticated API rejected.')


if __name__=='__main__':main()
