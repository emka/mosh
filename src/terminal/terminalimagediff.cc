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

#include <stdio.h>

#include <algorithm>

#include "terminalimagediff.h"

using namespace Terminal;

/* The most base64 one graphics command carries; the client discards longer ones. */
static const size_t IMAGE_CHUNK_BYTES = 4096;

/* Sends the base64 of an image from one offset to another, in chunks the
   client accepts, telling the terminal that receives it not to reply. The
   first chunk of an image carries its keys; the last chunk of a complete
   image ends the upload. */
static void append_chunks( std::string &out, const Image &image, size_t from, size_t to )
{
  const std::string &base64 = image.base64;
  for ( size_t start = from; start == from || start < to; start += IMAGE_CHUNK_BYTES ) {
    const size_t end = std::min( start + IMAGE_CHUNK_BYTES, to );
    if ( start == 0 ) {
      char keys[ 128 ];
      snprintf( keys, sizeof keys, "\033_Ga=t,i=%u,f=%d,q=2", image.id, image.format );
      out.append( keys );
      if ( image.width > 0 && image.height > 0 ) {
        snprintf( keys, sizeof keys, ",s=%d,v=%d", image.width, image.height );
        out.append( keys );
      }
      if ( image.zlib ) {
        out.append( ",o=z" );
      }
    } else {
      out.append( "\033_Gq=2" );
    }
    if ( start > 0 || end < base64.size() ) {
      out.append( end == base64.size() ? ",m=0" : ",m=1" );
    }
    out.append( 1, ';' );
    out.append( base64, start, end - start );
    out.append( "\033\\" );
  }
}

/* Places an image virtually, for the client to show in placeholder cells. */
static void append_placement( std::string &out, uint32_t id, const ImagePlacement &placement )
{
  char command[ 128 ];
  snprintf( command, sizeof command, "\033_Ga=p,U=1,i=%u,p=%u,c=%d,r=%d,q=2\033\\",
            id, placement.placement_id, placement.cols, placement.rows );
  out.append( command );
}

/* Deletes an image (scope I) or only its placement (scope i). */
static void append_delete( std::string &out, char scope, uint32_t id )
{
  char command[ 64 ];
  snprintf( command, sizeof command, "\033_Ga=d,d=%c,i=%u,q=2\033\\", scope, id );
  out.append( command );
}

/* Whether a view has an image with the given id, complete or under way. */
static bool holds( const ImageView &view, uint32_t id )
{
  return view.get( id ) || ( view.uploading() && view.uploading()->id == id );
}

std::string Terminal::image_commands( const ImageView &last, const ImageView &now )
{
  std::string out;
  const ImageView::image_type &continued = last.uploading();
  if ( continued && ( now.has( continued ) || now.uploading() == continued ) ) {
    const size_t to = now.has( continued ) ? continued->base64.size() : now.uploaded();
    if ( to > last.uploaded() ) {
      append_chunks( out, *continued, last.uploaded(), to );
    }
  } else if ( continued && !holds( now, continued->id ) ) {
    append_delete( out, 'I', continued->id );
  }
  for ( const auto &image : last.images() ) {
    if ( !now.get( image->id ) ) {
      append_delete( out, 'I', image->id );
    } else if ( last.placement( image->id ) && !now.placement( image->id ) ) {
      append_delete( out, 'i', image->id );
    }
  }
  for ( const auto &image : now.images() ) {
    if ( image != continued && !last.has( image ) ) {
      append_chunks( out, *image, 0, image->base64.size() );
    }
  }
  for ( const auto &image : now.images() ) {
    const std::optional<ImagePlacement> placement = now.placement( image->id );
    if ( placement && placement != last.placement( image->id ) ) {
      append_placement( out, image->id, *placement );
    }
  }
  if ( now.uploading() && now.uploading() != continued ) {
    append_chunks( out, *now.uploading(), 0, now.uploaded() );
  }
  return out;
}
