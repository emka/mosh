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

/* Kitty graphics commands kept as framebuffer state. */

#include <locale.h>

#include <string>

#include "completeterminal.h"
#include "fatal_assert.h"
#include "locale_utils.h"

static std::string cell_text( const Terminal::Framebuffer &fb, int row, int col )
{
  std::string text;
  fb.get_cell( row, col )->print_grapheme( text );
  return text;
}

static bool screen_is_blank( const Terminal::Framebuffer &fb )
{
  for ( int row = 0; row < fb.ds.get_height(); row++ ) {
    for ( int col = 0; col < fb.ds.get_width(); col++ ) {
      if ( !fb.get_cell( row, col )->empty() ) {
        return false;
      }
    }
  }
  return true;
}

/* Base64 that decodes to the given number of zero bytes. */
static std::string base64_of_zeros( size_t bytes )
{
  std::string encoded( bytes / 3 * 4, 'A' );
  if ( bytes % 3 == 1 ) {
    encoded += "AA==";
  } else if ( bytes % 3 == 2 ) {
    encoded += "AAA=";
  }
  return encoded;
}

/* Sends an image to the terminal in chunks of 4096 base64 bytes. */
static void transmit_in_chunks( Terminal::Complete &term, uint32_t id, const std::string &base64 )
{
  for ( size_t start = 0; start < base64.size(); start += 4096 ) {
    const bool first = start == 0;
    const bool last = start + 4096 >= base64.size();
    std::string command = "\033_G";
    if ( first ) {
      command += "a=t,f=100,i=" + std::to_string( id ) + ",";
    }
    command += last ? "m=0;" : "m=1;";
    command += base64.substr( start, 4096 ) + "\033\\";
    term.act( command );
  }
}

static void stores_an_image_transmitted_in_one_command( void )
{
  // Given
  Terminal::Complete term( 80, 24 );

  // When
  term.act( "\033_Ga=t,i=7,f=100;iVBORw0KGgo=\033\\" );

  // Then
  const Terminal::Image *image = term.get_fb().get_image( 7 );
  fatal_assert( image && image->format == 100 && image->base64 == "iVBORw0KGgo=" );
}

static void stores_an_image_ended_by_the_c1_string_terminator( void )
{
  // Given
  Terminal::Complete term( 80, 24 );

  // When
  term.act( "\033_Ga=t,i=7,f=100;iVBORw0KGgo=\xc2\x9c" );

  // Then
  const Terminal::Image *image = term.get_fb().get_image( 7 );
  fatal_assert( image && image->base64 == "iVBORw0KGgo=" );
}

static void stores_the_size_and_compression_of_raw_pixels( void )
{
  // Given
  Terminal::Complete term( 80, 24 );

  // When
  term.act( "\033_Ga=t,i=7,f=32,s=2,v=3,o=z;eJw=\033\\" );

  // Then
  const Terminal::Image *image = term.get_fb().get_image( 7 );
  fatal_assert( image && image->format == 32 && image->width == 2 && image->height == 3 && image->zlib );
}

static void joins_chunks_into_one_image( void )
{
  // Given
  Terminal::Complete term( 80, 24 );
  term.act( "\033_Ga=t,i=7,f=100,m=1;iVBO\033\\" );
  term.act( "\033_Gm=1;Rw0K\033\\" );

  // When
  term.act( "\033_Gm=0;Ggo=\033\\" );

  // Then
  const Terminal::Image *image = term.get_fb().get_image( 7 );
  fatal_assert( term.get_fb().image_count() == 1 && image && image->format == 100 && image->base64 == "iVBORw0KGgo=" );
}

static void stores_a_command_split_across_reads( void )
{
  // Given
  Terminal::Complete term( 80, 24 );
  term.act( "\033_Ga=t,i=7,f=100;iVBOR" );

  // When
  term.act( "w0KGgo=\033\\" );

  // Then
  const Terminal::Image *image = term.get_fb().get_image( 7 );
  fatal_assert( image && image->base64 == "iVBORw0KGgo=" );
}

