#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <random>

using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  : isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) )
  , initial_RTO_ms_( initial_RTO_ms )
  , current_RTO_ms_( initial_RTO_ms )
  , zero_point( isn_ )
{}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return in_flight_cnt;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return retransmission_cnt;
}

optional<TCPSenderMessage> TCPSender::maybe_send()
{
  if ( segments_to_sent.empty() ) {
    return {};
  }
  auto frame = segments_to_sent.top();
  segments_to_sent.pop();
  segments_outstanding.push_back( frame );
  return frame.msg;
}

void TCPSender::push( Reader& outbound_stream )
{
  string str;
  read( outbound_stream, window_size - sequence_numbers_in_flight(), str );
  TCPSenderMessage sm;
  if ( str.empty() && sync_sent ) {
    if ( outbound_stream.is_finished() && sequence_numbers_in_flight() + 1 <= window_size ) {
      sm = TCPSenderMessage { isn_, !sync_sent, Buffer {}, true };
    } else {
      return;
    }
  } else {
    sm = TCPSenderMessage { isn_,
                            !sync_sent,
                            ( str.length() > 0 ) ? ( Buffer { string( str.begin(), str.end() ) } ) : Buffer {},
                            outbound_stream.is_finished() };
  }
  if ( !sync_sent ) {
    sync_sent = true;
  }
  isn_ = isn_ + sm.sequence_length();
  checkpoint += sm.sequence_length();
  in_flight_cnt += sm.sequence_length();
  segments_to_sent.push( Frame { checkpoint, now, now + current_RTO_ms_, sm } );
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  return TCPSenderMessage { segments_outstanding.empty() ? isn_
                                                         : segments_outstanding.back().msg.seqno
                                                             + segments_outstanding.back().msg.sequence_length(),
                            false,
                            {},
                            false };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  window_size = msg.window_size;
  if ( msg.ackno.has_value() && msg.ackno.value().unwrap( zero_point, checkpoint ) > max_checkpoint_in_flight() ) {
    return;
  }
  while ( !segments_outstanding.empty()
          && segments_outstanding.front().checkpoint <= msg.ackno->unwrap( zero_point, checkpoint ) ) {
    if ( segments_outstanding.front().msg.SYN ) {
      sync_sent = true;
    }
    current_RTO_ms_ = initial_RTO_ms_;
    in_flight_cnt -= segments_outstanding.front().msg.sequence_length();
    segments_outstanding.pop_front();
    retransmission_cnt = 0;
  }
}

void TCPSender::tick( const size_t ms_since_last_tick )
{
  now += ms_since_last_tick;
  for ( auto it = segments_outstanding.begin(); it != segments_outstanding.end(); ) {
    if ( now >= it->expire_time ) {
      current_RTO_ms_ *= 2;
      it->time = now;
      it->expire_time = now + current_RTO_ms_;
      segments_to_sent.push( *it );
      it = segments_outstanding.erase( it );
      retransmission_cnt++;
    } else {
      ++it;
    }
  }
}

uint64_t TCPSender::max_checkpoint_in_flight() const
{
  uint64_t max_checkpoint = 0;
  for ( const auto& it : segments_outstanding ) {
    max_checkpoint = max( max_checkpoint, it.checkpoint );
  }
  return max_checkpoint;
}