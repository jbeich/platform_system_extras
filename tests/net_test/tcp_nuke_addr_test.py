#!/usr/bin/python
#
# Copyright 2015 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import fcntl
import os
import socket
import struct
import threading
import time
import unittest


IPV4_LOOPBACK_ADDR = '127.0.0.1'
IPV6_LOOPBACK_ADDR = '::1'

SIOCKILLADDR = 0x8939

DEFAULT_TCP_PORT = 8001
DEFAULT_BUFFER_SIZE = 20
DEFAULT_TEST_MESSAGE = "TCP NUKE ADDR TEST"


def IPv6AcceptAndReceive(listening_socket, buffer_size=DEFAULT_BUFFER_SIZE):
  """Accepts a single connection and blocks receiving data from it.

  Args:
    listening_socket: A socket in LISTEN state.
    buffer_size: Size of buffer where to read a message.
  """
  connection, _ = listening_socket.accept()
  try:
    _ = connection.recv(buffer_size)
  finally:
    connection.close()


def IPv6ExchangeMessage(ip6_addr, tcp_port, message=DEFAULT_TEST_MESSAGE):
  """Creates a listening socket, accepts a connection and sends data to it.

  Args:
    ip6_addr: The IPv6 address.
    tcp_port: The TCP port to listen on.
    message: The message to send on the socket.
  """
  test_addr = (ip6_addr, tcp_port)
  listening_socket = socket.socket(socket.AF_INET6, socket.SOCK_STREAM)
  try:
    listening_socket.bind(test_addr)
    listening_socket.listen(1)
    receive_thread = threading.Thread(target=IPv6AcceptAndReceive,
                                      args=(listening_socket,))
    receive_thread.start()
    client_socket = socket.socket(socket.AF_INET6, socket.SOCK_STREAM)
    try:
      client_socket.connect(test_addr)
      client_socket.send(message)
    finally:
      client_socket.close()
      receive_thread.join()
  finally:
    listening_socket.close()


def IPv6KillAddrIoctl():
  """Calls the SIOCKILLADDR on IPv6 address family."""
  in6_ifreq = struct.pack('BBBBBBBBBBBBBBBBIi',
                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                          128, 1)
  datagram_socket = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM)
  fcntl.ioctl(datagram_socket.fileno(), SIOCKILLADDR, in6_ifreq)
  datagram_socket.close()


class TcpNukeAddrTest(unittest.TestCase):

  def testIPv6KillAddr(self):
    """Tests that SIOCKILLADDR works as expected.

    Relevant kernel commits:
      https://www.codeaurora.org/cgit/quic/la/kernel/msm-3.18/commit/net/ipv4/tcp.c?h=aosp/android-3.10&id=1dcd3a1fa2fe78251cc91700eb1d384ab02e2dd6
    """
    IPv6ExchangeMessage(IPV6_LOOPBACK_ADDR, DEFAULT_TCP_PORT)
    IPv6KillAddrIoctl()
    # Test passes if kernel does not crash.


if __name__ == "__main__":
  unittest.main()