static void keeps_output_between_chunks_acting_on_the_screen( void )
{
  // Given
  Terminal::Complete term( 80, 24 );
  term.act( "x\033_Ga=t,i=7,f=100,m=1;iVBORw0K\033\\" );

  // When
  term.act( "\033[2J\033_Gm=0;Ggo=\033\\" );

  // Then
  const Terminal::Image *image = term.get_fb().get_image( 7 );
  fatal_assert( term.get_fb().get_cell( 0, 0 )->empty() && image && image->base64 == "iVBORw0KGgo=" );
}

static void replaces_an_image_sent_again_with_the_same_id( void )
{
  // Given
  Terminal::Complete term( 80, 24 );
  term.act( "\033_Ga=t,i=7,f=100;iVBORw0KGgo=\033\\" );

  // When
  term.act( "\033_Ga=t,i=7,f=100;AAAA\033\\" );

  // Then
  const Terminal::Image *image = term.get_fb().get_image( 7 );
  fatal_assert( term.get_fb().image_count() == 1 && image && image->base64 == "AAAA" );
}

static void never_prints_a_graphics_command( void )
{
  // Given
  Terminal::Complete term( 80, 24 );

  // When
  term.act( "\033_Ga=t,i=7,f=100;iVBORw0KGgo=\033\\" );

  // Then
  fatal_assert( screen_is_blank( term.get_fb() ) );
}

static void swallows_start_of_string_and_privacy_messages( void )
{
  // Given
  Terminal::Complete term( 80, 24 );

  // When
  term.act( "\033XGa=t,i=7,f=100;AAAA\033\\\033^Ga=t,i=8,f=100;AAAA\033\\" );

  // Then
  fatal_assert( term.get_fb().image_count() == 0 && screen_is_blank( term.get_fb() ) );
}

static void ignores_application_commands_other_than_graphics( void )
{
  // Given
  Terminal::Complete term( 80, 24 );

  // When
  term.act( "\033_Xa=t,i=7,f=100;AAAA\033\\" );

  // Then
  fatal_assert( term.get_fb().image_count() == 0 );
}

static void tells_a_terminal_apart_from_its_copy_once_it_gets_an_image( void )
{
  // Given
  Terminal::Complete term( 80, 24 );
  const Terminal::Complete copy( term );

  // When
  term.act( "\033_Ga=t,i=7,f=100;AAAA\033\\" );

  // Then
  fatal_assert( !( term == copy ) );
}

static void keeps_an_assigned_copy_independent_of_the_original( void )
{
  // Given
  Terminal::Complete term( 80, 24 );
  term.act( "\033_Ga=t,i=7,f=100;AAAA\033\\" );
  Terminal::Complete copy( 80, 24 );
  copy = term;

  // When
  term.act( "\033_Ga=t,i=7,f=100;BBBB\033\\" );

  // Then
  const Terminal::Image *image = copy.get_fb().get_image( 7 );
  fatal_assert( image && image->base64 == "AAAA" );
}

static bool has_placement( const Terminal::Framebuffer &fb, uint32_t id, uint32_t placement_id, int cols, int rows )
{
  const std::optional<Terminal::ImagePlacement> placement = fb.get_placement( id );
  return placement && placement->placement_id == placement_id
    && placement->cols == cols && placement->rows == rows;
}

static void stores_the_virtual_placement_sent_with_an_image( void )
{
  // Given
  Terminal::Complete term( 80, 24 );

  // When
  term.act( "\033_Ga=T,U=1,i=1,c=4,r=2,f=100;AAAA\033\\" );

  // Then
  fatal_assert( has_placement( term.get_fb(), 1, 0, 4, 2 ) );
}

static void places_a_stored_image( void )
{
  // Given
  Terminal::Complete term( 80, 24 );
  term.act( "\033_Ga=t,i=1,f=100;AAAA\033\\" );

  // When
  term.act( "\033_Ga=p,U=1,i=1,p=3,c=10,r=5\033\\" );

  // Then
  const Terminal::Image *image = term.get_fb().get_image( 1 );
  fatal_assert( has_placement( term.get_fb(), 1, 3, 10, 5 ) && image && image->base64 == "AAAA" );
}

