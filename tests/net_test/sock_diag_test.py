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

from errno import *
import os
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
import threading


NUM_SOCKETS = 100

ALL_NON_TIME_WAIT = 0xffffffff & ~(1 << sock_diag.TCP_TIME_WAIT)


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

  def assertSocketConnected(self, sock):
    sock.getpeername()  # No errors? Socket is alive and connected.

  def assertSocketsClosed(self, socketpair):
    for sock in socketpair:
      self.assertRaisesErrno(ENOTCONN, sock.getpeername)

  def assertSockDiagMatchesSocket(self, s, diag_msg):
    family = s.getsockopt(net_test.SOL_SOCKET, net_test.SO_DOMAIN)
    self.assertEqual(diag_msg.family, family)

    # TODO: The kernel (at least 3.10) seems only to fill in the first 4 bytes
    # of src and dst in the case of IPv4 addresses. This means we can't just do
    # something like:
    #  self.assertEqual(diag_msg.id.src, self.sock_diag.PaddedAddress(src))
    # because the trailing bytes might not match.
    # This seems like a bug because it might leaks kernel memory contents, but
    # regardless, work around that here.
    addrlen = {AF_INET: 4, AF_INET6: 16}[family]

    src, sport = s.getsockname()[0:2]
    self.assertEqual(diag_msg.id.sport, sport)
    self.assertEqual(diag_msg.id.src[:addrlen],
                     self.sock_diag.RawAddress(src))

    if self.sock_diag.GetDestinationAddress(diag_msg) not in ["0.0.0.0", "::"]:
      dst, dport = s.getpeername()[0:2]
      self.assertEqual(diag_msg.id.dst[:addrlen],
                       self.sock_diag.RawAddress(dst))
      self.assertEqual(diag_msg.id.dport, dport)
    else:
      assertRaisesErrno(ENOTCONN, s.getpeername)

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

        # Get the cookie wrong and ensure that we get an error and the socket
        # is not closed.
        real_cookie = diag_msg.id.cookie
        diag_msg.id.cookie = os.urandom(len(real_cookie))
        self.assertRaisesErrno(
            ENOENT,
            self.sock_diag.CloseSocket, family, IPPROTO_TCP, diag_msg.id)
        self.assertSocketConnected(s)

        # Now close it with the correct cookie.
        diag_msg.id.cookie = real_cookie
        self.sock_diag.CloseSocket(family, IPPROTO_TCP, diag_msg.id)

      # Check that both sockets in the pair are closed.
      self.assertSocketsClosed(socketpair)

  def testNonTcpSockets(self):
    s = socket(AF_INET6, SOCK_DGRAM, 0)
    s.connect(("::1", 53))
    diag_msg = self.sock_diag.GetSockDiagForFd(s)
    self.assertRaisesErrno(EOPNOTSUPP, self.sock_diag.CloseSocketFromFd, s)

  def testNonSockDiagCommand(self):
    def DiagDump(code):
      sock_id = self.sock_diag._EmptyInetDiagSockId()
      req = sock_diag.InetDiagReqV2((AF_INET6, IPPROTO_TCP, 0, 0xffffffff,
                                     sock_id))
      self.sock_diag._Dump(code, req, sock_diag.InetDiagMsg)

    op = sock_diag.SOCK_DIAG_BY_FAMILY
    DiagDump(op)  # No errors? Good.
    self.assertRaisesErrno(EINVAL, DiagDump, op + 17)

  # TODO:
  # Test that killing unix sockets returns EOPNOTSUPP.


class SocketExceptionThread(threading.Thread):

  def __init__(self, sock, operation):
    self.exception = None
    super(SocketExceptionThread, self).__init__()
    self.daemon = True
    self.sock = sock
    self.operation = operation

  def run(self):
    try:
      self.operation(self.sock)
    except Exception, e:
      self.exception = e


