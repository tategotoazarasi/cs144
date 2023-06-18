#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream )
{
  if ( !syn_received && message.SYN ) {
    zero_point = message.seqno;
    syn_received = true;
  }
  if ( syn_received ) {
    reassembler.insert(
      message.seqno.unwrap( Wrap32::wrap( message.SYN ? 0 : 1, zero_point ), inbound_stream.bytes_pushed() ),
      message.payload,
      message.FIN,
      inbound_stream );
  }
}

TCPReceiverMessage TCPReceiver::send( const Writer& inbound_stream ) const
{
  TCPReceiverMessage rm;
  rm.window_size = static_cast<uint16_t>(
    inbound_stream.available_capacity() > UINT16_MAX ? UINT16_MAX : inbound_stream.available_capacity() );
  if ( syn_received ) {
    rm.ackno = Wrap32::wrap( inbound_stream.bytes_pushed() + ( inbound_stream.is_closed() ? 2 : 1 ), zero_point );
  }
  return rm;
}
