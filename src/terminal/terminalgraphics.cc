/*
    Mosh: the mobile shell
    Copyright 2012 Keith Winstein

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    In addition, as a special exception, the copyright holders give
    permission to link the code of portions of this program with the
    OpenSSL library under certain conditions as described in each
    individual source file, and distribute linked combinations including
    the two.

    You must obey the GNU General Public License in all respects for all
    of the code used other than OpenSSL. If you modify file(s) with this
    exception, you may extend this exception to your version of the
    file(s), but you are not obligated to do so. If you do not wish to do
    so, delete this exception statement from your version. If you delete
    this exception statement from all source files in the program, then
    also delete it here.
*/

#include <ctype.h>
#include <stdlib.h>

#include <map>

#include "terminalgraphics.h"

using namespace Terminal;

/* The number of bytes valid base64 decodes to. */
static size_t decoded_size( const std::string &base64 )
{
  size_t padding = 0;
  while ( padding < 2 && padding < base64.size() && base64[ base64.size() - 1 - padding ] == '=' ) {
    padding++;
  }
  return base64.size() / 4 * 3 - padding;
}

/* Whether a whole payload is base64 that decodes to whole bytes. */
static bool is_base64( const std::string &text )
{
  if ( text.size() % 4 != 0 ) {
    return false;
  }
  for ( size_t i = 0; i < text.size(); i++ ) {
    const char c = text[ i ];
    const bool padding = c == '=' && i + 2 >= text.size()
      && ( i + 1 == text.size() || text[ i + 1 ] == '=' );
    if ( !isalnum( static_cast<unsigned char>( c ) ) && c != '+' && c != '/' && !padding ) {
      return false;
    }
  }
  return true;
}

const Image *Images::get( uint32_t id ) const
{
  for ( const auto &image : images ) {
    if ( image->id == id ) {
      return image.get();
    }
  }
  return nullptr;
}

std::optional<ImagePlacement> Images::placement( uint32_t id ) const
{
  auto found = placements.find( id );
  if ( found == placements.end() ) {
    return std::nullopt;
  }
  return found->second;
}

void Images::place( uint32_t id, const std::optional<ImagePlacement> &placement )
{
  if ( !placement ) {
    placements.erase( id );
  } else if ( get( id ) ) {
    placements[ id ] = *placement;
  }
}

void Images::remove( uint32_t id )
{
  placements.erase( id );
  for ( auto i = images.begin(); i != images.end(); i++ ) {
    if ( (*i)->id == id ) {
      images.erase( i );
      return;
    }
  }
}

void Images::put( const Image &image )
{
  for ( auto i = images.begin(); i != images.end(); i++ ) {
    if ( (*i)->id == image.id ) {
      images.erase( i );
      break;
    }
  }
  images.push_back( shared::make_shared<const Image>( image ) );

  size_t bytes = 0;
  for ( const auto &stored : images ) {
    bytes += decoded_size( stored->base64 );
  }
  while ( bytes > max_bytes ) {
    bytes -= decoded_size( images.front()->base64 );
    remove( images.front()->id );
  }
}

typedef std::map<std::string, std::string> Keys;

static Keys parse_keys( const std::string &control )
{
  Keys keys;
  size_t start = 0;
  while ( start < control.size() ) {
    size_t end = control.find( ',', start );
    if ( end == std::string::npos ) {
      end = control.size();
    }
    std::string pair = control.substr( start, end - start );
    size_t equals = pair.find( '=' );
    if ( equals != std::string::npos ) {
      keys[ pair.substr( 0, equals ) ] = pair.substr( equals + 1 );
    }
    start = end + 1;
  }
  return keys;
}

static ImagePlacement placement_from( Keys &keys )
{
  ImagePlacement placement;
  placement.placement_id = strtoul( keys[ "p" ].c_str(), nullptr, 10 );
  placement.cols = atoi( keys[ "c" ].c_str() );
  placement.rows = atoi( keys[ "r" ].c_str() );
  return placement;
}

static uint32_t image_id( Keys &keys )
{
  return strtoul( keys[ "i" ].c_str(), nullptr, 10 );
}

static void delete_images( Keys &keys, Images &images )
{
  const std::string scope = keys[ "d" ].empty() ? "a" : keys[ "d" ];
  if ( scope == "a" ) {
    images.unplace_all();
  } else if ( scope == "A" ) {
    images.clear();
  } else if ( scope == "i" ) {
    images.place( image_id( keys ), std::nullopt );
  } else if ( scope == "I" ) {
    images.remove( image_id( keys ) );
  }
}

