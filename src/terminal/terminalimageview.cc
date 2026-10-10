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

#include <algorithm>

#include "terminalimageview.h"

using namespace Terminal;

std::optional<ImagePlacement> ImageView::placement( uint32_t id ) const
{
  auto found = placements.find( id );
  if ( found == placements.end() ) {
    return std::nullopt;
  }
  return found->second;
}

bool ImageView::has( const image_type &image ) const
{
  return std::find( complete.begin(), complete.end(), image ) != complete.end();
}

const Image *ImageView::get( uint32_t id ) const
{
  for ( const auto &image : complete ) {
    if ( image->id == id ) {
      return image.get();
    }
  }
  return nullptr;
}

/* Adds an image whose bytes the client has, with the placement it should have. */
void ImageView::add_complete( const image_type &image, const Images &target )
{
  complete.push_back( image );
  const std::optional<ImagePlacement> wanted = target.placement( image->id );
  if ( wanted ) {
    placements[ image->id ] = *wanted;
  }
}

bool ImageView::step_toward( const Images &target )
{
  bool changed = false;
  if ( upload && target.get( upload->id ) != upload.get() ) {
    upload.reset();
    sent = 0;
    changed = true;
  }
  if ( upload ) {
    sent = std::min( sent + SLICE_BYTES, upload->base64.size() );
    if ( sent == upload->base64.size() ) {
      add_complete( upload, target );
      upload.reset();
      sent = 0;
    }
    return true;
  }

  for ( auto i = complete.begin(); i != complete.end(); ) {
    if ( target.get( (*i)->id ) != i->get() ) {
      placements.erase( (*i)->id );
      i = complete.erase( i );
      changed = true;
    } else {
      if ( placements.count( (*i)->id ) && !target.placement( (*i)->id ) ) {
        placements.erase( (*i)->id );
        changed = true;
      }
      i++;
    }
  }

  size_t budget = SLICE_BYTES;
  for ( const auto &image : target.list() ) {
    if ( has( image ) ) {
      continue;
    }
    changed = true;
    if ( image->base64.size() > budget ) {
      upload = image;
      sent = budget;
      break;
    }
    budget -= image->base64.size();
    add_complete( image, target );
  }
  for ( const auto &image : complete ) {
    const std::optional<ImagePlacement> wanted = target.placement( image->id );
    if ( wanted && wanted != placement( image->id ) ) {
      placements[ image->id ] = *wanted;
      changed = true;
    }
  }
  return changed;
}