# TODO: Take a tun fd as input, make this a utility class, and reuse at least
# in forwarding_test.
# TODO: Generalize to support both incoming and outgoing connections.
class TcpTest(multinetwork_base.MultiNetworkBaseTest):

  def setUp(self):
    super(TcpTest, self).setUp()
    self.sock_diag = sock_diag.SockDiag()
    self.netid = random.choice(self.tuns.keys())

  def OpenListenSocket(self, version):
    self.port = packets.RandomPort()
    family = {4: AF_INET, 6: AF_INET6}[version]
    address = {4: "0.0.0.0", 6: "::"}[version]
    s = net_test.Socket(family, SOCK_STREAM, IPPROTO_TCP)
    s.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
    s.bind((address, self.port))
    # We haven't configured inbound iptables marking, so bind explicitly.
    self.SelectInterface(s, self.netid, "mark")
    s.listen(100)
    return s

  def _ReceiveAndExpectResponse(self, netid, packet, reply, msg):
    pkt = super(TcpTest, self)._ReceiveAndExpectResponse(netid, packet,
                                                         reply, msg)
    self.last_packet = pkt
    return pkt

  def ReceivePacketOn(self, netid, packet):
    super(TcpTest, self).ReceivePacketOn(netid, packet)
    self.last_packet = packet

  def RstPacket(self):
    return packets.RST(self.version, self.myaddr, self.remoteaddr,
                       self.last_packet)

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
    msg = "Received %s, expected to see reply %s" % (desc, synack_desc)
    reply = self._ReceiveAndExpectResponse(netid, syn, synack, msg)
    if end_state == sock_diag.TCP_SYN_RECV:
      return

    establishing_ack = packets.ACK(version, remoteaddr, myaddr, reply)[1]
    self.ReceivePacketOn(netid, establishing_ack)

    self.accepted, _ = self.s.accept()
    desc, data = packets.ACK(version, myaddr, remoteaddr, establishing_ack,
                             payload=net_test.UDP_PAYLOAD)

    if end_state == sock_diag.TCP_ESTABLISHED:
      return

    self.accepted.send(net_test.UDP_PAYLOAD)
    self.ExpectPacketOn(netid, msg + ": expecting %s" % desc, data)

    desc, fin = packets.FIN(version, remoteaddr, myaddr, data)
    fin = packets._GetIpLayer(version)(str(fin))
    ack_desc, ack = packets.ACK(version, myaddr, remoteaddr, fin)
    msg = "Received %s, expected to see reply %s" % (desc, ack_desc)

    # TODO: Why can't we use this?
    #   self._ReceiveAndExpectResponse(netid, fin, ack, msg)
    self.ReceivePacketOn(netid, fin)
    time.sleep(0.1)
    self.ExpectPacketOn(netid, msg + ": expecting %s" % ack_desc, ack)
    if end_state == sock_diag.TCP_CLOSE_WAIT:
      return

  def assertSocketClosed(self, sock):
    self.assertRaisesErrno(ENOTCONN, sock.getpeername)

  def CheckRstOnClose(self, sock, expect_reset, msg):
    self.sock_diag.CloseSocketFromFd(sock)
    self.assertRaisesErrno(EINVAL, sock.accept)
    if expect_reset:
      desc, rst = self.RstPacket()
      msg = "%s: expecting %s: " % (msg, desc)
      self.ExpectPacketOn(self.netid, msg, rst)
    else:
      msg = "%s: " % msg
      self.ExpectNoPacketsOn(self.netid, msg)
    sock.close()

  def testTcpResets(self):
    for version in [4, 6]:
      msg = "Closing incoming IPv%d TCP_LISTEN socket" % version
      self.IncomingConnection(version, sock_diag.TCP_LISTEN, self.netid)
      self.CheckRstOnClose(self.s, False, msg)

      msg = "Closing incoming IPv%d TCP_SYN_RECV socket" % version
      self.IncomingConnection(version, sock_diag.TCP_SYN_RECV, self.netid)
      self.CheckRstOnClose(self.s, False, msg)
      # TODO: check that the closing the embryonic socket sends a RST.

      msg = "Closing incoming IPv%d TCP_ESTABLISHED socket" % version
      self.IncomingConnection(version, sock_diag.TCP_ESTABLISHED, self.netid)
      self.CheckRstOnClose(self.s, False, msg)
      msg = "Closing accepted IPv%d TCP_ESTABLISHED socket" % version
      self.CheckRstOnClose(self.accepted, True, msg)

      msg = "Closing incoming IPv%d TCP_CLOSE_WAIT socket" % version
      self.IncomingConnection(version, sock_diag.TCP_CLOSE_WAIT, self.netid)
      self.CheckRstOnClose(self.s, False, msg)
      msg = "Closing accepted IPv%d TCP_ESTABLISHED socket" % version
      self.CheckRstOnClose(self.accepted, True, msg)

  def CloseDuringBlockingCall(self, sock, call, expected_errno):
    thread = SocketExceptionThread(sock, call)
    thread.start()
    time.sleep(0.1)
    self.sock_diag.CloseSocketFromFd(sock)
    thread.join(1)
    self.assertFalse(thread.is_alive())
    self.assertIsNotNone(thread.exception)
    self.assertTrue(isinstance(thread.exception, IOError),
                    "Expected IOError, got %s" % thread.exception)
    self.assertEqual(expected_errno, thread.exception.errno)
    self.assertSocketClosed(sock)

  def testAcceptInterrupted(self):
    for version in [4, 6]:
      self.IncomingConnection(version, sock_diag.TCP_LISTEN, self.netid)
      self.CloseDuringBlockingCall(self.s, lambda sock: sock.accept(), EINVAL)
      self.assertRaisesErrno(ECONNABORTED, self.s.send, "foo")
      self.assertRaisesErrno(EINVAL, self.s.accept)

  def testReadInterrupted(self):
    for version in [4, 6]:
      self.IncomingConnection(version, sock_diag.TCP_ESTABLISHED, self.netid)
      self.CloseDuringBlockingCall(self.accepted, lambda sock: sock.recv(4096),
                                   ECONNABORTED)
      self.assertRaisesErrno(EPIPE, self.accepted.send, "foo")


  def testConnectInterrupted(self):
    for version in [4, 6]:
      family = {4: AF_INET, 6: AF_INET6}[version]
      s = net_test.Socket(family, SOCK_STREAM, IPPROTO_TCP)
      self.SelectInterface(s, self.netid, "mark")
      remoteaddr = self.GetRemoteAddress(version)
      s.bind(("", 0))
      _, sport = s.getsockname()[:2]
      self.CloseDuringBlockingCall(
          s, lambda sock: sock.connect((remoteaddr, 53)), ECONNABORTED)
      desc, syn = packets.SYN(53, version, self.MyAddress(version, self.netid),
                              remoteaddr, sport=sport, seq=None)
      self.ExpectPacketOn(self.netid, desc, syn)
      msg = "SOCK_DESTROY of socket in connect, expected no RST"
      self.ExpectNoPacketsOn(self.netid, msg)


if __name__ == "__main__":
  unittest.main()
