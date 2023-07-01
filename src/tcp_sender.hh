#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include <list>
#include <unordered_set>
#include <vector>

struct Frame
{
  uint64_t checkpoint {};
  uint64_t time {};
  uint64_t expire_time {};
  TCPSenderMessage msg;

  bool operator<( const Frame& rhs ) const { return this->checkpoint < rhs.checkpoint; }
  bool operator>( const Frame& rhs ) const { return this->checkpoint > rhs.checkpoint; }
};

class TCPSender
{
  Wrap32 isn_;
  uint64_t initial_RTO_ms_;
  uint64_t current_RTO_ms_;
  std::list<Frame> segments_outstanding = std::list<Frame>();
  std::priority_queue<Frame, std::vector<Frame>, std::greater<>> segments_to_sent
    = std::priority_queue<Frame, std::vector<Frame>, std::greater<>>();
  bool sync_sent = false;
  bool fin_sent = false;
  Wrap32 zero_point;
  uint64_t checkpoint = 0;
  uint16_t window_size = 1;
  uint64_t now = 0;
  uint64_t in_flight_cnt = 0;
  uint64_t retransmission_cnt = 0;

public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( uint64_t initial_RTO_ms, std::optional<Wrap32> fixed_isn );

  /* Push bytes from the outbound stream */
  void push( Reader& outbound_stream );

  /* Send a TCPSenderMessage if needed (or empty optional otherwise) */
  std::optional<TCPSenderMessage> maybe_send();

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage send_empty_message() const;

  /* Receive an act on a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called. */
  void tick( uint64_t ms_since_last_tick );

  /* Accessors for use in testing */
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?
  uint64_t max_checkpoint_in_flight() const;
};
