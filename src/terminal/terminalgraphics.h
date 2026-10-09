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
  };

  struct Image {
    uint32_t id;
    int format;          /* f: 24 (RGB), 32 (RGBA) or 100 (PNG) */
    int width, height;   /* s and v; 0 when absent */
    bool zlib;           /* o=z */
    std::string base64;  /* the whole payload, as received */
    std::optional<ImagePlacement> placement;
  };

  /* The images a terminal holds. Copies share each image. */
  class Images {
  private:
    std::vector< shared::shared_ptr<const Image> > images;

  public:
    Images() : images() {}

    const Image *get( uint32_t id ) const;
    size_t size( void ) const { return images.size(); }
    void put( const Image &image );

    bool operator==( const Images &x ) const { return images == x.images; }
  };

  /* Applies graphics commands, the bodies of APC strings starting with G, to
     the images a terminal holds. Keeps a transmission sent in chunks until
     its last chunk arrives. */
  class GraphicsReceiver {
  private:
    std::optional<Image> upload;

  public:
    GraphicsReceiver() : upload() {}

    void apply( const std::string &body, Images &images );
  };
}

#endif
