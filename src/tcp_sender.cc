#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <random>

using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  : isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) ), initial_RTO_ms_( initial_RTO_ms )
{}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  uint64_t res = 0;
  for ( auto const& sm : segments_outstanding ) {
    res += sm.sequence_length();
  }
  return res;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return {};
}

optional<TCPSenderMessage> TCPSender::maybe_send()
{
  if ( segments_to_sent.empty() ) {
    return {};
  }
  auto res = segments_to_sent.front();
  segments_to_sent.pop_front();
  segments_outstanding.push_back( res );
  return res;
}

void TCPSender::push( Reader& outbound_stream )
{
  string str;
  read( outbound_stream, window_size, str );
  if ( str.empty() && sync_sent ) {
    return;
  }
  TCPSenderMessage const sm = TCPSenderMessage {
    isn_, !sync_sent, ( str.length() > 0 ) ? ( Buffer { string( str.begin(), str.end() ) } ) : Buffer {}, false };
  if ( !sync_sent ) {
    sync_sent = true;
  }
  isn_ = isn_ + sm.sequence_length();
  segments_to_sent.push_back( sm );
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  return TCPSenderMessage { segments_outstanding.empty()
                              ? isn_
                              : segments_outstanding.back().seqno + segments_outstanding.back().sequence_length(),
                            false,
                            {},
                            false };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  window_size = msg.window_size;
  if ( !segments_outstanding.empty()
       && segments_outstanding.front().seqno + segments_outstanding.front().sequence_length() == msg.ackno ) {
    if ( segments_outstanding.front().SYN ) {
      sync_sent = true;
    }
    segments_outstanding.pop_front();
  }
}

void TCPSender::tick( const size_t ms_since_last_tick )
{
  // Your code here.
  (void)ms_since_last_tick;
}