static void keeps_only_the_latest_placement_of_an_image( void )
{
  // Given
  Terminal::Complete term( 80, 24 );
  term.act( "\033_Ga=T,U=1,i=1,c=4,r=2,f=100;AAAA\033\\" );

  // When
  term.act( "\033_Ga=p,U=1,i=1,p=2,c=6,r=3\033\\" );

  // Then
  fatal_assert( has_placement( term.get_fb(), 1, 2, 6, 3 ) );
}

static void ignores_a_placement_of_an_unknown_image( void )
{
  // Given
  Terminal::Complete term( 80, 24 );

  // When
  term.act( "\033_Ga=p,U=1,i=1,c=4,r=2\033\\" );

  // Then
  fatal_assert( term.get_fb().image_count() == 0 );
}

static void refuses_a_direct_placement( void )
{
  // Given
  Terminal::Complete term( 80, 24 );

  // When
  term.act( "\033_Ga=T,i=1,c=4,r=2,f=100;AAAA\033\\" );

  // Then
  fatal_assert( term.get_fb().image_count() == 0 );
}

static void deletes_the_placement_of_one_image( void )
{
  // Given
  Terminal::Complete term( 80, 24 );
  term.act( "\033_Ga=T,U=1,i=1,c=4,r=2,f=100;AAAA\033\\" );

  // When
  term.act( "\033_Ga=d,d=i,i=1\033\\" );

  // Then
  const Terminal::Image *image = term.get_fb().get_image( 1 );
  fatal_assert( image && !term.get_fb().get_placement( 1 ) && image->base64 == "AAAA" );
}

static void deletes_one_image( void )
{
  // Given
  Terminal::Complete term( 80, 24 );
  term.act( "\033_Ga=t,i=1,f=100;AAAA\033\\\033_Ga=t,i=2,f=100;BBBB\033\\" );

  // When
  term.act( "\033_Ga=d,d=I,i=1\033\\" );

  // Then
  fatal_assert( !term.get_fb().get_image( 1 ) && term.get_fb().get_image( 2 ) );
}

static void deletes_every_placement( void )
{
  // Given
  Terminal::Complete term( 80, 24 );
  term.act( "\033_Ga=T,U=1,i=1,c=1,r=1,f=100;AAAA\033\\\033_Ga=T,U=1,i=2,c=1,r=1,f=100;BBBB\033\\" );

  // When
  term.act( "\033_Ga=d,d=a\033\\" );

  // Then
  const Terminal::Image *first = term.get_fb().get_image( 1 );
  const Terminal::Image *second = term.get_fb().get_image( 2 );
  fatal_assert( first && !term.get_fb().get_placement( 1 ) && second && !term.get_fb().get_placement( 2 ) );
}

static void deletes_every_image( void )
{
  // Given
  Terminal::Complete term( 80, 24 );
  term.act( "\033_Ga=T,U=1,i=1,c=1,r=1,f=100;AAAA\033\\\033_Ga=t,i=2,f=100;BBBB\033\\" );

  // When
  term.act( "\033_Ga=d,d=A\033\\" );

  // Then
  fatal_assert( term.get_fb().image_count() == 0 );
}

static void deletes_every_placement_when_no_scope_is_given( void )
{
  // Given
  Terminal::Complete term( 80, 24 );
  term.act( "\033_Ga=T,U=1,i=1,c=1,r=1,f=100;AAAA\033\\" );

  // When
  term.act( "\033_Ga=d\033\\" );

  // Then
  const Terminal::Image *image = term.get_fb().get_image( 1 );
  fatal_assert( image && !term.get_fb().get_placement( 1 ) );
}

static void refuses_an_image_over_one_mebibyte( void )
{
  // Given
  Terminal::Complete term( 80, 24 );

  // When
  transmit_in_chunks( term, 1, base64_of_zeros( 1024 * 1024 + 1 ) );

  // Then
  fatal_assert( term.get_fb().image_count() == 0 );
}

static void stores_an_image_of_exactly_one_mebibyte( void )
{
  // Given
  Terminal::Complete term( 80, 24 );

  // When
  transmit_in_chunks( term, 1, base64_of_zeros( 1024 * 1024 ) );

  // Then
  const Terminal::Image *image = term.get_fb().get_image( 1 );
  fatal_assert( image && image->base64 == base64_of_zeros( 1024 * 1024 ) );
}

