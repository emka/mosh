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
  return 0;
}
