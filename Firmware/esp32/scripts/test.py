# This example code is in the Public Domain (or CC0 licensed, at your option.)

# Unless required by applicable law or agreed to in writing, this
# software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied.

# -*- coding: utf-8 -*-

from __future__ import print_function
from __future__ import unicode_literals
import os
import sys
import re
import struct
import socket
import numpy as np


# -----------  Config  ----------
PORT = 3333
INTERFACE = 'eth0'
# -------------------------------


def tcp_client(address, payload):
    for res in socket.getaddrinfo(address, PORT, socket.AF_UNSPEC,
                                  socket.SOCK_STREAM, 0, socket.AI_PASSIVE):
        family_addr, socktype, proto, canonname, addr = res
    try:
        sock = socket.socket(family_addr, socket.SOCK_STREAM)
    except socket.error as msg:
        print('Could not create socket: ' + str(msg[0]) + ': ' + msg[1])
        raise
    try:
        sock.connect(addr)
    except socket.error as msg:
        print('Could not open socket: ', msg)
        sock.close()
        raise
    sock.sendall(bytes(payload, 'utf-8'))
    while 1:
        try:
            data = sock.recv(1024)
            if not data:
                return
            numbers = np.array(list(struct.iter_unpack('B', data)))
            # print(numbers.transpose())
            print(f'Received: {data.__len__()} bytes, byte[0] = {numbers[0, 0]}')
            # print(data.decode())
        except KeyboardInterrupt:
            exit(0)
    sock.close()
    return data


if __name__ == '__main__':
    if sys.argv[2:]:    # if two arguments provided:
        # Usage: example_test.py <server_address> <message_to_send_to_server>
        tcp_client(sys.argv[1], sys.argv[2])