static Terminal::Image image_of_eight_bytes( uint32_t id )
{
  Terminal::Image image;
  image.id = id;
  image.format = 100;
  image.width = 0;
  image.height = 0;
  image.zlib = false;
  image.base64 = base64_of_zeros( 8 );
  return image;
}

static void evicts_the_oldest_image_past_the_table_limit( void )
{
  // Given
  Terminal::Images images( 20 );
  images.put( image_of_eight_bytes( 1 ) );
  images.put( image_of_eight_bytes( 2 ) );

  // When
  images.put( image_of_eight_bytes( 3 ) );

  // Then
  fatal_assert( !images.get( 1 ) && images.get( 2 ) && images.get( 3 ) );
}

static void counts_a_replaced_image_as_the_newest( void )
{
  // Given
  Terminal::Images images( 20 );
  images.put( image_of_eight_bytes( 1 ) );
  images.put( image_of_eight_bytes( 2 ) );
  images.put( image_of_eight_bytes( 1 ) );

  // When
  images.put( image_of_eight_bytes( 3 ) );

  // Then
  fatal_assert( images.get( 1 ) && !images.get( 2 ) && images.get( 3 ) );
}

static void limits_an_image_to_one_mebibyte_and_all_images_to_sixteen( void )
{
  // Given
  const Terminal::Images images;

  // When
  const size_t table_limit = Terminal::Images::MAX_BYTES;
  const size_t image_limit = Terminal::GraphicsReceiver::MAX_IMAGE_BYTES;

  // Then
  fatal_assert( image_limit == 1024 * 1024 && table_limit == 16 * 1024 * 1024 && images.size() == 0 );
}

static void drops_a_command_longer_than_any_image_needs( void )
{
  // Given
  Terminal::Complete term( 80, 24 );
  std::string padding;
  for ( int i = 0; i < 512 * 1024; i++ ) {
    padding += ",x=1";
  }

  // When
  term.act( "\033_Ga=t,i=1,f=100" + padding + ";AAAA\033\\hi" );

  // Then
  fatal_assert( term.get_fb().image_count() == 0 && cell_text( term.get_fb(), 0, 0 ) == "h" );
}

static void refuses_a_payload_that_is_not_base64( void )
{
  // Given
  Terminal::Complete term( 80, 24 );

  // When
  term.act( "\033_Ga=t,i=1,f=100;@@@@\033\\" );

  // Then
  fatal_assert( term.get_fb().image_count() == 0 );
}

static void refuses_padding_before_the_last_chunk( void )
{
  // Given
  Terminal::Complete term( 80, 24 );
  term.act( "\033_Ga=t,i=1,f=100,m=1;AA==\033\\" );

  // When
  term.act( "\033_Gm=0;AA==\033\\" );

  // Then
  fatal_assert( term.get_fb().image_count() == 0 );
}

static void refuses_an_image_without_an_id( void )
{
  // Given
  Terminal::Complete term( 80, 24 );

  // When
  term.act( "\033_Ga=t,f=100;AAAA\033\\\033_Ga=t,i=0,f=100;AAAA\033\\\033_Ga=t,I=5,f=100;AAAA\033\\" );

  // Then
  fatal_assert( term.get_fb().image_count() == 0 );
}

static void refuses_images_sent_as_files( void )
{
  // Given
  Terminal::Complete term( 80, 24 );

  // When
  term.act( "\033_Ga=t,t=f,i=1,f=100;L3RtcC9h\033\\\033_Ga=t,t=s,i=2,f=100;L3RtcC9h\033\\" );

  // Then
  fatal_assert( term.get_fb().image_count() == 0 );
}

static void refuses_raw_pixels_without_a_size_and_unknown_formats( void )
{
  // Given
  Terminal::Complete term( 80, 24 );

  // When
  term.act( "\033_Ga=t,i=1,f=24,s=1;AAAA\033\\\033_Ga=t,i=2,f=32,v=1;AAAA\033\\\033_Ga=t,i=3,f=99;AAAA\033\\" );

  // Then
  fatal_assert( term.get_fb().image_count() == 0 );
}

