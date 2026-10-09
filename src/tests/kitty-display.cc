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

/* Kitty images sent to the client in the diffs between terminal states. */

#include <locale.h>

#include <map>
#include <string>
#include <vector>

#include "completeterminal.h"
#include "fatal_assert.h"
#include "locale_utils.h"

/* One graphics command found in a diff. */
struct Command {
  std::map<std::string, std::string> keys;
  std::string payload;
};

/* The graphics commands in a diff, in order. */
static std::vector<Command> commands_in( const std::string &diff )
{
  std::vector<Command> commands;
  size_t start = diff.find( "\033_G" );
  while ( start != std::string::npos ) {
    const size_t end = diff.find( "\033\\", start );
    fatal_assert( end != std::string::npos );
    const std::string body = diff.substr( start + 3, end - start - 3 );
    const size_t semicolon = body.find( ';' );
    Command command;
    const std::string control = body.substr( 0, semicolon );
    size_t key = 0;
    while ( key < control.size() ) {
      size_t comma = control.find( ',', key );
      if ( comma == std::string::npos ) {
        comma = control.size();
      }
      const std::string pair = control.substr( key, comma - key );
      const size_t equals = pair.find( '=' );
      command.keys[ pair.substr( 0, equals ) ] = pair.substr( equals + 1 );
      key = comma + 1;
    }
    if ( semicolon != std::string::npos ) {
      command.payload = body.substr( semicolon + 1 );
    }
    commands.push_back( command );
    start = diff.find( "\033_G", end );
  }
  return commands;
}

static void sends_an_image_to_a_client_that_has_none( void )
{
  // Given
  const Terminal::Complete blank( 80, 24 );
  Terminal::Complete term( blank );
  term.act( "\033_Ga=t,i=7,f=100;iVBORw0KGgo=\033\\" );

  // When
  const std::vector<Command> commands = commands_in( term.diff_from( blank ) );

  // Then
  fatal_assert( commands.size() == 1 );
  std::map<std::string, std::string> keys = commands[ 0 ].keys;
  fatal_assert( keys[ "a" ] == "t" && keys[ "i" ] == "7" && keys[ "f" ] == "100" && keys[ "q" ] == "2"
                && ( keys[ "m" ].empty() || keys[ "m" ] == "0" ) && commands[ 0 ].payload == "iVBORw0KGgo=" );
}

static void sends_a_large_image_in_chunks_of_at_most_4096_bytes( void )
{
  // Given
  const Terminal::Complete blank( 80, 24 );
  Terminal::Complete term( blank );
  const std::string base64( 10000, 'A' );
  term.act( "\033_Ga=t,i=7,f=100;" + base64 + "\033\\" );

  // When
  std::vector<Command> commands = commands_in( term.diff_from( blank ) );

  // Then
  fatal_assert( commands.size() == 3 );
  std::string joined;
  for ( size_t i = 0; i < commands.size(); i++ ) {
    fatal_assert( commands[ i ].payload.size() <= 4096 );
    fatal_assert( commands[ i ].keys[ "m" ] == ( i + 1 < commands.size() ? "1" : "0" ) );
    fatal_assert( ( i == 0 ) == commands[ i ].keys.count( "i" ) );
    joined += commands[ i ].payload;
  }
  fatal_assert( commands[ 0 ].keys[ "a" ] == "t" && commands[ 0 ].keys[ "f" ] == "100" && joined == base64 );
}

static void sends_the_placement_after_the_image( void )
{
  // Given
  const Terminal::Complete blank( 80, 24 );
  Terminal::Complete term( blank );
  term.act( "\033_Ga=T,U=1,i=7,c=4,r=2,f=100;AAAA\033\\\033_Ga=p,U=1,i=7,p=3,c=10,r=5\033\\" );

  // When
  std::vector<Command> commands = commands_in( term.diff_from( blank ) );

  // Then
  fatal_assert( commands.size() == 2 && commands[ 0 ].keys[ "a" ] == "t" );
  std::map<std::string, std::string> keys = commands[ 1 ].keys;
  fatal_assert( keys[ "a" ] == "p" && keys[ "U" ] == "1" && keys[ "i" ] == "7" && keys[ "p" ] == "3"
                && keys[ "c" ] == "10" && keys[ "r" ] == "5" && keys[ "q" ] == "2" );
}

static void sends_nothing_for_images_the_client_has( void )
{
  // Given
  Terminal::Complete term( 80, 24 );
  term.act( "\033_Ga=T,U=1,i=7,c=4,r=2,f=100;AAAA\033\\" );
  const Terminal::Complete client( term );

  // When
  term.act( "hello" );

  // Then
  fatal_assert( commands_in( term.diff_from( client ) ).empty() );
}

static void sends_an_image_again_when_a_program_sends_it_again( void )
{
  // Given
  Terminal::Complete term( 80, 24 );
  term.act( "\033_Ga=t,i=7,f=100;AAAA\033\\" );
  const Terminal::Complete client( term );

  // When
  term.act( "\033_Ga=t,i=7,f=100;AAAA\033\\" );

  // Then
  std::vector<Command> commands = commands_in( term.diff_from( client ) );
  fatal_assert( commands.size() == 1 && commands[ 0 ].keys[ "a" ] == "t" );
}

