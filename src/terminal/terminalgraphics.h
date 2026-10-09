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

#ifndef TERMINALGRAPHICS_HPP
#define TERMINALGRAPHICS_HPP

#include <stdint.h>

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "shared.h"

/* Images sent with the Kitty graphics protocol, kept as terminal state. */

namespace Terminal {
  struct ImagePlacement {
    uint32_t placement_id;
    int cols;
    int rows;

    bool operator==( const ImagePlacement &x ) const
    {
      return placement_id == x.placement_id && cols == x.cols && rows == x.rows;
    }
    bool operator!=( const ImagePlacement &x ) const { return !operator==( x ); }
  };

  struct Image {
    uint32_t id;
    int format;          /* f: 24 (RGB), 32 (RGBA) or 100 (PNG) */
    int width, height;   /* s and v; 0 when absent */
    bool zlib;           /* o=z */
    std::string base64;  /* the whole payload, as received */
  };

  /* The images a terminal holds, and the latest virtual placement of each.
     Copies share each image; an image is replaced, never changed, so a new
     pointer means new pixels. */
  class Images {
  public:
    typedef std::vector< shared::shared_ptr<const Image> > list_type;

  private:
    list_type images; /* oldest first */
    std::map<uint32_t, ImagePlacement> placements; /* by image id */
    size_t max_bytes;

  public:
    /* The most all images together may take once decoded from base64. */
    static const size_t MAX_BYTES = 16 * 1024 * 1024;

    Images( size_t s_max_bytes = MAX_BYTES ) : images(), placements(), max_bytes( s_max_bytes ) {}

    const Image *get( uint32_t id ) const;
    size_t size( void ) const { return images.size(); }
    const list_type & list( void ) const { return images; }
    std::optional<ImagePlacement> placement( uint32_t id ) const;
    /* Stores an image, replacing one with the same id, and evicts the oldest
       images while all of them take more than the limit. */
    void put( const Image &image );
    void remove( uint32_t id );
    void unplace_all( void ) { placements.clear(); }
    void clear( void ) { images.clear(); placements.clear(); }
    /* Sets or, with an empty placement, drops the placement of a stored image. */
    void place( uint32_t id, const std::optional<ImagePlacement> &placement );

    bool operator==( const Images &x ) const { return images == x.images && placements == x.placements; }
  };

  /* Applies graphics commands, the bodies of APC strings starting with G, to
     the images a terminal holds. Keeps a transmission sent in chunks until
     its last chunk arrives. */
  class GraphicsReceiver {
  private:
    std::optional<Image> upload;
    std::optional<ImagePlacement> upload_placement; /* a=T with U=1 places the image once stored */
    bool refused; /* the transmission in progress is dropped until its last chunk */

  public:
    /* The most an image may take once decoded from base64. */
    static const size_t MAX_IMAGE_BYTES = 1024 * 1024;

    /* The longest base64 that decodes to at most MAX_IMAGE_BYTES. */
    static const size_t MAX_IMAGE_BASE64 = ( MAX_IMAGE_BYTES + 2 ) / 3 * 4;

    /* The longest command worth reading: a whole image plus room for its keys. */
    static const size_t MAX_COMMAND_BYTES = MAX_IMAGE_BASE64 + 4096;

    GraphicsReceiver() : upload(), upload_placement(), refused( false ) {}

    void apply( const std::string &body, Images &images );
  };
}

#endif
