#include <sstream>
#include <stdexcept>

#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ), rest( capacity ) {}

void Writer::push( string data )
{
  uint64_t const len = min( data.size(), rest );
  buff += data.substr( 0, len );
  rest -= len;
  pushed += len;
}

void Writer::close()
{
  closed = true;
}

void Writer::set_error()
{
  err = true;
}

bool Writer::is_closed() const
{
  return closed;
}

uint64_t Writer::available_capacity() const
{
  return rest;
}

uint64_t Writer::bytes_pushed() const
{
  return pushed;
}

string_view Reader::peek() const
{
  return buff;
}

bool Reader::is_finished() const
{
  return buff.empty() && closed;
}

bool Reader::has_error() const
{
  return err;
}

void Reader::pop( uint64_t len )
{
  len = min( len, bytes_buffered() );
  poped += len;
  rest += len;
  buff = buff.substr( len );
}

uint64_t Reader::bytes_buffered() const
{
  return capacity_ - rest;
}

uint64_t Reader::bytes_popped() const
{
  return poped;
}
