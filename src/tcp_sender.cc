#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <random>

using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  : isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) )
  , initial_RTO_ms_( initial_RTO_ms )
  , zero_point( isn_ )
{}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  uint64_t res = 0;
  for ( auto const& [_, sm] : segments_outstanding ) {
    res += sm.sequence_length();
  }
  for ( auto const& [_, sm] : segments_to_sent ) {
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
  auto [index, res] = segments_to_sent.front();
  segments_to_sent.pop_front();
  segments_outstanding.push_back( make_pair( index, res ) );
  return res;
}

void TCPSender::push( Reader& outbound_stream )
{
  string str;
  read( outbound_stream, window_size - sequence_numbers_in_flight(), str );
  if ( str.empty() && sync_sent ) {
    return;
  }
  TCPSenderMessage const sm = TCPSenderMessage {
    isn_, !sync_sent, ( str.length() > 0 ) ? ( Buffer { string( str.begin(), str.end() ) } ) : Buffer {}, false };
  if ( !sync_sent ) {
    sync_sent = true;
  }
  isn_ = isn_ + sm.sequence_length();
  checkpoint += sm.sequence_length();
  segments_to_sent.push_back( make_pair( checkpoint, sm ) );
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  return TCPSenderMessage { segments_outstanding.empty() ? isn_
                                                         : segments_outstanding.back().second.seqno
                                                             + segments_outstanding.back().second.sequence_length(),
                            false,
                            {},
                            false };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  window_size = msg.window_size;
  while ( !segments_outstanding.empty()
          && segments_outstanding.front().first <= msg.ackno->unwrap( zero_point, checkpoint ) ) {
    if ( segments_outstanding.front().second.SYN ) {
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
