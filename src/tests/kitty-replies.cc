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

/* Replies to Kitty graphics commands, as kitty sends them. */

#include <locale.h>

#include <string>

#include "completeterminal.h"
#include "fatal_assert.h"
#include "locale_utils.h"

static void answers_a_transmission_with_ok( void )
{
  // Given
  Terminal::Complete term( 80, 24 );

  // When
  const std::string reply = term.act( "\033_Ga=t,i=7,f=100;AAAA\033\\" );

  // Then
  fatal_assert( reply == "\033_Gi=7;OK\033\\" );
}

static void keeps_quiet_about_success_when_asked( void )
{
  // Given
  Terminal::Complete term( 80, 24 );

  // When
  const std::string reply = term.act( "\033_Ga=t,i=7,f=100,q=1;AAAA\033\\" );

  // Then
  fatal_assert( reply.empty() && term.get_fb().get_image( 7 ) );
}

static void answers_an_image_over_the_limit_with_efbig( void )
{
  // Given
  Terminal::Complete term( 80, 24 );
  const std::string base64( 1400000, 'A' );

  // When
  const std::string reply = term.act( "\033_Ga=t,i=7,f=100,m=1;" + base64.substr( 0, 4096 ) + "\033\\" )
    + term.act( "\033_Gm=0;" + base64.substr( 4096 ) + "\033\\" );

  // Then
  fatal_assert( reply.rfind( "\033_Gi=7;EFBIG:", 0 ) == 0 && reply.size() > 14 && reply.substr( reply.size() - 2 ) == "\033\\" );
}

static void refuses_files_so_programs_send_the_data_instead( void )
{
  // Given
  Terminal::Complete term( 80, 24 );

  // When
  const std::string reply = term.act( "\033_Ga=t,t=f,i=7,f=100;L3RtcC9h\033\\" );

  // Then
  fatal_assert( reply.rfind( "\033_Gi=7;ENOTSUPPORTED:", 0 ) == 0 );
}

static void keeps_quiet_about_errors_when_asked( void )
{
  // Given
  Terminal::Complete term( 80, 24 );

  // When
  const std::string reply = term.act( "\033_Ga=t,t=f,i=7,f=100,q=2;L3RtcC9h\033\\" );

  // Then
  fatal_assert( reply.empty() );
}

static void answers_a_query_without_storing_its_image( void )
{
  // Given
  Terminal::Complete term( 80, 24 );

  // When
  const std::string reply = term.act( "\033_Ga=q,i=31,s=1,v=1,f=24;AAAA\033\\" );

  // Then
  fatal_assert( reply == "\033_Gi=31;OK\033\\" && term.get_fb().image_count() == 0 );
}

static void answers_a_query_for_files_with_an_error( void )
{
  // Given
  Terminal::Complete term( 80, 24 );

  // When
  const std::string reply = term.act( "\033_Ga=q,t=f,i=32,s=1,v=1,f=24;L3RtcC9h\033\\" );

  // Then
  fatal_assert( reply.rfind( "\033_Gi=32;ENOTSUPPORTED:", 0 ) == 0 && term.get_fb().image_count() == 0 );
}

static void answers_a_chunked_transmission_once_it_is_complete( void )
{
  // Given
  Terminal::Complete term( 80, 24 );
  const std::string first = term.act( "\033_Ga=t,i=7,f=100,m=1;AAAA\033\\" );

  // When
  const std::string last = term.act( "\033_Gm=0;AAAA\033\\" );

  // Then
  fatal_assert( first.empty() && last == "\033_Gi=7;OK\033\\" );
}

static void answers_nothing_without_an_image_id( void )
{
  // Given
  Terminal::Complete term( 80, 24 );

  // When
  const std::string reply = term.act( "\033_Ga=t,f=100;AAAA\033\\" );

  // Then
  fatal_assert( reply.empty() );
}

static void tells_programs_that_direct_placements_are_not_supported( void )
{
  // Given
  Terminal::Complete term( 80, 24 );
  term.act( "\033_Ga=t,i=1,f=100;AAAA\033\\" );

  // When
  const std::string reply = term.act( "\033_Ga=T,i=7,f=100;AAAA\033\\" ) + term.act( "\033_Ga=p,i=1\033\\" );

  // Then
  fatal_assert( reply.rfind( "\033_Gi=7;ENOTSUPPORTED:", 0 ) == 0
                && reply.find( "\033_Gi=1;ENOTSUPPORTED:" ) != std::string::npos );
}

static void answers_a_new_command_after_an_abandoned_transmission( void )
{
  // Given
  Terminal::Complete term( 80, 24 );
  term.act( "\033_Ga=t,i=5,f=100,m=1;AAAA\033\\" );

  // When
  const std::string reply = term.act( "\033_Ga=q,i=31,s=1,v=1,f=24;AAAA\033\\" );

  // Then
  fatal_assert( reply == "\033_Gi=31;OK\033\\" && term.get_fb().image_count() == 0 );
}

static void refuses_an_empty_image( void )
{
  // Given
  Terminal::Complete term( 80, 24 );

  // When
  const std::string reply = term.act( "\033_Ga=t,i=7,f=100\033\\" );

  // Then
  fatal_assert( reply.rfind( "\033_Gi=7;EINVAL:", 0 ) == 0 && term.get_fb().image_count() == 0 );
}

int main( void )
{
  /* mosh-server runs in a UTF-8 locale; the parser decodes input with it. */
  setlocale( LC_ALL, "C.UTF-8" );
  fatal_assert( is_utf8_locale() );

  answers_a_transmission_with_ok();
  keeps_quiet_about_success_when_asked();
  answers_an_image_over_the_limit_with_efbig();
  refuses_files_so_programs_send_the_data_instead();
  keeps_quiet_about_errors_when_asked();
  answers_a_query_without_storing_its_image();
  answers_a_query_for_files_with_an_error();
  answers_a_chunked_transmission_once_it_is_complete();
  answers_nothing_without_an_image_id();
  tells_programs_that_direct_placements_are_not_supported();
  answers_a_new_command_after_an_abandoned_transmission();
  refuses_an_empty_image();
  return 0;
}
