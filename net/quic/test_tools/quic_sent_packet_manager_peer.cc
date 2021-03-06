// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/quic_sent_packet_manager_peer.h"

#include "net/quic/congestion_control/send_algorithm_interface.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_sent_packet_manager.h"

namespace net {
namespace test {

// static
void QuicSentPacketManagerPeer::SetSendAlgorithm(
    QuicSentPacketManager* sent_packet_manager,
    SendAlgorithmInterface* send_algorithm) {
  sent_packet_manager->send_algorithm_.reset(send_algorithm);
}

// static
size_t QuicSentPacketManagerPeer::GetNackCount(
    const QuicSentPacketManager* sent_packet_manager,
    QuicPacketSequenceNumber sequence_number) {
  return sent_packet_manager->packet_history_map_.find(
      sequence_number)->second->nack_count();
}

// static
QuicTime::Delta QuicSentPacketManagerPeer::rtt(
    QuicSentPacketManager* sent_packet_manager) {
  return sent_packet_manager->rtt_sample_;
}

}  // namespace test
}  // namespace net
