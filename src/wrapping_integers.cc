#include "wrapping_integers.hh"
#include <array>

#define MYABS( a, b ) ( ( a ) > ( b ) ) ? ( ( a ) - ( b ) ) : ( ( b ) - ( a ) )

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  auto addr = static_cast<uint64_t>( zero_point.raw_value_ );
  addr += n;
  return Wrap32 { static_cast<uint32_t>( addr ) };
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  array<uint64_t, 3> options = {};
  size_t options_i = 0;
  auto zero64 = static_cast<uint64_t>( zero_point.raw_value_ );
  auto addr = static_cast<uint64_t>( raw_value_ ) - zero64;
  addr &= 0xFFFFFFFFULL;
  if ( ( checkpoint >> 32 ) <= UINT32_MAX - 1 ) {
    options[options_i++] = ( ( ( checkpoint >> 32 ) + 1 ) << 32 ) + addr;
  }
  options[options_i++] = ( checkpoint & ~0xFFFFFFFFULL ) + addr;
  if ( ( checkpoint >> 32 ) >= 1 ) {
    options[options_i++] = ( ( ( checkpoint >> 32 ) - 1 ) << 32 ) + addr;
  }
  size_t min_i = 0;
  for ( size_t i = 1; i < options_i; i++ ) {
    uint64_t const abs1 = MYABS( options[i], checkpoint );
    uint64_t const abs2 = MYABS( options[min_i], checkpoint );
    if ( abs1 < abs2 ) {
      min_i = i;
    }
  }
  return options[min_i];
}
