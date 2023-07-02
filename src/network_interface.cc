#include <cstddef>

#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

using namespace std;

// ethernet_address: Ethernet (what ARP calls "hardware") address of the interface
// ip_address: IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( const EthernetAddress& ethernet_address, const Address& ip_address )
  : ethernet_address_( ethernet_address ), ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

// dgram: the IPv4 datagram to be sent
// next_hop: the IP address of the interface to send it to (typically a router or default gateway, but
// may also be another host if directly connected to the same network as the destination)

// Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) by using the
// Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  if ( arp_table.contains( next_hop.ipv4_numeric() ) ) {
    frames_to_sent.push_back( generate_frame( dgram, next_hop ) );
  } else {
    arp_query( next_hop );
    frames_waiting_for_arp.push_back( pair<InternetDatagram, Address> { dgram, next_hop } );
  }
}

// frame: the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  if ( frame.header.dst == ETHERNET_BROADCAST || frame.header.dst == ethernet_address_ ) {
    switch ( frame.header.type ) {
      case EthernetHeader::TYPE_ARP: {
        auto msg = ARPMessage();
        if ( !parse( msg, frame.payload ) ) {
          return {};
        }
        if ( msg.sender_ethernet_address == ETHERNET_BROADCAST ) {
          break;
        }
        arp_table[msg.sender_ip_address] = pair<uint64_t, EthernetAddress> {
          now + static_cast<uint64_t>( 30 * 1000 ), msg.sender_ethernet_address };
        for ( auto it = frames_waiting_for_arp.begin(); it != frames_waiting_for_arp.end(); ) {
          if ( it->second.ipv4_numeric() == msg.sender_ip_address ) {
            frames_to_sent.push_back( generate_frame( it->first, it->second ) );
            it = frames_waiting_for_arp.erase( it );
          } else {
            ++it;
          };
        }
        if ( msg.opcode == ARPMessage::OPCODE_REQUEST && msg.target_ip_address == ip_address_.ipv4_numeric()
             && ( msg.target_ethernet_address == ethernet_address_
                  || msg.target_ethernet_address == ETHERNET_BROADCAST
                  || msg.target_ethernet_address == ARP_BROADCAST ) ) {
          arp_to_sent.push_back( EthernetFrame {
            EthernetHeader { msg.sender_ethernet_address, ethernet_address_, EthernetHeader::TYPE_ARP },
            serialize( ARPMessage { ARPMessage::TYPE_ETHERNET,
                                    EthernetHeader::TYPE_IPv4,
                                    sizeof( EthernetHeader::src ),
                                    sizeof( IPv4Header::src ),
                                    ARPMessage::OPCODE_REPLY,
                                    ethernet_address_,
                                    ip_address_.ipv4_numeric(),
                                    msg.sender_ethernet_address,
                                    msg.sender_ip_address } ) } );
        }
        break;
      }
      case EthernetHeader::TYPE_IPv4: {
        auto msg = IPv4Datagram();
        if ( parse( msg, frame.payload ) ) {
          return msg;
        }
      }
    }
  }
  return {};
}

// ms_since_last_tick: the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  now += ms_since_last_tick;
  for ( auto it = arp_table.begin(); it != arp_table.end(); ) {
    if ( it->second.first <= now ) {
      it = arp_table.erase( it );
    } else {
      ++it;
    }
  }
}

optional<EthernetFrame> NetworkInterface::maybe_send()
{
  optional<EthernetFrame> frame;
  if ( !arp_to_sent.empty() ) {
    frame = arp_to_sent.front();
    arp_to_sent.pop_front();
  } else if ( !frames_to_sent.empty() ) {
    frame = frames_to_sent.front();
    frames_to_sent.pop_front();
  } else {
    frame = {};
  }
  return frame;
}

void NetworkInterface::arp_query( const Address& addr )
{
  arp_to_sent.push_back(
    EthernetFrame { EthernetHeader { ETHERNET_BROADCAST, ethernet_address_, EthernetHeader::TYPE_ARP },
                    serialize( ARPMessage { ARPMessage::TYPE_ETHERNET,
                                            EthernetHeader::TYPE_IPv4,
                                            sizeof( EthernetHeader::src ),
                                            sizeof( IPv4Header::src ),
                                            ARPMessage::OPCODE_REQUEST,
                                            ethernet_address_,
                                            ip_address_.ipv4_numeric(),
                                            ARP_BROADCAST,
                                            addr.ipv4_numeric() } ) } );
}

EthernetFrame NetworkInterface::generate_frame( const InternetDatagram& dgram, const Address& next_hop )
{
  return EthernetFrame {
    EthernetHeader { arp_table[next_hop.ipv4_numeric()].second, ethernet_address_, EthernetHeader::TYPE_IPv4 },
    serialize( dgram ) };
}