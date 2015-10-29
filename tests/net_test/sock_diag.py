#!/usr/bin/python
#
# Copyright 2014 The Android Open Source Project
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

"""Partial Python implementation of iproute functionality."""

# pylint: disable=g-bad-todo

import errno
import os
import random
import socket
import struct
import sys

import cstruct
import net_test
import netlink

SO_PROTOCOL = 38
SO_DOMAIN = 39

### Base netlink constants. See include/uapi/linux/netlink.h.
NETLINK_SOCK_DIAG = 4

### sock_diag constants. See include/uapi/linux/sock_diag.h.
# Message types.
SOCK_DIAG_BY_FAMILY = 20
SOCK_DESTROY = 21

### inet_diag_constants. See include/uapi/linux/inet_diag.h
# Message types.
TCPDIAG_GETSOCK = 18

INET_DIAG_NONE = 0
INET_DIAG_MEMINFO = 1
INET_DIAG_INFO = 2
INET_DIAG_VEGASINFO = 3
INET_DIAG_CONG = 4
INET_DIAG_TOS = 5
INET_DIAG_TCLASS = 6
INET_DIAG_SKMEMINFO = 7
INET_DIAG_SHUTDOWN = 8
INET_DIAG_DCTCPINFO = 9

# Data structure formats.
# These aren't constants, they're classes. So, pylint: disable=invalid-name
InetDiagSockId = cstruct.Struct("InetDiagSockId", "!HH16s16sI8s",
                                "sport dport src dst iface cookie")
InetDiagReqV2 = cstruct.Struct("InetDiagReqV2", "=BBBxIS", "family protocol ext states id",
                               [InetDiagSockId])
InetDiagMsg = cstruct.Struct("InetDiagMsg", "=BBBBSLLLLL",
                             "family state timer retrans id expires rqueue wqueue uid inode",
                             [InetDiagSockId])

class SockDiagSocket(netlink.NetlinkSocket):

  FAMILY = NETLINK_SOCK_DIAG
  NL_DEBUG = []

  def _Decode(self, command, msg, nla_type, nla_data):
    """Decodes netlink attributes to Python types."""
    if msg.family == 2 or msg.family == 10:
      name = self._GetConstantName(__name__, nla_type, "INET_DIAG")
    else:
      # Don't know what this is. Leave it as an integer.
      name = nla_type

    if name in ["INET_DIAG_SHUTDOWN", "INET_DIAG_TOS", "INET_DIAG_TCLASS"]:
      data = ord(nla_data)
    elif name == "INET_DIAG_CONG":
      data = nla_data.strip("\x00")
    else:
      data = nla_data

    return name, data

  @staticmethod
  def _EmptyInetDiagSockId():
    return InetDiagSockId(("\x00" * len(InetDiagSockId)))

  def MaybeDebugCommand(self, command, data):
    name = self._GetConstantName(__name__, command, "SOCK_")
    if "ALL" not in self.NL_DEBUG and "SOCK" not in self.NL_DEBUG:
      return
    parsed = self._ParseNLMsg(data, InetDiagReqV2)
    print "%s %s" % (name, str(parsed))

  def DumpSockets(self, family, protocol, ext, states, sock_id):
    if sock_id is None:
      sock_id = self._EmptyInetDiagSockId()

    diag_req = InetDiagReqV2((family, protocol, ext, states, sock_id))
    return self._Dump(SOCK_DIAG_BY_FAMILY, diag_req, InetDiagMsg)

  def DumpAllInetSockets(self, protocol, sock_id=None, ext=0, states=0xffffffff):
    # TODO: DumpSockets(AF_UNSPEC, ...) results in diag_msg structures that are
    # 60 instead of 72 bytes long, why?
    sockets = []
    for family in [socket.AF_INET, socket.AF_INET6]:
      sockets += self.DumpSockets(family, protocol, ext, states, None)
    return sockets

  @staticmethod
  def GetSourceAddress(diag_msg):
    addrlen = {socket.AF_INET:4, socket.AF_INET6: 16}[diag_msg.family]
    return socket.inet_ntop(diag_msg.family, diag_msg.id.src[:addrlen])

  @staticmethod
  def PaddedAddress(family, addr):
    padded = socket.inet_pton(family, addr)
    if len(padded) < 16:
      padded += "\x00" * (16 - len(padded))
    return padded

  @classmethod
  def DiagReqFromSocket(cls, s):
    family = s.getsockopt(socket.SOL_SOCKET, SO_DOMAIN)
    protocol = s.getsockopt(socket.SOL_SOCKET, SO_PROTOCOL)
    iface = s.getsockopt(socket.SOL_SOCKET, net_test.SO_BINDTODEVICE)
    src, sport = s.getsockname()[:2]
    dst, dport = s.getpeername()[:2]
    src = cls.PaddedAddress(family, src)
    dst = cls.PaddedAddress(family, dst)
    sock_id = InetDiagSockId((sport, dport, src, dst, iface, "\x00" * 8))
    return InetDiagReqV2((family, protocol, 0, 0xffffffff, sock_id))

  def GetSocketFromFd(self, s):
    req = self.DiagReqFromSocket(s)
    for diag_msg in self._Dump(SOCK_DIAG_BY_FAMILY, req, InetDiagMsg):
      return diag_msg
    raise ValueError("Dump of %s returned no sockets" % req)

  def CloseSocketFromFd(self, s):
    req = self.DiagReqFromSocket(s)
    self._SendNlRequest(SOCK_DESTROY, req.Pack(),
                        netlink.NLM_F_REQUEST | netlink.NLM_F_ACK)

  def GetSocket(self, family, protocol, sock_id, ext=0, states=0xffffffff):
    req = InetDiagReqV2((family, protocol, ext, states, sock_id))
    self._SendNlRequest(SOCK_DIAG_BY_FAMILY, req.Pack(), netlink.NLM_F_REQUEST)
    data = self._Recv()
    return self._ParseNLMsg(data, InetDiagMsg)[0]

  def CloseSocket(self, family, protocol, sock_id, ext=0, states=0xffffffff):
    req = InetDiagReqV2((family, protocol, ext, states, sock_id))
    self._SendNlRequest(SOCK_DESTROY, req.Pack(),
                        netlink.NLM_F_REQUEST | netlink.NLM_F_ACK)


if __name__ == "__main__":
  n = SockDiagSocket()
  sock_id = n._EmptyInetDiagSockId()
  sock_id.dport = 443
  family = socket.AF_INET6
  protocol = socket.IPPROTO_TCP
  ext = 0
  states = 0xffffffff
  ext = 1 << (INET_DIAG_TOS - 1) | 1 << (INET_DIAG_TCLASS - 1)
  diag_msgs = n.DumpSockets(family, protocol, ext, states, sock_id)
  print diag_msgs
