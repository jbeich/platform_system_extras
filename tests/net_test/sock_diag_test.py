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
import multinetwork_base
import net_test
import packets
import sock_diag


NUM_SOCKETS = 100

ALL_NON_TIME_WAIT = 0xffffffff & ~(1 << sock_diag.TCP_TIME_WAIT)

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


class SockDiagTest(multinetwork_base.MultiNetworkBaseTest):

  @staticmethod
  def _CreateLotsOfSockets():
    # Dict mapping (addr, sport, dport) tuples to socketpairs.
    socketpairs = {}
    for i in xrange(NUM_SOCKETS):
      family, addr = random.choice([(AF_INET, "127.0.0.1"), (AF_INET6, "::1")])
      socketpair = net_test.CreateSocketPair(family, SOCK_STREAM, addr)
      sport, dport = (socketpair[0].getsockname()[1],
                      socketpair[1].getsockname()[1])
      socketpairs[(addr, sport, dport)] = socketpair
    return socketpairs

  def setUp(self):
    self.sock_diag = sock_diag.SockDiag()
    self.socketpairs = self._CreateLotsOfSockets()

  def tearDown(self):
    [s.close() for socketpair in self.socketpairs.values() for s in socketpair]

  def assertSocketsClosed(self, socketpair):
    for sock in socketpair:
      self.assertRaisesErrno(errno.ENOTCONN, sock.getpeername)

  def assertSockDiagMatchesSocket(self, s, diag_msg):
    src, sport = s.getsockname()[0:2]
    dst, dport = s.getpeername()[0:2]
    self.assertEqual(diag_msg.id.src, self.sock_diag.PaddedAddress(src))
    self.assertEqual(diag_msg.id.dst, self.sock_diag.PaddedAddress(dst))
    self.assertEqual(diag_msg.id.sport, sport)
    self.assertEqual(diag_msg.id.dport, dport)

  def testFindsAllMySockets(self):
    sockets = self.sock_diag.DumpAllInetSockets(IPPROTO_TCP,
                                                states=ALL_NON_TIME_WAIT)
    self.assertGreaterEqual(len(sockets), NUM_SOCKETS)

    # Find the cookies for all of our sockets.
    cookies = {}
    for diag_msg, attrs in sockets:
      addr = self.sock_diag.GetSourceAddress(diag_msg)
      sport = diag_msg.id.sport
      dport = diag_msg.id.dport
      if (addr, sport, dport) in self.socketpairs:
        cookies[(addr, sport, dport)] = diag_msg.id.cookie
      elif (addr, dport, sport) in self.socketpairs:
        cookies[(addr, sport, dport)] = diag_msg.id.cookie

    # Did we find all the cookies?
    self.assertEquals(2 * NUM_SOCKETS, len(cookies))

    socketpairs = self.socketpairs.values()
    random.shuffle(socketpairs)
    for socketpair in socketpairs:
      for sock in socketpair:
        self.assertSockDiagMatchesSocket(
            sock,
            self.sock_diag.GetSockDiagForFd(sock))

  def testClosesSockets(self):
    for (addr, _, _), socketpair in self.socketpairs.iteritems():
      # Close one of the sockets.
      # This will send a RST that will close the other side as well.
      s = random.choice(socketpair)
      if random.randrange(0, 2) == 1:
        self.sock_diag.CloseSocketFromFd(s)
      else:
        diag_msg = self.sock_diag.GetSockDiagForFd(s)
        family = AF_INET6 if ":" in addr else AF_INET
        self.sock_diag.CloseSocket(family, IPPROTO_TCP, diag_msg.id)
      # Check that both sockets in the pair are closed.
      self.assertSocketsClosed(socketpair)

  def testNonTcpSockets(self):
    s = socket(AF_INET6, SOCK_DGRAM, 0)
    s.connect(("::1", 53))
    diag_msg = self.sock_diag.GetSockDiagForFd(s)
    self.assertRaisesErrno(errno.EINVAL, self.sock_diag.CloseSocketFromFd, s)

  # TODO:
  # Test that killing unix sockets returns EINVAL
  # Test that botching the cookie returns ENOENT


