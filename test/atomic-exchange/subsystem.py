#!/usr/bin/env python3
# Copyright 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Forward SFTP v3 requests to OpenSSH; inject faults at acknowledged protocol boundaries."""
from pathlib import Path
import os
import struct
import subprocess
import sys

root, executable = sys.argv[1:]
root = Path(root)


def packet(stream):
    header = stream.read(4)
    if not header:
        raise EOFError
    size, = struct.unpack(">I", header)
    if not 0 < size <= 1024 * 1024:
        raise ValueError("invalid SFTP packet size")
    body = stream.read(size)
    if len(body) != size:
        raise EOFError
    return body


def send(stream, body):
    stream.write(struct.pack(">I", len(body)) + body)
    stream.flush()


def status(request, code):
    return bytes([101]) + request[1:5] + struct.pack(">III", code, 0, 0)


def string(data):
    size, = struct.unpack_from(">I", data)
    return data[4:4 + size], data[4 + size:]


def forward(server, req):
    send(server.stdin, req)
    return packet(server.stdout)


def serve(server):
    paused = False
    while True:
        request = packet(sys.stdin.buffer)
        operation = request[0]
        fault_file = root / "fault"
        fault = fault_file.read_text() if fault_file.exists() else ""
        if fault == "no-replace" and operation in (3, 13, 18):
            def mapped(path):
                parent, name = os.fsdecode(path).rsplit("/", 1)
                folder = {"/inbound": "in", "/staging": "stage"}[parent]
                local = os.fsencode(root / folder / name)
                return struct.pack(">I", len(local)) + local

            source, rest = string(request[5:])
            if operation == 18:
                target, _ = string(rest)
                rest = mapped(target)
            request = request[:5] + mapped(source) + rest
        if operation == 18 and fault == "no-replace":
            source, rest = string(request[5:])
            target, _ = string(rest)
            try:
                os.link(source, target)
                os.unlink(source)
                reply = status(request, 0)
            except FileExistsError:
                reply = status(request, 4)
        elif operation == 6 and fault == "write-error":
            reply = status(request, 4)
        else:
            reply = forward(server, request)
            if operation == 1 and fault == "no-extension":
                reply = reply[:5]
            elif operation == 4 and fault == "stall-close":
                # Wait for the client to abandon the transport after the missing acknowledgement.
                sys.stdin.buffer.read()
                return
            elif operation == 4 and fault == "close-error":
                reply = status(request, 4)
            elif operation == 200 and fault == "drop-commit":
                return
            elif operation == 5 and fault == "short-read":
                reply = status(request, 1)
            elif operation == 5 and fault == "pause-read" and not paused:
                paused = True
                with (root / "gate-ready").open("wb", buffering=0) as ready:
                    ready.write(bytes([1]))
                with (root / "gate-release").open("rb", buffering=0) as release:
                    release.read(1)
            elif operation == 5 and fault == "replace-during-read" and not (root / "read-barrier").exists():
                # The first data reply is available but the downloader has not received it.
                visible = (root / "local/member").read_bytes()
                (root / "read-barrier").write_bytes(visible)
                (root / "replacement").replace(root / "in/member")
        send(sys.stdout.buffer, reply)


with subprocess.Popen([executable], stdin=subprocess.PIPE, stdout=subprocess.PIPE) as server:
    try:
        serve(server)
    except EOFError:
        pass
    finally:
        server.stdin.close()
