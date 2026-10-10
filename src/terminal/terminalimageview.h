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

#ifndef TERMINALIMAGEVIEW_HPP
#define TERMINALIMAGEVIEW_HPP

#include <map>
#include <optional>

#include "terminalgraphics.h"

namespace Terminal {
  /* The images a client should have now: complete images with their
     placements, and at most one image whose upload is under way. It
     follows a terminal's images one step at a time, so that each step
     sends the client at most one slice of image data. */
  class ImageView {
  public:
    typedef shared::shared_ptr<const Image> image_type;

  private:
    Images::list_type complete;
    std::map<uint32_t, ImagePlacement> placements; /* by image id */
    image_type upload;
    size_t sent; /* base64 bytes of the upload */

    void add_complete( const image_type &image, const Images &target );

  public:
    /* The most base64 one step sends. */
    static const size_t SLICE_BYTES = 64 * 1024;

    ImageView() : complete(), placements(), upload(), sent( 0 ) {}

    const Images::list_type & images( void ) const { return complete; }
    bool has( const image_type &image ) const;
    const Image *get( uint32_t id ) const;
    std::optional<ImagePlacement> placement( uint32_t id ) const;
    const image_type & uploading( void ) const { return upload; }
    size_t uploaded( void ) const { return sent; }

    /* Moves one step toward the given images; returns whether anything changed. */
    bool step_toward( const Images &target );

    bool operator==( const ImageView &x ) const
    {
      return complete == x.complete && placements == x.placements && upload == x.upload && sent == x.sent;
    }
  };
}

#endif
