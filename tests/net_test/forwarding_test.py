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

import itertools
import random
import unittest

from socket import *

import iproute
import multinetwork_base
import net_test
import packets


class ForwardingTest(multinetwork_base.MultiNetworkBaseTest):

  TCP_TIME_WAIT = 6

  @classmethod
  def ForwardBetweenInterfaces(cls, enabled, iface1, iface2):
    for iif, oif in itertools.permutations([iface1, iface2]):
      cls.iproute.IifRule(6, enabled, cls.GetInterfaceName(iif),
                          cls._TableForNetid(oif), cls.PRIORITY_IIF)

  @classmethod
  def setUpClass(cls):
    super(ForwardingTest, cls).setUpClass()

    # Pick an interface to send traffic on and two to forward traffic between.
    cls.netid, cls.iface1, cls.iface2 = random.sample(cls.tuns.keys(), 3)
    cls.ForwardBetweenInterfaces(True, cls.iface1, cls.iface2)

  @classmethod
  def tearDownClass(cls):
    super(ForwardingTest, cls).tearDownClass()
    cls.ForwardBetweenInterfaces(False, cls.iface1, cls.iface2)

  def testCrash(self):
    listenport = packets.RandomPort()
    self.listensocket = net_test.IPv6TCPSocket()
    self.listensocket.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
    self.listensocket.bind(("::", listenport))
    self.listensocket.listen(100)
    self.SetSocketMark(self.listensocket, self.netid)

    version = 6
    remoteaddr = self.GetRemoteAddress(version)
    myaddr = self.MyAddress(version, self.netid)

    desc, syn = packets.SYN(listenport, version, remoteaddr, myaddr)
    synack_desc, synack = packets.SYNACK(version, myaddr, remoteaddr, syn)
    msg = "Sent %s, expected %s" % (desc, synack_desc)
    reply = self._ReceiveAndExpectResponse(self.netid, syn, synack, msg)

    establishing_ack = packets.ACK(version, remoteaddr, myaddr, reply)[1]
    self.ReceivePacketOn(self.netid, establishing_ack)
    accepted, peer = self.listensocket.accept()
    remoteport = accepted.getpeername()[1]

    accepted.close()
    desc, fin = packets.FIN(version, myaddr, remoteaddr, establishing_ack)
    self.ExpectPacketOn(self.netid, msg + ": expecting %s after close" % desc,
                        fin)

    desc, finack = packets.FIN(version, remoteaddr, myaddr, fin)
    self.ReceivePacketOn(self.netid, finack)

    # Check our socket is now in TIME_WAIT.
    sockets = self.ReadProcNetSocket("tcp6")
    mysrc = "%s:%04X" % (net_test.FormatSockStatAddress(myaddr), listenport)
    mydst = "%s:%04X" % (net_test.FormatSockStatAddress(remoteaddr), remoteport)
    state = None
    sockets = [s for s in sockets if s[0] == mysrc and s[1] == mydst]
    self.assertEquals(1, len(sockets))
    self.assertEquals("%02X" % self.TCP_TIME_WAIT, sockets[0][2])

    # Remove our IP address.
    self.iproute.DelAddress(myaddr, 64, self.ifindices[self.netid])

    self.ReceivePacketOn(self.iface1, finack)
    self.ReceivePacketOn(self.iface1, establishing_ack)
    self.ReceivePacketOn(self.iface1, establishing_ack)
    self.ReceivePacketOn(self.iface1, establishing_ack)
    self.ReceivePacketOn(self.iface1, establishing_ack)
    self.ReceivePacketOn(self.iface1, establishing_ack)
    self.ReceivePacketOn(self.iface1, establishing_ack)
    self.ReceivePacketOn(self.iface1, establishing_ack)
    # No crashes? Good.


if __name__ == "__main__":
  unittest.main()