/* The image a transmission describes, before its payload arrives. */
static Image image_from( Keys &keys )
{
  Image image;
  image.id = image_id( keys );
  image.format = keys[ "f" ].empty() ? 32 : atoi( keys[ "f" ].c_str() );
  image.width = atoi( keys[ "s" ].c_str() );
  image.height = atoi( keys[ "v" ].c_str() );
  image.zlib = keys[ "o" ] == "z";
  return image;
}

static const char *const NOT_VIRTUAL = "ENOTSUPPORTED:only virtual placements (U=1) are supported";

/* Why a transmission is not one this terminal keeps, as kitty reports it,
   or empty when it is: identified by i, sent directly in the command rather
   than as a file, either PNG or raw pixels of a given size, and, when it is
   placed at once, placed virtually. */
static std::string unsupported( Keys &keys )
{
  const Image image = image_from( keys );
  if ( keys.count( "I" ) ) {
    return "ENOTSUPPORTED:image numbers are not supported";
  }
  if ( image.id == 0 ) {
    return "EINVAL:no image id";
  }
  if ( !keys[ "t" ].empty() && keys[ "t" ] != "d" ) {
    return "ENOTSUPPORTED:only direct transmission is supported";
  }
  if ( image.format != 100 && image.format != 24 && image.format != 32 ) {
    return "EINVAL:unknown format";
  }
  if ( image.format != 100 && ( image.width <= 0 || image.height <= 0 ) ) {
    return "EINVAL:raw pixels need s and v";
  }
  if ( keys[ "a" ] == "T" && keys[ "U" ] != "1" ) {
    return NOT_VIRTUAL;
  }
  return "";
}

/* A reply in the form kitty sends: OK, or an error code and message. */
static std::string reply( uint32_t id, const std::string &message )
{
  return "\033_Gi=" + std::to_string( id ) + ";" + message + "\033\\";
}

/* The reply to a finished transmission, unless q asks for none. Without an
   id there is nothing to reply to. */
static std::string answer( const Transmission &done )
{
  if ( done.id == 0 || done.quiet >= 2 || ( done.error.empty() && done.quiet >= 1 ) ) {
    return "";
  }
  return reply( done.id, done.error.empty() ? "OK" : done.error );
}

static const char *const TOO_BIG = "EFBIG:images are limited to 1 MiB";

std::string GraphicsReceiver::apply( const std::string &body, Images &images )
{
  if ( body.empty() || body[ 0 ] != 'G' ) {
    return "";
  }
  size_t semicolon = body.find( ';' );
  Keys keys = parse_keys( body.substr( 1, semicolon == std::string::npos ? std::string::npos : semicolon - 1 ) );
  std::string payload = semicolon == std::string::npos ? std::string() : body.substr( semicolon + 1 );

  if ( !transmission ) {
    const std::string action = keys[ "a" ].empty() ? "t" : keys[ "a" ];
    if ( action == "p" ) {
      Transmission placing;
      placing.id = image_id( keys );
      placing.query = false;
      placing.quiet = atoi( keys[ "q" ].c_str() );
      if ( keys[ "U" ] == "1" ) {
        images.place( placing.id, placement_from( keys ) );
        return "";
      }
      placing.error = NOT_VIRTUAL;
      return answer( placing );
    }
    if ( action == "d" ) {
      delete_images( keys, images );
      return "";
    }
    if ( action != "t" && action != "T" && action != "q" ) {
      return "";
    }
    Transmission started;
    started.id = image_id( keys );
    started.query = action == "q";
    started.quiet = atoi( keys[ "q" ].c_str() );
    started.error = unsupported( keys );
    if ( started.error.empty() ) {
      started.image = image_from( keys );
      if ( action == "T" && keys[ "U" ] == "1" ) {
        started.placement = placement_from( keys );
      }
    }
    transmission = started;
  }

  if ( transmission->image ) {
    transmission->image->base64 += payload;
    if ( transmission->image->base64.size() > MAX_IMAGE_BASE64 ) {
      transmission->image.reset();
      transmission->error = TOO_BIG;
    }
  }

  if ( keys[ "m" ] == "1" ) {
    return "";
  }
  Transmission done = *transmission;
  transmission.reset();
  if ( done.image && !is_base64( done.image->base64 ) ) {
    done.image.reset();
    done.error = "EINVAL:the payload is not base64";
  } else if ( done.image && decoded_size( done.image->base64 ) > MAX_IMAGE_BYTES ) {
    done.image.reset();
    done.error = TOO_BIG;
  }
  if ( done.image && !done.query ) {
    images.put( *done.image );
    if ( done.placement ) {
      images.place( done.id, done.placement );
    }
  }
  return answer( done );
}
