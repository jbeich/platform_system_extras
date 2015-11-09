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

import errno
import random
from socket import *
import time
import unittest

import csocket
import cstruct
import net_test
import sock_diag


NUM_SOCKETS = 100


def CreateSocketPair(family, addr):
  clientsock = socket(family, SOCK_STREAM, 0)
  listensock = socket(family, SOCK_STREAM, 0)
  listensock.bind((addr, 0))
  addr = listensock.getsockname()
  listensock.listen(1)
  clientsock.connect(addr)
  acceptedsock, _ = listensock.accept()
  listensock.close()
  return clientsock, acceptedsock


class SockDiagTest(net_test.NetworkTest):

  def assertSocketsClosed(self, socketpair):
    for sock in socketpair:
      self.assertRaisesErrno(errno.ENOTCONN, sock.getpeername)

  def setUp(self):
    self.sock_diag = sock_diag.SockDiagSocket()
    self.socketpairs = self._CreateLotsOfSockets()

  def tearDown(self):
    [s.close() for socketpair in self.socketpairs.values() for s in socketpair]

  @staticmethod
  def _CreateLotsOfSockets():
    # Dict mapping (addr, sport, dport) tuples to socketpairs.
    socketpairs = {}
    for i in xrange(NUM_SOCKETS):
      family, addr = random.choice([(AF_INET, "127.0.0.1"), (AF_INET6, "::1")])
      socketpair = CreateSocketPair(family, addr)
      sport, dport = (socketpair[0].getsockname()[1],
                      socketpair[1].getsockname()[1])
      socketpairs[(addr, sport, dport)] = socketpair
    return socketpairs

  def testFindsAllMySockets(self):
    sockets = self.sock_diag.DumpAllInetSockets(IPPROTO_TCP)
    self.assertGreaterEqual(len(sockets), NUM_SOCKETS)

  def testClosesSockets(self):
    cookies = {}
    for diag_msg, attrs in self.sock_diag.DumpAllInetSockets(IPPROTO_TCP):
      addr = self.sock_diag.GetSourceAddress(diag_msg)
      sport = diag_msg.id.sport
      dport = diag_msg.id.dport
      cookies[(addr, sport, dport)] = diag_msg.id.cookie

    self.assertEquals(2 * NUM_SOCKETS, len(cookies))

    for (addr, sport, dport), socketpair in self.socketpairs.iteritems():
      sock_id = self.sock_diag._EmptyInetDiagSockId()
      family = AF_INET6 if ":" in addr else AF_INET
      rawaddr = inet_pton(family, addr)
      rawaddr += (16 - len(rawaddr)) * "\x00"

      sock_id.src = sock_id.dst = rawaddr
      sock_id.sport = sport
      sock_id.dport = dport
      sock_id.cookie = cookies[(addr, sport, dport)]

      diag_msg, attrs = self.sock_diag.GetSocket(family, IPPROTO_TCP, sock_id)
      self.assertEqual(diag_msg.id.src, rawaddr)
      self.assertEqual(diag_msg.id.dst, rawaddr)
      self.assertEqual(diag_msg.id.sport, sport)
      self.assertEqual(diag_msg.id.dport, dport)
      s = self.sock_diag.GetSocket(family, IPPROTO_TCP, sock_id)
      self.assertEqual(diag_msg.id.src, rawaddr)
      self.assertEqual(diag_msg.id.dst, rawaddr)
      self.assertEqual(diag_msg.id.sport, sport)
      self.assertEqual(diag_msg.id.dport, dport)

      # Close socket and check it's closed.
      self.sock_diag.CloseSocket(family, IPPROTO_TCP, sock_id)

      # Close other side.
      sock_id.sport = dport
      sock_id.dport = sport
      sock_id.cookie = cookies[(addr, dport, sport)]
      self.sock_diag.CloseSocket(family, IPPROTO_TCP, sock_id)

      self.assertSocketsClosed(socketpair)

  def testNonTcpSockets(self):
    s = socket(AF_INET6, SOCK_DGRAM, 0)
    s.connect(("::1", 53))
    diag_msg = self.sock_diag.GetSocketFromFd(s)
    self.assertRaisesErrno(errno.EINVAL, self.sock_diag.CloseSocketFromFd, s)
    
  # TODO:
  # Test that killing UDP sockets does EINVAL
  # Test that killing unix sockets returns EINVAL
  # Test that botching the cookie returns ENOENT
  # Test that killing accepted sockets works?
  # Test that killing sockets in connect() works?


if __name__ == "__main__":
  unittest.main()
