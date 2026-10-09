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

/* Colours given with colon-separated subparameters (ITU T.416), as
   kitten icat and other modern programs send them. */

#include <locale.h>

#include <string>

#include "completeterminal.h"
#include "fatal_assert.h"
#include "locale_utils.h"

/* Whether text drawn after each sequence looks the same. */
static bool same_cell( const std::string &sequence, const std::string &expected )
{
  Terminal::Complete term( 80, 24 ), reference( 80, 24 );
  term.act( sequence + "x" );
  reference.act( expected + "x" );
  return *term.get_fb().get_cell( 0, 0 ) == *reference.get_fb().get_cell( 0, 0 );
}

static void takes_a_true_colour_with_colons( void )
{
  // Given
  const std::string colons = "\033[38:2:64:228:216m";

  // When
  const bool same = same_cell( colons, "\033[38;2;64;228;216m" );

  // Then
  fatal_assert( same );
}

static void skips_the_colour_space_of_a_true_colour( void )
{
  // Given
  const std::string colons = "\033[38:2::64:228:216m";

  // When
  const bool same = same_cell( colons, "\033[38;2;64;228;216m" );

  // Then
  fatal_assert( same );
}

static void takes_an_indexed_background_with_colons( void )
{
  // Given
  const std::string colons = "\033[48:5:123m";

  // When
  const bool same = same_cell( colons, "\033[48;5;123m" );

  // Then
  fatal_assert( same );
}

static void keeps_only_the_first_value_of_other_subparameters( void )
{
  // Given
  const std::string curly_underline = "\033[4:3m";

  // When
  const bool same = same_cell( curly_underline, "\033[4m" );

  // Then
  fatal_assert( same );
}

int main( void )
{
  /* mosh-server runs in a UTF-8 locale; the parser decodes input with it. */
  setlocale( LC_ALL, "C.UTF-8" );
  fatal_assert( is_utf8_locale() );

  takes_a_true_colour_with_colons();
  skips_the_colour_space_of_a_true_colour();
  takes_an_indexed_background_with_colons();
  keeps_only_the_first_value_of_other_subparameters();
  return 0;
}
