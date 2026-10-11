"""Temporary desktop esptool transport through the native P4 development bridge.

The device implementation is C++. This tool needs physical UART access to get
the short-lived key and opens a loopback-only socket for the standard esptool.
It never prints the key. Serial remains open across all operations in a plan.
"""
import argparse
import json
import os
from pathlib import Path
import re
import select
import socket
import subprocess
import sys
import time
import serial


def discover_console(port):
    owner = subprocess.run(['lsof', '-t', '--', port], capture_output=True, text=True)
    if owner.stdout.strip():
        raise RuntimeError('Serial port is already in use; refusing to interrupt its owner')
    console = serial.Serial()
    console.port = port
    console.baudrate = 115200
    console.timeout = .2
    console.dtr = False
    console.rts = False
    console.open()
    output = bytearray()
    deadline = time.monotonic() + 35
    while time.monotonic() < deadline:
        output += console.read(4096)
        if len(output) > 128 * 1024:
            raise RuntimeError('Console discovery exceeded its bound')
        key = re.search(rb'DEVELOPMENT_FLASH_KEY=([0-9a-f]{64})\r?\n', output)
        host = re.search(rb'Phone remote ready on home Wi-Fi: http://([0-9.]+)', output)
        if key and host:
            print(f'P4 native bridge ready at {host[1].decode()}; physical console key acquired', flush=True)
            return console, host[1].decode(), key[1]
    console.close()
    raise RuntimeError('P4 development key/home-network address not observed')


def exact(connection, count):
    result = b''
    while len(result) < count:
        data = connection.recv(count - len(result))
        if not data:
            raise RuntimeError('P4 bridge closed before its READY response')
        result += data
    return result


def operation(host, key, arguments, console):
    # Only trusted local plan arguments reach the standard esptool subprocess.
    if not arguments or arguments[0] not in ('flash-id', 'read-flash', 'write-flash', 'diagnostics'):
        raise ValueError('Unsupported operation in commissioning plan')
    live = arguments[0] == 'diagnostics'
    if live and (len(arguments) != 3 or not 1 <= int(arguments[1]) <= 120):
        raise ValueError('Diagnostics requires a duration of 1–120 seconds and a local output path')
    deadline = time.monotonic() + 20
    remote = None
    while time.monotonic() < deadline:
        candidate = socket.create_connection((host, 8765), timeout=5)
        candidate.settimeout(8)
        candidate.sendall(key + b'\n' + (b'T' if live else b'R'))
        response = exact(candidate, 6)
        if response == b'READY\n':
            remote = candidate
            break
        candidate.close()
        if response != b'BUSY!\n':
            raise RuntimeError('P4 bridge rejected maintenance takeover')
        print('Waiting for the existing S3 capture to finish', flush=True)
        time.sleep(.5)
    if remote is None:
        raise RuntimeError('S3 capture remained busy beyond the maintenance deadline')
    with remote:
        remote.settimeout(8)
        if live:
            deadline = time.monotonic() + int(arguments[1])
            remote.settimeout(1)
            line = bytearray()
            with open(arguments[2], 'wb') as output, open(arguments[2]+'.p4.log', 'wb') as uart:
                while time.monotonic() < deadline:
                    uart.write(console.read(4096))
                    try:
                        data = remote.recv(4096)
                    except socket.timeout:
                        continue
                    if not data:
                        raise RuntimeError('Live diagnostic connection closed early')
                    for byte in data:
                        if byte == 10:
                            # Diagnostic text only; never persist captured PCM packets.
                            if b' PCM ' not in line:
                                output.write(line + b'\n')
                            line.clear()
                        elif len(line) < 2048:
                            line.append(byte)
            print(f'S3 live diagnostics saved to {arguments[2]}', flush=True)
            return
        with socket.socket() as listener:
            listener.bind(('127.0.0.1', 0))
            listener.listen(1)
            listener.settimeout(15)
            port = listener.getsockname()[1]
            command = [sys.executable, '-m', 'esptool', '--chip', 'esp32s3', '--port',
                       f'socket://127.0.0.1:{port}', '--before', 'no-reset', '--after', 'no-reset', *arguments]
            print(f'Executing S3 {arguments[0]} through P4', flush=True)
            process = subprocess.Popen(command)
            try:
                local, _ = listener.accept()
                with local:
                    remote.settimeout(5)
                    local.settimeout(5)
                    deadline = time.monotonic() + 900
                    while process.poll() is None and time.monotonic() < deadline:
                        readable, _, _ = select.select([local, remote], [], [], 1)
                        closed = False
                        for source in readable:
                            data = source.recv(4096)
                            if not data:
                                closed = True
                                break
                            (remote if source is local else local).sendall(data)
                        if closed:
                            break
                code = process.wait(timeout=15)
                if code:
                    raise RuntimeError(f'esptool failed with exit code {code}')
            finally:
                if process.poll() is None:
                    process.terminate()
                    process.wait(timeout=10)
    # Allow the native bridge to release USB ownership and restore S3 application.
    time.sleep(2)


def main():
    os.umask(0o077)  # Flash backups may contain saved device configuration.
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--serial', default='/dev/cu.usbmodem5CF71081441')
    parser.add_argument('--plan', type=Path, required=True, help='JSON array of esptool argument arrays')
    args = parser.parse_args()
    plan = json.loads(args.plan.read_text())
    if not isinstance(plan, list) or not plan or any(not isinstance(row, list) or not all(isinstance(item, str) for item in row) for row in plan):
        raise ValueError('Plan must contain nonempty esptool argument arrays')
    console, host, key = discover_console(args.serial)
    try:
        for arguments in plan:
            operation(host, key, arguments, console)
    finally:
        console.close()


if __name__ == '__main__':
    main()