# TODO: Take a tun fd as input, make this a utility class, and reuse at least
# in forwarding_test.
# TODO: Generalize to support both incoming and outgoing connections.
class IncomingConnectionTest(multinetwork_base.MultiNetworkBaseTest):

  def OpenListenSocket(self, version):
    self.port = packets.RandomPort()
    family = {4: AF_INET, 6: AF_INET6}[version]
    address = {4: "0.0.0.0", 6: "::"}[version]
    s = net_test.TCPSocket(family)
    s.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
    s.bind((address, self.port))
    # We haven't configured inbound iptables marking, so bind explicitly.
    self.SetSocketMark(s, self.netid)
    s.listen(100)
    return s

  def _ReceiveAndExpectResponse(self, netid, packet, reply, msg):
    pkt = super(IncomingConnectionTest, self)._ReceiveAndExpectResponse(
        netid, packet, reply, msg)
    self.last_packet = pkt
    return pkt

  def ReceivePacketOn(self, netid, packet):
    super(IncomingConnectionTest, self).ReceivePacketOn(netid, packet)
    self.last_packet = packet

  def IncomingConnection(self, version, end_state, netid):
    self.version = version
    self.s = self.OpenListenSocket(version)
    self.end_state = end_state

    remoteaddr = self.remoteaddr = self.GetRemoteAddress(version)
    myaddr = self.myaddr = self.MyAddress(version, netid)

    if end_state == sock_diag.TCP_LISTEN:
      return

    desc, syn = packets.SYN(self.port, version, remoteaddr, myaddr)
    synack_desc, synack = packets.SYNACK(version, myaddr, remoteaddr, syn)
    msg = "Sent %s, expected %s" % (desc, synack_desc)
    reply = self._ReceiveAndExpectResponse(netid, syn, synack, msg)
    if end_state == sock_diag.TCP_SYN_RECV:
      return

    establishing_ack = packets.ACK(version, remoteaddr, myaddr, reply)[1]
    self.ReceivePacketOn(netid, establishing_ack)
    if end_state == sock_diag.TCP_ESTABLISHED:
      return


  def RstPacket(self):
    return packets.RST(self.version, self.myaddr, self.remoteaddr,
                       self.last_packet)

  def setUp(self):
    self.sock_diag = sock_diag.SockDiag()
    self.netid = random.choice(self.tuns.keys())

  def assertSocketClosed(self, sock):
    self.assertRaisesErrno(errno.ENOTCONN, sock.getpeername)

  def testTcpResets(self):
    self.IncomingConnection(6, sock_diag.TCP_LISTEN, self.netid)
    self.assertRaisesErrno(errno.EAGAIN, self.s.accept)
    self.sock_diag.CloseSocketFromFd(self.s)
    self.ExpectNoPacketsOn(self.netid, "Destroying listen socket")
    self.assertRaisesErrno(errno.EINVAL, self.s.accept)
    self.s.close()

    self.IncomingConnection(6, sock_diag.TCP_SYN_RECV, self.netid)
    self.assertRaisesErrno(errno.EAGAIN, self.s.accept)
    self.sock_diag.CloseSocketFromFd(self.s)
    self.ExpectNoPacketsOn(self.netid, "Destroying listen socket")
    self.assertRaisesErrno(errno.EINVAL, self.s.accept)
    self.s.close()

    self.IncomingConnection(6, sock_diag.TCP_ESTABLISHED, self.netid)
    accepted, peer = self.s.accept()
    self.sock_diag.CloseSocketFromFd(self.s)
    self.ExpectNoPacketsOn(self.netid, "Destroying listen socket")
    msg, rst = self.RstPacket()
    self.sock_diag.CloseSocketFromFd(accepted)
    self.ExpectPacketOn(self.netid, msg, rst)
    self.assertSocketClosed(accepted)


if __name__ == "__main__":
  unittest.main()
