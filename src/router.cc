#include "router.hh"

#include <iostream>
#include <limits>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix_ to match the datagram's destination address against
// prefix_length_: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop_: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num_: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";
  routing_table_.emplace_back( route_prefix, prefix_length, next_hop, interface_num );
}

void Router::route()
{
  for ( auto& interface : interfaces_ ) {
    auto mc = interface.maybe_receive();
    if ( mc.has_value() ) {
      IPv4Datagram datagram = mc.value();
      if ( datagram.header.ttl <= 1 ) {
        continue;
      }
      datagram.header.ttl--;
      datagram.header.compute_checksum();
      MatchResult longest_match = match( datagram.header.dst );
      if ( longest_match.null_result ) {
        continue;
      }
      this->interface( longest_match.interface_num )
        .send_datagram( datagram,
                        longest_match.next_hop.has_value() ? longest_match.next_hop.value()
                                                           : Address::from_ipv4_numeric( datagram.header.dst ) );
    }
  }
}

MatchResult Router::match( uint32_t ip )
{
  auto longest_match = MatchResult { 0, {}, 0, true };
  for ( auto& item : routing_table_ ) {
    auto match_res = item.match( ip );
    if ( longest_match.null_result || ( match_res.prefix_length > longest_match.prefix_length ) ) {
      longest_match = match_res;
    }
  }
  return longest_match;
}

RoutingTableItem::RoutingTableItem( uint32_t prefix,
                                    uint8_t prefix_length,
                                    std::optional<Address> next_hop,
                                    size_t interface_num )
  : prefix_( prefix )
  , mask_( 0 )
  , prefix_length_( prefix_length )
  , next_hop_( next_hop )
  , interface_num_( interface_num )
{
  if ( prefix_length != 0 ) {
    for ( int i = 0; i < prefix_length; i++ ) {
      mask_ = mask_ << 1;
      mask_ += 1;
    }
    mask_ <<= ( 32U - prefix_length );
  }
}

MatchResult RoutingTableItem::match( uint32_t ip )
{
  if ( ( ip & mask_ ) == prefix_ ) {
    return MatchResult { prefix_length_, next_hop_, interface_num_ };
  }
  return MatchResult { 0, {}, 0, true };
}
