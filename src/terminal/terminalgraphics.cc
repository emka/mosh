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

#include <stdlib.h>

#include <map>

#include "terminalgraphics.h"

using namespace Terminal;

const Image *Images::get( uint32_t id ) const
{
  for ( const auto &image : images ) {
    if ( image->id == id ) {
      return image.get();
    }
  }
  return nullptr;
}

void Images::place( uint32_t id, const std::optional<ImagePlacement> &placement )
{
  for ( auto &image : images ) {
    if ( image->id == id ) {
      Image placed( *image );
      placed.placement = placement;
      image = shared::make_shared<const Image>( placed );
      return;
    }
  }
}

void Images::remove( uint32_t id )
{
  for ( auto i = images.begin(); i != images.end(); i++ ) {
    if ( (*i)->id == id ) {
      images.erase( i );
      return;
    }
  }
}

void Images::unplace_all( void )
{
  for ( auto &image : images ) {
    if ( image->placement ) {
      Image unplaced( *image );
      unplaced.placement.reset();
      image = shared::make_shared<const Image>( unplaced );
    }
  }
}

void Images::put( const Image &image )
{
  remove( image.id );
  images.push_back( shared::make_shared<const Image>( image ) );
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

static Image image_from( Keys &keys, const std::string &payload )
{
  Image image;
  image.id = image_id( keys );
  image.format = atoi( keys[ "f" ].c_str() );
  image.width = atoi( keys[ "s" ].c_str() );
  image.height = atoi( keys[ "v" ].c_str() );
  image.zlib = keys[ "o" ] == "z";
  image.base64 = payload;
  if ( keys[ "a" ] == "T" && keys[ "U" ] == "1" ) {
    image.placement = placement_from( keys );
  }
  return image;
}

void GraphicsReceiver::apply( const std::string &body, Images &images )
{
  if ( body.empty() || body[ 0 ] != 'G' ) {
    return;
  }
  size_t semicolon = body.find( ';' );
  Keys keys = parse_keys( body.substr( 1, semicolon == std::string::npos ? std::string::npos : semicolon - 1 ) );
  std::string payload = semicolon == std::string::npos ? std::string() : body.substr( semicolon + 1 );

  if ( upload ) {
    upload->base64 += payload;
  } else if ( keys[ "a" ] == "p" ) {
    if ( keys[ "U" ] == "1" ) {
      images.place( image_id( keys ), placement_from( keys ) );
    }
    return;
  } else if ( keys[ "a" ] == "d" ) {
    delete_images( keys, images );
    return;
  } else {
    upload = image_from( keys, payload );
  }

  if ( keys[ "m" ] != "1" ) {
    images.put( *upload );
    upload.reset();
  }
}