static void tells_the_client_what_was_deleted( void )
{
  // Given
  Terminal::Complete term( 80, 24 );
  term.act( "\033_Ga=t,i=1,f=100;AAAA\033\\\033_Ga=T,U=1,i=2,c=1,r=1,f=100;BBBB\033\\" );
  const Terminal::Complete client( term );

  // When
  term.act( "\033_Ga=d,d=I,i=1\033\\\033_Ga=d,d=i,i=2\033\\" );

  // Then
  std::vector<Command> commands = commands_in( term.diff_from( client ) );
  fatal_assert( commands.size() == 2 );
  fatal_assert( commands[ 0 ].keys[ "a" ] == "d" && commands[ 0 ].keys[ "d" ] == "I"
                && commands[ 0 ].keys[ "i" ] == "1" && commands[ 0 ].keys[ "q" ] == "2" );
  fatal_assert( commands[ 1 ].keys[ "a" ] == "d" && commands[ 1 ].keys[ "d" ] == "i"
                && commands[ 1 ].keys[ "i" ] == "2" && commands[ 1 ].keys[ "q" ] == "2" );
}

static void sends_images_before_the_cells_that_show_them( void )
{
  // Given
  const Terminal::Complete blank( 80, 24 );
  Terminal::Complete term( blank );
  term.act( "hello\033_Ga=t,i=7,f=100;AAAA\033\\" );

  // When
  const std::string diff = term.diff_from( blank );

  // Then
  fatal_assert( diff.find( "\033_G" ) < diff.find( "hello" ) );
}

static void leaves_images_out_of_a_display_for_the_local_terminal( void )
{
  // Given
  const Terminal::Complete blank( 80, 24 );
  Terminal::Complete term( blank );
  term.act( "\033_Ga=T,U=1,i=7,c=1,r=1,f=100;AAAA\033\\" );
  const Terminal::Display local( false, false );

  // When
  const std::string frame = local.new_frame( true, blank.get_fb(), term.get_fb() );

  // Then
  fatal_assert( frame.find( "\033_G" ) == std::string::npos );
}

static bool same_images( const Terminal::Framebuffer &a, const Terminal::Framebuffer &b )
{
  const Terminal::Images::list_type &left = a.get_images().list(), &right = b.get_images().list();
  if ( left.size() != right.size() ) {
    return false;
  }
  for ( size_t i = 0; i < left.size(); i++ ) {
    const Terminal::Image &x = *left[ i ], &y = *right[ i ];
    if ( x.id != y.id || x.format != y.format || x.width != y.width || x.height != y.height
         || x.zlib != y.zlib || x.base64 != y.base64 || a.get_placement( x.id ) != b.get_placement( y.id ) ) {
      return false;
    }
  }
  return true;
}

static void gives_a_new_client_every_image_and_placement( void )
{
  // Given
  const Terminal::Complete blank( 80, 24 );
  Terminal::Complete term( blank );
  term.act( "\033_Ga=T,U=1,i=1,c=4,r=2,f=24,s=1,v=1,o=z;" + std::string( 9000, 'A' ) + "\033\\" );
  term.act( "\033_Ga=t,i=2,f=100;BBBB\033\\" );
  Terminal::Complete client( blank );

  // When
  client.act( term.diff_from( blank ) );

  // Then
  fatal_assert( same_images( client.get_fb(), term.get_fb() ) );
}

static void keeps_a_client_applying_images_from_replying( void )
{
  // Given
  const Terminal::Complete blank( 80, 24 );
  Terminal::Complete term( blank );
  term.act( "\033_Ga=T,U=1,i=1,c=4,r=2,f=100;" + std::string( 9000, 'A' ) + "\033\\" );
  const Terminal::Complete placed( term );
  term.act( "\033_Ga=d,d=i,i=1\033\\\033_Ga=t,i=2,f=100;BBBB\033\\\033_Ga=d,d=I,i=1\033\\" );
  Terminal::Complete client( blank );

  // When
  const std::string reply = client.act( placed.diff_from( blank ) ) + client.act( term.diff_from( placed ) );

  // Then
  fatal_assert( reply.empty() );
}

int main( void )
{
  /* mosh-server runs in a UTF-8 locale; the parser decodes input with it. */
  setlocale( LC_ALL, "C.UTF-8" );
  fatal_assert( is_utf8_locale() );

  sends_an_image_to_a_client_that_has_none();
  sends_a_large_image_in_chunks_of_at_most_4096_bytes();
  sends_the_placement_after_the_image();
  sends_nothing_for_images_the_client_has();
  sends_an_image_again_when_a_program_sends_it_again();
  tells_the_client_what_was_deleted();
  sends_images_before_the_cells_that_show_them();
  leaves_images_out_of_a_display_for_the_local_terminal();
  gives_a_new_client_every_image_and_placement();
  keeps_a_client_applying_images_from_replying();
  return 0;
}