static void takes_an_image_without_a_format_as_rgba( void )
{
  // Given
  Terminal::Complete term( 80, 24 );

  // When
  term.act( "\033_Ga=t,i=1,s=1,v=1;AAAAAA==\033\\" );

  // Then
  const Terminal::Image *image = term.get_fb().get_image( 1 );
  fatal_assert( image && image->format == 32 );
}

static void drops_every_image_on_a_full_reset( void )
{
  // Given
  Terminal::Complete term( 80, 24 );
  term.act( "\033_Ga=t,i=1,f=100;AAAA\033\\" );

  // When
  term.act( "\033c" );

  // Then
  fatal_assert( term.get_fb().image_count() == 0 );
}

static void ignores_actions_other_than_transmit_place_and_delete( void )
{
  // Given
  Terminal::Complete term( 80, 24 );
  term.act( "\033_Ga=t,i=1,f=100;AAAA\033\\" );

  // When
  term.act( "\033_Ga=f,i=1,f=100;BBBB\033\\" );

  // Then
  const Terminal::Image *image = term.get_fb().get_image( 1 );
  fatal_assert( image && image->base64 == "AAAA" );
}

static void deletes_after_an_abandoned_transmission( void )
{
  // Given
  Terminal::Complete term( 80, 24 );
  term.act( "\033_Ga=t,i=1,f=100;AAAA\033\\\033_Ga=t,i=5,f=100,m=1;AAAA\033\\" );

  // When
  term.act( "\033_Ga=d,d=A\033\\" );

  // Then
  fatal_assert( term.get_fb().image_count() == 0 );
}

static void forgets_an_unfinished_transmission_on_a_full_reset( void )
{
  // Given
  Terminal::Complete term( 80, 24 );
  term.act( "\033_Ga=t,i=5,f=100,m=1;AAAA\033\\" );

  // When
  term.act( "\033c\033_Gm=0;AAAA\033\\" );

  // Then
  fatal_assert( term.get_fb().image_count() == 0 );
}

int main( void )
{
  /* mosh-server runs in a UTF-8 locale; the parser decodes input with it. */
  setlocale( LC_ALL, "C.UTF-8" );
  fatal_assert( is_utf8_locale() );

  stores_an_image_transmitted_in_one_command();
  stores_an_image_ended_by_the_c1_string_terminator();
  stores_the_size_and_compression_of_raw_pixels();
  joins_chunks_into_one_image();
  stores_a_command_split_across_reads();
  keeps_output_between_chunks_acting_on_the_screen();
  replaces_an_image_sent_again_with_the_same_id();
  never_prints_a_graphics_command();
  swallows_start_of_string_and_privacy_messages();
  ignores_application_commands_other_than_graphics();
  tells_a_terminal_apart_from_its_copy_once_it_gets_an_image();
  keeps_an_assigned_copy_independent_of_the_original();
  stores_the_virtual_placement_sent_with_an_image();
  places_a_stored_image();
  keeps_only_the_latest_placement_of_an_image();
  ignores_a_placement_of_an_unknown_image();
  refuses_a_direct_placement();
  deletes_the_placement_of_one_image();
  deletes_one_image();
  deletes_every_placement();
  deletes_every_image();
  deletes_every_placement_when_no_scope_is_given();
  refuses_an_image_over_one_mebibyte();
  stores_an_image_of_exactly_one_mebibyte();
  evicts_the_oldest_image_past_the_table_limit();
  counts_a_replaced_image_as_the_newest();
  limits_an_image_to_one_mebibyte_and_all_images_to_sixteen();
  drops_a_command_longer_than_any_image_needs();
  refuses_a_payload_that_is_not_base64();
  refuses_padding_before_the_last_chunk();
  refuses_an_image_without_an_id();
  refuses_images_sent_as_files();
  refuses_raw_pixels_without_a_size_and_unknown_formats();
  takes_an_image_without_a_format_as_rgba();
  drops_every_image_on_a_full_reset();
  ignores_actions_other_than_transmit_place_and_delete();
  deletes_after_an_abandoned_transmission();
  forgets_an_unfinished_transmission_on_a_full_reset();
  return 0;
}
