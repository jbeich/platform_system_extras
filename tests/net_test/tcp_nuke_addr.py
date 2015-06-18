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

import os
from threading import Thread
import time
import unittest
from time import sleep
import net_test
import fcntl
import socket
import struct

class TcpNukeAddrTest(net_test.NetworkTest):

  IPV4_LOOPBACK_ADDR = '127.0.0.1'
  IPV6_LOOPBACK_ADDR = '::1'
  TCP_PORT = 8001
  BUFFER_SIZE = 20
  MESSAGE = "TCP NUKE ADDR TEST"
  SIOCKILLADDR = 0x8939

  @classmethod
  def setUpClass(cls):
    super(TcpNukeAddrTest, cls).setUpClass()

  @classmethod
  def tearDownClass(cls):
    super(TcpNukeAddrTest, cls).tearDownClass()

  def setUp(self):
    return

  @classmethod
  def DatagramSocket(self, family):
    return socket.socket(family, socket.SOCK_DGRAM)

  @classmethod
  def IPv6TCPBlockingSocket(self):
    return socket.socket(socket.AF_INET6, socket.SOCK_STREAM)

  @classmethod
  def IPv6TcpRcvThread(self):
    # Create IPv6 TCP listener
    s = self.IPv6TCPBlockingSocket()
    s.bind((self.IPV6_LOOPBACK_ADDR, self.TCP_PORT))
    s.listen(1)
    conn, addr = s.accept()
    rcvd = conn.recv(self.BUFFER_SIZE)
    conn.close()

  @classmethod
  def IPv6CreateLoopbackTimeWaitSocket(self):
    # Create IPv6 TIME_WAIT socket by sending data to a TCP socket
    # and then immediately closing that connection.
    recv_thread = Thread(target = self.IPv6TcpRcvThread)
    recv_thread.start()
    sleep(0.01)
    s = self.IPv6TCPBlockingSocket()
    s.connect((self.IPV6_LOOPBACK_ADDR, self.TCP_PORT))
    s.send(self.MESSAGE)
    s.close()
    recv_thread.join()

  @classmethod
  def IPv6KillAddrIOCTL(self):
    s = self.DatagramSocket(socket.AF_INET6)
    fd = s.fileno()
    in6_ifreq = struct.pack('BBBBBBBBBBBBBBBBIi', 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,128,1)
    fcntl.ioctl(fd, self.SIOCKILLADDR, in6_ifreq)

  def testIPv6KillAddr(self):
    self.IPv6CreateLoopbackTimeWaitSocket()
    self.IPv6KillAddrIOCTL()

    # Pass if you do not crash
    self.assertTrue(1)

if __name__ == "__main__":
  unittest.main()
