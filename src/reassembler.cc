#include "reassembler.hh"
#include <sstream>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{
  if ( is_last_substring ) {
    last_rcvd = is_last_substring;
  }
  for ( uint64_t i = 0; i < data.size(); i++ ) {
    if ( first_index + i < current_index || m.contains( first_index + i )
         || first_index + i - current_index + 1 > output.available_capacity() ) {
      continue;
    }
    m[first_index + i] = data[i];
    pending++;
  }
  ostringstream oss;

  while ( m.contains( current_index ) ) {
    oss << m[current_index];
    pending--;
    current_index++;
  }
  string const str = oss.str();
  if ( !str.empty() ) {
    output.push( str );
  }
  if ( last_rcvd && bytes_pending() == 0 ) {
    output.close();
  }
}

uint64_t Reassembler::bytes_pending() const
{
  return pending;
}
