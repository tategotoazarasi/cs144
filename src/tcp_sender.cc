#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <random>

using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  : isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) )
  , initial_RTO_ms_( initial_RTO_ms )
  , zero_point( isn_ )
{
  timer.rto = initial_RTO_ms;
}

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
  timer.run();
  return frame.msg;
}

void TCPSender::push( Reader& outbound_stream )
{
  if ( fin_sent ) {
    return;
  }
  uint64_t const ws = window_size > 0 ? window_size : 1;
  string str;
  read( outbound_stream, min( TCPConfig::MAX_PAYLOAD_SIZE, ws - sequence_numbers_in_flight() ), str );
  TCPSenderMessage sm;
  if ( str.empty() && sync_sent ) {
    if ( outbound_stream.is_finished() && sequence_numbers_in_flight() + 1 <= ws ) {
      sm = TCPSenderMessage { isn_, !sync_sent, Buffer {}, true };
      fin_sent = true;
    } else {
      return;
    }
  } else {
    sm = TCPSenderMessage { isn_,
                            !sync_sent,
                            ( str.length() > 0 ) ? ( Buffer { string( str.begin(), str.end() ) } ) : Buffer {},
                            ( str.size() + 1 ) <= ( ws - sequence_numbers_in_flight() )
                              && outbound_stream.is_finished() };
    fin_sent = sm.FIN;
  }
  if ( !sync_sent ) {
    sync_sent = true;
  }
  isn_ = isn_ + sm.sequence_length();
  checkpoint += sm.sequence_length();
  in_flight_cnt += sm.sequence_length();
  segments_to_sent.push( Frame { checkpoint, sm, window_size == 0 } );
  if ( !outbound_stream.peek().empty() && ws - sequence_numbers_in_flight() > 0 ) {
    push( outbound_stream );
  }
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  if ( segments_outstanding.empty() ) {
    return TCPSenderMessage { isn_, false, {}, false };
  }
  Frame latest_frame = segments_outstanding.front();
  for ( const auto& frame : segments_outstanding ) {
    if ( frame.checkpoint > latest_frame.checkpoint ) {
      latest_frame = frame;
    }
  }
  return TCPSenderMessage { latest_frame.msg.seqno + latest_frame.msg.sequence_length(), false, {}, false };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  window_size = msg.window_size;
  uint64_t const ack_no = msg.ackno->unwrap( zero_point, checkpoint );
  if ( msg.ackno.has_value() && ack_no > max_checkpoint_in_flight() ) {
    return;
  }
  for ( auto it = segments_outstanding.begin(); it != segments_outstanding.end(); ) {
    if ( it->checkpoint <= ack_no ) {
      if ( it->msg.SYN ) {
        sync_sent = true;
      }
      timer.rto = initial_RTO_ms_;
      timer.restart();
      in_flight_cnt -= it->msg.sequence_length();
      it = segments_outstanding.erase( it );
      retransmission_cnt = 0;
    } else {
      ++it;
    }
  }
}

void TCPSender::tick( const size_t ms_since_last_tick )
{
  if ( segments_outstanding.empty() ) {
    timer.shutdown();
    return;
  }
  timer.elapse( ms_since_last_tick );
  if ( timer.expired() ) {
    auto frame = segments_outstanding.begin();
    for ( auto it = segments_outstanding.begin(); it != segments_outstanding.end(); ++it ) {
      if ( it->checkpoint < frame->checkpoint ) {
        frame = it;
      }
    }
    segments_to_sent.push( *frame );
    if ( !frame->dont_back_off_rto ) {
      timer.rto *= 2;
    }
    segments_outstanding.erase( frame );
    retransmission_cnt++;
    timer.restart();
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

void RetransmissionTimer::elapse( uint64_t time )
{
  if ( running ) {
    this->now += time;
  }
}

bool RetransmissionTimer::expired() const
{
  if ( running ) {
    return this->now >= this->rto;
  }
  return false;
}

void RetransmissionTimer::run()
{
  if ( !running ) {
    running = true;
    this->now = 0;
  }
}

void RetransmissionTimer::shutdown()
{
  running = false;
}

void RetransmissionTimer::restart()
{
  running = true;
  this->now = 0;
}