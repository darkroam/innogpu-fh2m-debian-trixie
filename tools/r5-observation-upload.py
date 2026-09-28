#!/usr/bin/env python3
"""Bounded capture upload, or emit a persistent systemd unit; never installs it."""
import argparse
import hashlib
from http.server import BaseHTTPRequestHandler, HTTPServer
import ipaddress
import json
import os
from pathlib import Path
import re

MAX_CAPTURE = 1024**3 + 65536


def load_config(path):
    config = json.loads(path.read_text())
    for key in ('listen', 'peer'):
        address = ipaddress.IPv4Address(config[key])
        if address.is_unspecified or address.is_multicast or address.is_reserved:
            raise ValueError('explicit unicast address required')
    if (type(config['port']) is not int or not 1024 <= config['port'] <= 65535 or
            not re.fullmatch('[a-f0-9]{32}', config['session'])):
        raise ValueError('port/session mismatch')
    root = Path(config['output'])
    if not root.is_absolute() or root.resolve(strict=True) != root or not root.is_dir():
        raise ValueError('existing absolute output directory without symlinks required')
    config['output'] = root
    return config


def durable_json(path, value):
    with path.open('x', encoding='utf-8') as f:
        json.dump(value, f, indent=2)
        f.write('\n'); f.flush(); os.fsync(f.fileno())
    fd = os.open(path.parent, os.O_RDONLY | os.O_DIRECTORY)
    try:
        os.fsync(fd)
    finally:
        os.close(fd)


def save_upload(stream, root, size, expected, session):
    """Never overwrite a prior or interrupted upload. Integrity != coverage."""
    if (type(size) is not int or not 8 <= size <= MAX_CAPTURE or
            not re.fullmatch('[a-f0-9]{64}', expected) or
            not re.fullmatch('[a-f0-9]{32}', session)):
        raise ValueError('invalid upload identity/size')
    attempt = root / session
    attempt.mkdir(mode=0o700)  # Atomic one-shot; restart preserves incomplete attempts.
    fd = os.open(root, os.O_RDONLY | os.O_DIRECTORY)
    try:
        os.fsync(fd)
    finally:
        os.close(fd)
    digest = hashlib.sha256(); stored = 0; error = None
    try:
        with (attempt/'capture.r5o').open('xb') as f:
            try:
                while stored < size:
                    block = stream.read(min(65536, size-stored))
                    if not block:
                        raise ValueError('short upload')
                    f.write(block); digest.update(block); stored += len(block)
            finally:
                f.flush(); os.fsync(f.fileno())
        if digest.hexdigest() != expected:
            raise ValueError('SHA mismatch')
    except (OSError, ValueError) as exc:
        error = str(exc)
    result = dict(upload='SHA_MATCH' if error is None else 'FAILED_OR_UNVERIFIED',
                  session=session, bytes=stored, sha256=digest.hexdigest(),
                  expected_sha256=expected, error=error, coverage='UNVERIFIED', R5='FAIL')
    durable_json(attempt/'receipt.json', result)
    return result


class Handler(BaseHTTPRequestHandler):
    # One LAN peer/session only. IP filtering is not authentication on an untrusted LAN.
    def reply(self, status, value):
        body = json.dumps(value).encode()
        self.send_response(status)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers(); self.wfile.write(body)

    def allowed(self):
        return self.client_address[0] == self.server.config['peer']

    def do_GET(self):
        if not self.allowed() or self.path != '/health':
            self.send_error(404); return
        self.reply(200, dict(service='READY_UPLOAD', PM='NOT_RUN', coverage='UNVERIFIED'))

    def do_POST(self):
        c = self.server.config
        if not self.allowed() or self.path != '/observation':
            self.send_error(404); return
        size = self.headers.get('Content-Length', '')
        sha = self.headers.get('X-Content-SHA256', '').lower()
        if (self.headers.get('X-R5-Session') != c['session'] or
                self.headers.get('Transfer-Encoding') is not None or
                len(self.headers.get_all('Content-Length', [])) != 1 or
                not re.fullmatch('[0-9]{1,10}', size) or
                not 8 <= int(size) <= MAX_CAPTURE or not re.fullmatch('[a-f0-9]{64}', sha)):
            self.send_error(400); return
        self.connection.settimeout(120)
        try:
            result = save_upload(self.rfile, c['output'], int(size), sha, c['session'])
        except FileExistsError:
            self.reply(409, dict(upload='PRESERVED_EXISTING_ATTEMPT')); return
        self.reply(200 if result['upload'] == 'SHA_MATCH' else 422, result)

    def log_message(self, *args):
        pass  # Do not put peer identities or headers in shared service logs.


def service_unit(config_path):
    config_path = config_path.absolute()
    config = load_config(config_path)
    script = Path(__file__).resolve()
    for p in (config_path, script, config['output']):
        if p.resolve(strict=True) != p or not re.fullmatch(r'/[A-Za-z0-9_./-]+', str(p)):
            raise ValueError('unit paths must be unambiguous, no symlinks/specifiers')
    return f'''[Unit]
Description=R5 bounded evidence upload (no PM action)
Wants=network-online.target
After=network-online.target

[Service]
Type=exec
User={os.getuid()}
ExecStart=/usr/bin/python3 {script} --serve {config_path}
Restart=on-failure
RestartSec=5
UMask=0077
NoNewPrivileges=yes
ProtectSystem=strict
ProtectHome=read-only
ReadWritePaths={config['output']}
PrivateTmp=yes
ProtectKernelTunables=yes
ProtectKernelModules=yes
ProtectControlGroups=yes
RestrictAddressFamilies=AF_INET

[Install]
WantedBy=multi-user.target
'''


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    modes = parser.add_mutually_exclusive_group(required=True)
    modes.add_argument('--serve', type=Path)
    modes.add_argument('--unit', type=Path, help='config to validate; print unit, do not install')
    args = parser.parse_args()
    if args.unit:
        print(service_unit(args.unit), end='')
    else:
        config = load_config(args.serve)
        with HTTPServer((config['listen'], config['port']), Handler) as server:
            server.config = config
            print('READY_UPLOAD; coverage=UNVERIFIED PM=NOT_RUN', flush=True)
            server.serve_forever()
