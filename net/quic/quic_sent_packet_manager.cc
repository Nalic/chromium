// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_sent_packet_manager.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "net/quic/congestion_control/pacing_sender.h"
#include "net/quic/quic_ack_notifier_manager.h"

using std::make_pair;
using std::min;

// TODO(rtenneti): Remove this.
// Do not flip this flag until the flakiness of the
// net/tools/quic/end_to_end_test is fixed.
// If true, then QUIC connections will track the retransmission history of a
// packet so that an ack of a previous transmission will ack the data of all
// other transmissions.
bool FLAGS_track_retransmission_history = false;

// A test-only flag to prevent the RTO from backing off when multiple sequential
// tail drops occur.
bool FLAGS_limit_rto_increase_for_tests = false;

// Do not remove this flag until the Finch-trials described in b/11706275
// are complete.
// If true, QUIC connections will support the use of a pacing algorithm when
// sending packets, in an attempt to reduce packet loss.  The client must also
// request pacing for the server to enable it.
bool FLAGS_enable_quic_pacing = false;

namespace net {
namespace {
static const int kBitrateSmoothingPeriodMs = 1000;
static const int kHistoryPeriodMs = 5000;

static const int kDefaultRetransmissionTimeMs = 500;
// TCP RFC calls for 1 second RTO however Linux differs from this default and
// define the minimum RTO to 200ms, we will use the same until we have data to
// support a higher or lower value.
static const int kMinRetransmissionTimeMs = 200;
static const int kMaxRetransmissionTimeMs = 60000;
static const size_t kMaxRetransmissions = 10;

// We want to make sure if we get a nack packet which triggers several
// retransmissions, we don't queue up too many packets.  10 is TCP's default
// initial congestion window(RFC 6928).
static const size_t kMaxRetransmissionsPerAck = kDefaultInitialWindow;

// TCP retransmits after 3 nacks.
static const size_t kNumberOfNacksBeforeRetransmission = 3;

COMPILE_ASSERT(kHistoryPeriodMs >= kBitrateSmoothingPeriodMs,
               history_must_be_longer_or_equal_to_the_smoothing_period);
}  // namespace

#define ENDPOINT (is_server_ ? "Server: " : " Client: ")

QuicSentPacketManager::HelperInterface::~HelperInterface() {
}

QuicSentPacketManager::QuicSentPacketManager(bool is_server,
                                             HelperInterface* helper,
                                             const QuicClock* clock,
                                             CongestionFeedbackType type)
    : is_server_(is_server),
      helper_(helper),
      clock_(clock),
      send_algorithm_(SendAlgorithmInterface::Create(clock, type)),
      rtt_sample_(QuicTime::Delta::Infinite()),
      consecutive_rto_count_(0),
      using_pacing_(false) {
}

QuicSentPacketManager::~QuicSentPacketManager() {
  for (UnackedPacketMap::iterator it = unacked_packets_.begin();
       it != unacked_packets_.end(); ++it) {
    delete it->second.retransmittable_frames;
  }
  while (!previous_transmissions_map_.empty()) {
    SequenceNumberSet* previous_transmissions =
        previous_transmissions_map_.begin()->second;
    for (SequenceNumberSet::const_iterator it = previous_transmissions->begin();
         it != previous_transmissions->end(); ++it) {
      DCHECK(ContainsKey(previous_transmissions_map_, *it));
      previous_transmissions_map_.erase(*it);
    }
    delete previous_transmissions;
  }
  STLDeleteValues(&packet_history_map_);
}

void QuicSentPacketManager::SetFromConfig(const QuicConfig& config) {
  if (config.initial_round_trip_time_us() > 0 &&
      rtt_sample_.IsInfinite()) {
    // The initial rtt should already be set on the client side.
    DVLOG_IF(1, !is_server_)
        << "Client did not set an initial RTT, but did negotiate one.";
    rtt_sample_ =
        QuicTime::Delta::FromMicroseconds(config.initial_round_trip_time_us());
  }
  if (config.congestion_control() == kPACE) {
    MaybeEnablePacing();
  }
  send_algorithm_->SetFromConfig(config, is_server_);
}

void QuicSentPacketManager::SetMaxPacketSize(QuicByteCount max_packet_size) {
  send_algorithm_->SetMaxPacketSize(max_packet_size);
}

void QuicSentPacketManager::OnSerializedPacket(
    const SerializedPacket& serialized_packet, QuicTime serialized_time) {
  if (serialized_packet.packet->is_fec_packet()) {
    DCHECK(!serialized_packet.retransmittable_frames);
    unacked_fec_packets_.insert(make_pair(
        serialized_packet.sequence_number, serialized_time));
    return;
  }

  if (serialized_packet.retransmittable_frames == NULL) {
    // Don't track ack/congestion feedback packets.
    return;
  }

  ack_notifier_manager_.OnSerializedPacket(serialized_packet);

  DCHECK(unacked_packets_.empty() ||
         unacked_packets_.rbegin()->first < serialized_packet.sequence_number);
  unacked_packets_[serialized_packet.sequence_number] =
      TransmissionInfo(serialized_packet.retransmittable_frames,
                       serialized_packet.sequence_number_length);
}

void QuicSentPacketManager::OnRetransmittedPacket(
    QuicPacketSequenceNumber old_sequence_number,
    QuicPacketSequenceNumber new_sequence_number) {
  DCHECK(ContainsKey(unacked_packets_, old_sequence_number));
  DCHECK(ContainsKey(pending_retransmissions_, old_sequence_number));
  DCHECK(unacked_packets_.empty() ||
         unacked_packets_.rbegin()->first < new_sequence_number);

  pending_retransmissions_.erase(old_sequence_number);

  RetransmittableFrames* frames =
      unacked_packets_[old_sequence_number].retransmittable_frames;
  DCHECK(frames);

  // A notifier may be waiting to hear about ACKs for the original sequence
  // number. Inform them that the sequence number has changed.
  ack_notifier_manager_.UpdateSequenceNumber(old_sequence_number,
                                             new_sequence_number);

  // We keep the old packet in the unacked packet list until it, or one of
  // the retransmissions of it are acked.
  unacked_packets_[old_sequence_number].retransmittable_frames = NULL;
  unacked_packets_[new_sequence_number] =
      TransmissionInfo(frames, GetSequenceNumberLength(old_sequence_number));

  // Keep track of all sequence numbers that this packet
  // has been transmitted as.
  SequenceNumberSet* previous_transmissions;
  PreviousTransmissionMap::iterator it =
      previous_transmissions_map_.find(old_sequence_number);
  if (it == previous_transmissions_map_.end()) {
    // This is the first retransmission of this packet, so create a new entry.
    previous_transmissions = new SequenceNumberSet;
    previous_transmissions_map_[old_sequence_number] = previous_transmissions;
    previous_transmissions->insert(old_sequence_number);
  } else {
    // Otherwise, use the existing entry.
    previous_transmissions = it->second;
  }
  previous_transmissions->insert(new_sequence_number);
  previous_transmissions_map_[new_sequence_number] = previous_transmissions;

  DCHECK(HasRetransmittableFrames(new_sequence_number));
}

void QuicSentPacketManager::OnPacketAcked(
    const ReceivedPacketInfo& received_info) {
  HandleAckForSentPackets(received_info);
  HandleAckForSentFecPackets(received_info);
}

void QuicSentPacketManager::DiscardUnackedPacket(
    QuicPacketSequenceNumber sequence_number) {
  MarkPacketReceivedByPeer(sequence_number);
}

void QuicSentPacketManager::HandleAckForSentPackets(
    const ReceivedPacketInfo& received_info) {
  // Go through the packets we have not received an ack for and see if this
  // incoming_ack shows they've been seen by the peer.
  UnackedPacketMap::iterator it = unacked_packets_.begin();
  while (it != unacked_packets_.end()) {
    QuicPacketSequenceNumber sequence_number = it->first;
    if (sequence_number > received_info.largest_observed) {
      // These are very new sequence_numbers.
      break;
    }

    if (IsAwaitingPacket(received_info, sequence_number)) {
      ++it;
      continue;
    }

    // Packet was acked, so remove it from our unacked packet list.
    DVLOG(1) << ENDPOINT <<"Got an ack for packet " << sequence_number;
    // If data is associated with the most recent transmission of this
    // packet, then inform the caller.
    it = MarkPacketReceivedByPeer(sequence_number);

    // The AckNotifierManager is informed of every ACKed sequence number.
    ack_notifier_manager_.OnPacketAcked(sequence_number);
  }

  // If we have received a trunacted ack, then we need to
  // clear out some previous transmissions to allow the peer
  // to actually ACK new packets.
  if (received_info.is_truncated) {
    size_t num_to_clear = received_info.missing_packets.size() / 2;
    UnackedPacketMap::iterator it = unacked_packets_.begin();
    while (it != unacked_packets_.end() && num_to_clear > 0) {
      QuicPacketSequenceNumber sequence_number = it->first;
      ++it;
      // If this is not a previous transmission then there is no point
      // in clearing out any further packets, because it will not affect
      // the high water mark.
      if (!IsPreviousTransmission(sequence_number)) {
        break;
      }

      DCHECK(ContainsKey(previous_transmissions_map_, sequence_number));
      DCHECK(!HasRetransmittableFrames(sequence_number));
      unacked_packets_.erase(sequence_number);
      SequenceNumberSet* previous_transmissions =
          previous_transmissions_map_[sequence_number];
      previous_transmissions_map_.erase(sequence_number);
      previous_transmissions->erase(sequence_number);
      if (previous_transmissions->size() == 1) {
        previous_transmissions_map_.erase(*previous_transmissions->begin());
        delete previous_transmissions;
      }
      --num_to_clear;
    }
  }
}

bool QuicSentPacketManager::HasRetransmittableFrames(
    QuicPacketSequenceNumber sequence_number) const {
  if (!ContainsKey(unacked_packets_, sequence_number)) {
    return false;
  }

  return unacked_packets_.find(
      sequence_number)->second.retransmittable_frames != NULL;
}

bool QuicSentPacketManager::MarkForRetransmission(
    QuicPacketSequenceNumber sequence_number,
    TransmissionType transmission_type) {
  DCHECK(ContainsKey(unacked_packets_, sequence_number));
  if (!HasRetransmittableFrames(sequence_number)) {
    return false;
  }
  // If it's already in the retransmission map, don't add it again, just let
  // the prior retransmission request win out.
  if (ContainsKey(pending_retransmissions_, sequence_number)) {
    return true;
  }

  pending_retransmissions_[sequence_number] = transmission_type;
  return true;
}

bool QuicSentPacketManager::HasPendingRetransmissions() const {
  return !pending_retransmissions_.empty();
}

QuicSentPacketManager::PendingRetransmission
    QuicSentPacketManager::NextPendingRetransmission() {
  DCHECK(!pending_retransmissions_.empty());
  QuicPacketSequenceNumber sequence_number =
      pending_retransmissions_.begin()->first;
  DCHECK(ContainsKey(unacked_packets_, sequence_number));
  DCHECK(unacked_packets_[sequence_number].retransmittable_frames);

  return PendingRetransmission(sequence_number,
                               pending_retransmissions_.begin()->second,
                               GetRetransmittableFrames(sequence_number),
                               GetSequenceNumberLength(sequence_number));
}

bool QuicSentPacketManager::IsPreviousTransmission(
    QuicPacketSequenceNumber sequence_number) const {
  DCHECK(ContainsKey(unacked_packets_, sequence_number));

  PreviousTransmissionMap::const_iterator it =
      previous_transmissions_map_.find(sequence_number);
  if (it == previous_transmissions_map_.end()) {
    return false;
  }

  SequenceNumberSet* previous_transmissions = it->second;
  DCHECK(!previous_transmissions->empty());
  return *previous_transmissions->rbegin() != sequence_number;
}

QuicPacketSequenceNumber QuicSentPacketManager::GetMostRecentTransmission(
    QuicPacketSequenceNumber sequence_number) const {
  DCHECK(ContainsKey(unacked_packets_, sequence_number));

  PreviousTransmissionMap::const_iterator it =
      previous_transmissions_map_.find(sequence_number);
  if (it == previous_transmissions_map_.end()) {
    return sequence_number;
  }

  SequenceNumberSet* previous_transmissions =
      previous_transmissions_map_.find(sequence_number)->second;
  DCHECK(!previous_transmissions->empty());
  return *previous_transmissions->rbegin();
}

QuicSentPacketManager::UnackedPacketMap::iterator
QuicSentPacketManager::MarkPacketReceivedByPeer(
    QuicPacketSequenceNumber sequence_number) {
  DCHECK(ContainsKey(unacked_packets_, sequence_number));
  UnackedPacketMap::iterator next_unacked =
      unacked_packets_.find(sequence_number);
  ++next_unacked;

  // If this packet has never been retransmitted, then simply drop it.
  if (!ContainsKey(previous_transmissions_map_, sequence_number)) {
    DiscardPacket(sequence_number);
    return next_unacked;
  }

  SequenceNumberSet* previous_transmissions =
    previous_transmissions_map_.find(sequence_number)->second;
  SequenceNumberSet::reverse_iterator previous_transmissions_it
      = previous_transmissions->rbegin();
  DCHECK(previous_transmissions_it != previous_transmissions->rend());

  QuicPacketSequenceNumber new_packet = *previous_transmissions_it;
  bool is_new_packet = new_packet == sequence_number;
  if (is_new_packet) {
    DiscardPacket(new_packet);
  } else {
    if (next_unacked->first == new_packet) {
      ++next_unacked;
    }
    // If we have received an ack for a previous transmission of a packet,
    // we want to keep the "new" transmission of the packet unacked,
    // but prevent the data from being retransmitted.
    delete unacked_packets_[new_packet].retransmittable_frames;
    unacked_packets_[new_packet].retransmittable_frames = NULL;
    pending_retransmissions_.erase(new_packet);
  }
  previous_transmissions_map_.erase(new_packet);

  // Clear out information all previous transmissions.
  ++previous_transmissions_it;
  while (previous_transmissions_it != previous_transmissions->rend()) {
    QuicPacketSequenceNumber previous_transmission = *previous_transmissions_it;
    ++previous_transmissions_it;
    DCHECK(ContainsKey(previous_transmissions_map_, previous_transmission));
    previous_transmissions_map_.erase(previous_transmission);
    if (next_unacked != unacked_packets_.end() &&
        next_unacked->first == previous_transmission) {
      ++next_unacked;
    }
    DiscardPacket(previous_transmission);
  }

  delete previous_transmissions;

  next_unacked = unacked_packets_.begin();
  while (next_unacked != unacked_packets_.end() &&
         next_unacked->first < sequence_number) {
        ++next_unacked;
      }
  return next_unacked;
}

void QuicSentPacketManager::DiscardPacket(
    QuicPacketSequenceNumber sequence_number) {
  UnackedPacketMap::iterator unacked_it =
      unacked_packets_.find(sequence_number);
  // Packet was not meant to be retransmitted.
  if (unacked_it == unacked_packets_.end()) {
    return;
  }

  // Delete the retransmittable frames.
  delete unacked_it->second.retransmittable_frames;
  unacked_packets_.erase(unacked_it);
  pending_retransmissions_.erase(sequence_number);
  return;
}

void QuicSentPacketManager::HandleAckForSentFecPackets(
    const ReceivedPacketInfo& received_info) {
  UnackedFecPacketMap::iterator it = unacked_fec_packets_.begin();
  while (it != unacked_fec_packets_.end()) {
    QuicPacketSequenceNumber sequence_number = it->first;
    if (sequence_number > received_info.largest_observed) {
      break;
    }

    if (!IsAwaitingPacket(received_info, sequence_number)) {
      DVLOG(1) << ENDPOINT << "Got an ack for fec packet: " << sequence_number;
      unacked_fec_packets_.erase(it++);
    } else {
      // TODO(rch): treat these packets more consistently.  They should
      // be subject to NACK and RTO based loss.  (Thought obviously, they
      // should not be retransmitted.)
      DVLOG(1) << ENDPOINT << "Still missing ack for fec packet: "
               << sequence_number;
      ++it;
    }
  }
}

void QuicSentPacketManager::DiscardFecPacket(
    QuicPacketSequenceNumber sequence_number) {
  DCHECK(ContainsKey(unacked_fec_packets_, sequence_number));
  unacked_fec_packets_.erase(sequence_number);
}

bool QuicSentPacketManager::IsRetransmission(
    QuicPacketSequenceNumber sequence_number) const {
  DCHECK(HasRetransmittableFrames(sequence_number));
  return HasRetransmittableFrames(sequence_number) &&
      ContainsKey(previous_transmissions_map_, sequence_number);
}

bool QuicSentPacketManager::IsUnacked(
    QuicPacketSequenceNumber sequence_number) const {
  return ContainsKey(unacked_packets_, sequence_number);
}

bool QuicSentPacketManager::IsFecUnacked(
    QuicPacketSequenceNumber sequence_number) const {
  return ContainsKey(unacked_fec_packets_, sequence_number);
}

const RetransmittableFrames& QuicSentPacketManager::GetRetransmittableFrames(
    QuicPacketSequenceNumber sequence_number) const {
  DCHECK(ContainsKey(unacked_packets_, sequence_number));

  return *unacked_packets_.find(sequence_number)->second.retransmittable_frames;
}

QuicSequenceNumberLength QuicSentPacketManager::GetSequenceNumberLength(
    QuicPacketSequenceNumber sequence_number) const {
  DCHECK(ContainsKey(unacked_packets_, sequence_number));

  return unacked_packets_.find(sequence_number)->second.sequence_number_length;
}

QuicTime QuicSentPacketManager::GetFecSentTime(
    QuicPacketSequenceNumber sequence_number) const {
  DCHECK(ContainsKey(unacked_fec_packets_, sequence_number));

  return unacked_fec_packets_.find(sequence_number)->second;
}

bool QuicSentPacketManager::HasUnackedPackets() const {
  return !unacked_packets_.empty();
}

size_t QuicSentPacketManager::GetNumRetransmittablePackets() const {
  size_t num_unacked_packets = 0;
  for (UnackedPacketMap::const_iterator it = unacked_packets_.begin();
       it != unacked_packets_.end(); ++it) {
    QuicPacketSequenceNumber sequence_number = it->first;
    if (HasRetransmittableFrames(sequence_number)) {
      ++num_unacked_packets;
    }
  }
  return num_unacked_packets;
}

bool QuicSentPacketManager::HasUnackedFecPackets() const {
  return !unacked_fec_packets_.empty();
}

QuicPacketSequenceNumber
QuicSentPacketManager::GetLeastUnackedSentPacket() const {
  if (unacked_packets_.empty()) {
    // If there are no unacked packets, set the least unacked packet to
    // the sequence number of the next packet sent.
    return helper_->GetNextPacketSequenceNumber();
  }

  return unacked_packets_.begin()->first;
}

QuicPacketSequenceNumber
QuicSentPacketManager::GetLeastUnackedFecPacket() const {
  if (unacked_fec_packets_.empty()) {
    // If there are no unacked packets, set the least unacked packet to
    // the sequence number of the next packet sent.
    return helper_->GetNextPacketSequenceNumber();
  }

  return unacked_fec_packets_.begin()->first;
}

SequenceNumberSet QuicSentPacketManager::GetUnackedPackets() const {
  SequenceNumberSet unacked_packets;
  for (UnackedPacketMap::const_iterator it = unacked_packets_.begin();
       it != unacked_packets_.end(); ++it) {
    unacked_packets.insert(it->first);
  }
  return unacked_packets;
}

void QuicSentPacketManager::OnPacketSent(
    QuicPacketSequenceNumber sequence_number,
    QuicTime sent_time,
    QuicByteCount bytes,
    TransmissionType transmission_type,
    HasRetransmittableData has_retransmittable_data) {
  DCHECK_LT(0u, sequence_number);
  DCHECK(!ContainsKey(pending_packets_, sequence_number));

  // Only track packets the send algorithm wants us to track.
  if (!send_algorithm_->OnPacketSent(sent_time, sequence_number, bytes,
                                     transmission_type,
                                     has_retransmittable_data)) {
    return;
  }
  packet_history_map_[sequence_number] = new SendAlgorithmInterface::SentPacket(
      bytes, sent_time, has_retransmittable_data);
  pending_packets_.insert(sequence_number);
  CleanupPacketHistory();
}

void QuicSentPacketManager::OnRetransmissionTimeout() {
  ++consecutive_rto_count_;
  send_algorithm_->OnRetransmissionTimeout();
  // Abandon all pending packets to ensure the congestion window
  // opens up before we attempt to retransmit packets.
  for (SequenceNumberSet::const_iterator it = pending_packets_.begin();
       it != pending_packets_.end(); ++it) {
    QuicPacketSequenceNumber sequence_number = *it;
    DCHECK(ContainsKey(packet_history_map_, sequence_number));
    send_algorithm_->OnPacketAbandoned(
        sequence_number, packet_history_map_[sequence_number]->bytes_sent());
  }
  pending_packets_.clear();
}

void QuicSentPacketManager::OnLeastUnackedIncreased() {
  consecutive_rto_count_ = 0;
}

void QuicSentPacketManager::OnPacketAbandoned(
    QuicPacketSequenceNumber sequence_number) {
  SequenceNumberSet::iterator it = pending_packets_.find(sequence_number);
  if (it != pending_packets_.end()) {
    DCHECK(ContainsKey(packet_history_map_, sequence_number));
    send_algorithm_->OnPacketAbandoned(
        sequence_number, packet_history_map_[sequence_number]->bytes_sent());
    pending_packets_.erase(it);
  }
}

void QuicSentPacketManager::OnIncomingQuicCongestionFeedbackFrame(
    const QuicCongestionFeedbackFrame& frame, QuicTime feedback_receive_time) {
  send_algorithm_->OnIncomingQuicCongestionFeedbackFrame(
      frame, feedback_receive_time, packet_history_map_);
}

SequenceNumberSet QuicSentPacketManager::OnIncomingAckFrame(
    const QuicAckFrame& frame,
    QuicTime ack_receive_time) {
  // We calculate the RTT based on the highest ACKed sequence number, the lower
  // sequence numbers will include the ACK aggregation delay.
  SendAlgorithmInterface::SentPacketsMap::iterator history_it =
      packet_history_map_.find(frame.received_info.largest_observed);
  // TODO(satyamshekhar): largest_observed might be missing.
  if (history_it != packet_history_map_.end()) {
    QuicTime::Delta send_delta = ack_receive_time.Subtract(
        history_it->second->send_timestamp());
    if (send_delta > frame.received_info.delta_time_largest_observed) {
      rtt_sample_ = send_delta.Subtract(
          frame.received_info.delta_time_largest_observed);
    } else if (rtt_sample_.IsInfinite()) {
      // Even though we received information from the peer suggesting
      // an invalid (negative) RTT, we can use the send delta as an
      // approximation until we get a better estimate.
      rtt_sample_ = send_delta;
    }
  }
  // We want to.
  // * Get all packets lower(including) than largest_observed
  //   from pending_packets_.
  // * Remove all missing packets.
  // * Send each ACK in the list to send_algorithm_.
  SequenceNumberSet::iterator it = pending_packets_.begin();
  SequenceNumberSet::iterator it_upper =
      pending_packets_.upper_bound(frame.received_info.largest_observed);

  SequenceNumberSet retransmission_packets;
  SequenceNumberSet lost_packets;
  while (it != it_upper) {
    QuicPacketSequenceNumber sequence_number = *it;
    if (!IsAwaitingPacket(frame.received_info, sequence_number)) {
      // Not missing, hence implicitly acked.
      size_t bytes_sent = packet_history_map_[sequence_number]->bytes_sent();
      send_algorithm_->OnPacketAcked(sequence_number, bytes_sent, rtt_sample_);
      pending_packets_.erase(it++);  // Must be incremented post to work.
      continue;
    }

    // The peer got packets after this sequence number.  This is an explicit
    // nack.
    DVLOG(1) << "still missing packet " << sequence_number;
    DCHECK(ContainsKey(packet_history_map_, sequence_number));
    const SendAlgorithmInterface::SentPacket* sent_packet =
        packet_history_map_[sequence_number];
    // Consider it multiple nacks when there is a gap between the missing packet
    // and the largest observed, since the purpose of a nack threshold is to
    // tolerate re-ordering.  This handles both StretchAcks and Forward Acks.
    // TODO(ianswett): This relies heavily on sequential reception of packets,
    // and makes an assumption that the congestion control uses TCP style nacks.
    size_t min_nacks = frame.received_info.largest_observed - sequence_number;
    packet_history_map_[sequence_number]->Nack(min_nacks);

    size_t num_nacks_needed = kNumberOfNacksBeforeRetransmission;
    // Check for early retransmit(RFC5827) when the last packet gets acked and
    // the there are fewer than 4 pending packets.
    if (pending_packets_.size() <= kNumberOfNacksBeforeRetransmission &&
        sent_packet->has_retransmittable_data() == HAS_RETRANSMITTABLE_DATA &&
        *pending_packets_.rbegin() == frame.received_info.largest_observed) {
      num_nacks_needed = frame.received_info.largest_observed - sequence_number;
    }

    if (sent_packet->nack_count() < num_nacks_needed) {
      ++it;
      continue;
    }

    // If the number of retransmissions has maxed out, don't lose or retransmit
    // any more packets.
    if (retransmission_packets.size() >= kMaxRetransmissionsPerAck) {
      ++it;
      continue;
    }

    lost_packets.insert(sequence_number);
    if (sent_packet->has_retransmittable_data() == HAS_RETRANSMITTABLE_DATA) {
      retransmission_packets.insert(sequence_number);
    }

    ++it;
  }
  // Abandon packets after the loop over pending packets, because otherwise it
  // changes the early retransmit logic and iteration.
  for (SequenceNumberSet::const_iterator it = lost_packets.begin();
       it != lost_packets.end(); ++it) {
    // TODO(ianswett): OnPacketLost is also called from TCPCubicSender when
    // an FEC packet is lost, but FEC loss information should be shared among
    // congestion managers.  Additionally, if it's expected the FEC packet may
    // repair the loss, it should be recorded as a loss to the congestion
    // manager, but not retransmitted until it's known whether the FEC packet
    // arrived.
    send_algorithm_->OnPacketLost(*it, ack_receive_time);
    OnPacketAbandoned(*it);
  }

  return retransmission_packets;
}

QuicTime::Delta QuicSentPacketManager::TimeUntilSend(
    QuicTime now,
    TransmissionType transmission_type,
    HasRetransmittableData retransmittable,
    IsHandshake handshake) {
  return send_algorithm_->TimeUntilSend(now, transmission_type, retransmittable,
                                        handshake);
}

const QuicTime::Delta QuicSentPacketManager::DefaultRetransmissionTime() {
  return QuicTime::Delta::FromMilliseconds(kDefaultRetransmissionTimeMs);
}

// Ensures that the Delayed Ack timer is always set to a value lesser
// than the retransmission timer's minimum value (MinRTO). We want the
// delayed ack to get back to the QUIC peer before the sender's
// retransmission timer triggers.  Since we do not know the
// reverse-path one-way delay, we assume equal delays for forward and
// reverse paths, and ensure that the timer is set to less than half
// of the MinRTO.
// There may be a value in making this delay adaptive with the help of
// the sender and a signaling mechanism -- if the sender uses a
// different MinRTO, we may get spurious retransmissions. May not have
// any benefits, but if the delayed ack becomes a significant source
// of (likely, tail) latency, then consider such a mechanism.

const QuicTime::Delta QuicSentPacketManager::DelayedAckTime() {
  return QuicTime::Delta::FromMilliseconds(kMinRetransmissionTimeMs/2);
}

const QuicTime::Delta QuicSentPacketManager::GetRetransmissionDelay() const {
  size_t number_retransmissions = consecutive_rto_count_;
  if (FLAGS_limit_rto_increase_for_tests) {
    const size_t kTailDropWindowSize = 5;
    const size_t kTailDropMaxRetransmissions = 4;
    if (pending_packets_.size() <= kTailDropWindowSize) {
      // Avoid exponential backoff of RTO when there are only a few packets
      // outstanding.  This helps avoid the situation where fake packet loss
      // causes a packet and it's retransmission to be dropped causing
      // test timouts.
      if (number_retransmissions <= kTailDropMaxRetransmissions) {
        number_retransmissions = 0;
      } else {
        number_retransmissions -= kTailDropMaxRetransmissions;
      }
    }
  }

  QuicTime::Delta retransmission_delay = send_algorithm_->RetransmissionDelay();
  if (retransmission_delay.IsZero()) {
    // We are in the initial state, use default timeout values.
    retransmission_delay =
        QuicTime::Delta::FromMilliseconds(kDefaultRetransmissionTimeMs);
  }
  // Calculate exponential back off.
  retransmission_delay = QuicTime::Delta::FromMilliseconds(
      retransmission_delay.ToMilliseconds() * static_cast<size_t>(
          (1 << min<size_t>(number_retransmissions, kMaxRetransmissions))));

  // TODO(rch): This code should move to |send_algorithm_|.
  if (retransmission_delay.ToMilliseconds() < kMinRetransmissionTimeMs) {
    return QuicTime::Delta::FromMilliseconds(kMinRetransmissionTimeMs);
  }
  if (retransmission_delay.ToMilliseconds() > kMaxRetransmissionTimeMs) {
    return QuicTime::Delta::FromMilliseconds(kMaxRetransmissionTimeMs);
  }
  return retransmission_delay;
}

const QuicTime::Delta QuicSentPacketManager::SmoothedRtt() const {
  return send_algorithm_->SmoothedRtt();
}

QuicBandwidth QuicSentPacketManager::BandwidthEstimate() const {
  return send_algorithm_->BandwidthEstimate();
}

QuicByteCount QuicSentPacketManager::GetCongestionWindow() const {
  return send_algorithm_->GetCongestionWindow();
}

void QuicSentPacketManager::SetCongestionWindow(QuicByteCount window) {
  send_algorithm_->SetCongestionWindow(window);
}

void QuicSentPacketManager::CleanupPacketHistory() {
  const QuicTime::Delta kHistoryPeriod =
      QuicTime::Delta::FromMilliseconds(kHistoryPeriodMs);
  QuicTime now = clock_->ApproximateNow();

  SendAlgorithmInterface::SentPacketsMap::iterator history_it =
      packet_history_map_.begin();
  for (; history_it != packet_history_map_.end(); ++history_it) {
    if (now.Subtract(history_it->second->send_timestamp()) <= kHistoryPeriod) {
      return;
    }
    // Don't remove packets which have not been acked.
    if (ContainsKey(pending_packets_, history_it->first)) {
      continue;
    }
    delete history_it->second;
    packet_history_map_.erase(history_it);
    history_it = packet_history_map_.begin();
  }
}

void QuicSentPacketManager::MaybeEnablePacing() {
  if (!FLAGS_enable_quic_pacing) {
    return;
  }

  if (using_pacing_) {
    return;
  }

  using_pacing_ = true;
  send_algorithm_.reset(
      new PacingSender(send_algorithm_.release(),
                       QuicTime::Delta::FromMicroseconds(1)));
}

}  // namespace net
