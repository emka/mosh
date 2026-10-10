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

/* Kitty images sent to the client in the diffs between terminal states,
   paced one slice per step. */

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

/* Base64 of the given length that differs from one offset to the next. */
static std::string base64_of( size_t length )
{
  static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string base64;
  for ( size_t i = 0; i < length; i++ ) {
    base64 += alphabet[ ( i * 7 + i / 64 ) % 64 ];
  }
  return base64;
}

static void sends_a_small_image_and_its_placement_in_one_step( void )
{
  // Given
  const Terminal::Complete blank( 80, 24 );
  Terminal::Complete term( blank );
  term.act( "\033_Ga=T,U=1,i=7,c=4,r=2,f=100;AAAA\033\\" );

  // When
  const bool stepped = term.step_images();

  // Then
  std::vector<Command> commands = commands_in( term.diff_from( blank ) );
  fatal_assert( stepped && commands.size() == 2 );
  fatal_assert( commands[ 0 ].keys[ "a" ] == "t" && commands[ 0 ].payload == "AAAA" );
  fatal_assert( commands[ 1 ].keys[ "a" ] == "p" && commands[ 1 ].keys[ "U" ] == "1" );
}

static void reports_no_change_when_the_view_has_caught_up( void )
{
  // Given
  Terminal::Complete term( 80, 24 );
  term.act( "\033_Ga=T,U=1,i=7,c=4,r=2,f=100;AAAA\033\\" );
  term.step_images();

  // When
  const bool stepped = term.step_images();

  // Then
  fatal_assert( !stepped );
}

static void sends_at_most_one_slice_of_a_large_image_in_the_first_step( void )
{
  // Given
  const Terminal::Complete blank( 80, 24 );
  Terminal::Complete term( blank );
  term.act( "\033_Ga=t,i=7,f=100;" + base64_of( 200000 ) + "\033\\" );

  // When
  term.step_images();

  // Then
  std::vector<Command> commands = commands_in( term.diff_from( blank ) );
  size_t sent = 0;
  for ( Command &command : commands ) {
    fatal_assert( command.payload.size() <= 4096 && command.keys[ "m" ] == "1" );
    sent += command.payload.size();
  }
  fatal_assert( sent > 0 && sent <= Terminal::ImageView::SLICE_BYTES );
  fatal_assert( commands[ 0 ].keys[ "a" ] == "t" && commands[ 0 ].keys[ "i" ] == "7" && commands[ 0 ].keys[ "f" ] == "100" );
}

/* The payloads of the given commands, joined. */
static std::string payload_of( const std::vector<Command> &commands )
{
  std::string payload;
  for ( const Command &command : commands ) {
    payload += command.payload;
  }
  return payload;
}

/* The graphics commands of each step's diff, until a step changes nothing. */
static std::vector< std::vector<Command> > step_until_caught_up( Terminal::Complete &term )
{
  std::vector< std::vector<Command> > diffs;
  for ( int i = 0; i < 1000; i++ ) {
    const Terminal::Complete last( term );
    if ( !term.step_images() ) {
      break;
    }
    diffs.push_back( commands_in( term.diff_from( last ) ) );
  }
  return diffs;
}

static void finishes_a_large_image_with_continuation_chunks_then_places_it( void )
{
  // Given
  const Terminal::Complete blank( 80, 24 );
  Terminal::Complete term( blank );
  const std::string base64 = base64_of( 200000 );
  term.act( "\033_Ga=T,U=1,i=7,c=4,r=2,f=100;" + base64 + "\033\\" );
  term.step_images();
  const std::string first = payload_of( commands_in( term.diff_from( blank ) ) );

  // When
  std::vector< std::vector<Command> > diffs = step_until_caught_up( term );

  // Then
  std::string rest;
  for ( size_t d = 0; d < diffs.size(); d++ ) {
    const bool final_diff = d + 1 == diffs.size();
    const size_t chunks = final_diff ? diffs[ d ].size() - 1 : diffs[ d ].size();
    for ( size_t c = 0; c < chunks; c++ ) {
      std::map<std::string, std::string> &keys = diffs[ d ][ c ].keys;
      fatal_assert( keys.size() == 2 && keys[ "q" ] == "2" );
      fatal_assert( keys[ "m" ] == ( final_diff && c + 1 == chunks ? "0" : "1" ) );
    }
    rest += payload_of( diffs[ d ] );
  }
  fatal_assert( diffs.size() > 1 && diffs.back().back().keys[ "a" ] == "p" );
  fatal_assert( first + rest == base64 );
}

static void sends_no_graphics_for_text_typed_during_an_upload( void )
{
  // Given
  Terminal::Complete term( 80, 24 );
  term.act( "\033_Ga=t,i=7,f=100;" + base64_of( 200000 ) + "\033\\" );
  term.step_images();
  const Terminal::Complete client( term );

  // When
  term.act( "hello" );

  // Then
  fatal_assert( commands_in( term.diff_from( client ) ).empty() );
}

/* Whether the commands place the image with the given id. */
static bool places( std::vector<Command> &commands, const std::string &id )
{
  for ( Command &command : commands ) {
    if ( command.keys[ "a" ] == "p" && command.keys[ "i" ] == id ) {
      return true;
    }
  }
  return false;
}

/* The index of the diff whose commands end an upload. */
static size_t completing( std::vector< std::vector<Command> > &diffs )
{
  for ( size_t d = 0; d < diffs.size(); d++ ) {
    for ( Command &command : diffs[ d ] ) {
      if ( command.keys[ "m" ] == "0" ) {
        return d;
      }
    }
  }
  return diffs.size();
}

static void places_another_image_only_after_the_upload_completes( void )
{
  // Given
  Terminal::Complete term( 80, 24 );
  term.act( "\033_Ga=t,i=2,f=100;AAAA\033\\" );
  term.step_images();
  term.act( "\033_Ga=t,i=1,f=100;" + base64_of( 200000 ) + "\033\\" );
  term.step_images();
  term.act( "\033_Ga=p,U=1,i=2,c=1,r=1\033\\" );

  // When
  std::vector< std::vector<Command> > diffs = step_until_caught_up( term );

  // Then
  const size_t done = completing( diffs );
  fatal_assert( done + 1 < diffs.size() );
  for ( size_t d = 0; d <= done; d++ ) {
    fatal_assert( !places( diffs[ d ], "2" ) );
  }
  fatal_assert( places( diffs[ done + 1 ], "2" ) );
}

static void restarts_the_upload_of_an_image_sent_again( void )
{
  // Given
  Terminal::Complete term( 80, 24 );
  term.act( "\033_Ga=t,i=1,f=100;" + base64_of( 200000 ) + "\033\\" );
  term.step_images();
  const std::string replacement( 150000, 'Q' );
  term.act( "\033_Ga=t,i=1,f=100;" + replacement + "\033\\" );

  // When
  std::vector< std::vector<Command> > diffs = step_until_caught_up( term );

  // Then
  fatal_assert( !diffs.empty() && !diffs[ 0 ].empty() );
  fatal_assert( diffs[ 0 ][ 0 ].keys[ "a" ] == "t" && diffs[ 0 ][ 0 ].keys[ "i" ] == "1" );
  std::string joined;
  for ( const std::vector<Command> &diff : diffs ) {
    joined += payload_of( diff );
  }
  fatal_assert( joined == replacement );
}

static void reports_a_change_when_the_image_in_flight_is_deleted( void )
{
  // Given
  Terminal::Complete term( 80, 24 );
  term.act( "\033_Ga=t,i=1,f=100;" + base64_of( 200000 ) + "\033\\" );
  term.step_images();
  term.act( "\033_Ga=d,d=I,i=1\033\\" );

  // When
  const bool stepped = term.step_images();

  // Then
  fatal_assert( stepped );
}

static void gives_a_new_client_complete_images_first_and_the_partial_upload_last( void )
{
  // Given
  const Terminal::Complete blank( 80, 24 );
  Terminal::Complete term( blank );
  term.act( "\033_Ga=T,U=1,i=2,c=1,r=1,f=100;AAAA\033\\" );
  term.step_images();
  const std::string base64 = base64_of( 200000 );
  term.act( "\033_Ga=t,i=1,f=100;" + base64 + "\033\\" );
  term.step_images();
  term.step_images();

  // When
  std::vector<Command> commands = commands_in( term.diff_from( blank ) );

  // Then
  fatal_assert( commands.size() > 3 );
  fatal_assert( commands[ 0 ].keys[ "a" ] == "t" && commands[ 0 ].keys[ "i" ] == "2" );
  fatal_assert( commands[ 1 ].keys[ "a" ] == "p" && commands[ 1 ].keys[ "i" ] == "2" );
  fatal_assert( commands[ 2 ].keys[ "a" ] == "t" && commands[ 2 ].keys[ "i" ] == "1" );
  for ( size_t c = 2; c < commands.size(); c++ ) {
    fatal_assert( commands[ c ].keys[ "m" ] == "1" );
  }
  const std::vector<Command> upload( commands.begin() + 2, commands.end() );
  fatal_assert( payload_of( upload ) == base64.substr( 0, 2 * Terminal::ImageView::SLICE_BYTES ) );
}

static void keeps_the_order_of_a_diff_that_spans_several_steps( void )
{
  // Given
  Terminal::Complete term( 80, 24 );
  term.act( "\033_Ga=t,i=2,f=100;AAAA\033\\" );
  term.step_images();
  const size_t large = 200000;
  term.act( "\033_Ga=t,i=1,f=100;" + base64_of( large ) + "\033\\" );
  term.step_images();
  const Terminal::Complete older( term );
  term.act( "\033_Ga=p,U=1,i=2,c=1,r=1\033\\\033_Ga=t,i=3,f=100;" + base64_of( large ) + "\033\\" );
  for ( size_t step = 0; step <= large / Terminal::ImageView::SLICE_BYTES; step++ ) {
    term.step_images();
  }

  // When
  std::vector<Command> commands = commands_in( term.diff_from( older ) );

  // Then
  size_t c = 0;
  while ( c < commands.size() && commands[ c ].keys.size() == 2 && commands[ c ].keys[ "m" ] == "1" ) {
    c++;
  }
  fatal_assert( c > 0 && c + 2 < commands.size() );
  fatal_assert( commands[ c ].keys.size() == 2 && commands[ c ].keys[ "m" ] == "0" );
  fatal_assert( commands[ c + 1 ].keys[ "a" ] == "p" && commands[ c + 1 ].keys[ "i" ] == "2" );
  fatal_assert( commands[ c + 2 ].keys[ "a" ] == "t" && commands[ c + 2 ].keys[ "i" ] == "3" );
  for ( c += 2; c < commands.size(); c++ ) {
    fatal_assert( commands[ c ].keys[ "m" ] == "1" );
  }
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

static void brings_a_client_applying_every_diff_to_the_terminal_images_without_replies( void )
{
  // Given
  const Terminal::Complete blank( 80, 24 );
  Terminal::Complete term( blank );
  term.act( "\033_Ga=T,U=1,i=1,c=1,r=1,f=100;AAAA\033\\" );
  term.act( "\033_Ga=T,U=1,i=2,c=4,r=2,f=32,s=10,v=10,o=z;" + base64_of( 300000 ) + "\033\\" );
  term.step_images();
  Terminal::Complete client( blank );
  std::string replies = client.act( term.diff_from( blank ) );
  term.act( "\033_Ga=d,d=I,i=1\033\\\033_Ga=t,i=3,f=100;" + base64_of( 100000 ) + "\033\\" );

  // When
  for ( int i = 0; i < 1000; i++ ) {
    const Terminal::Complete last( term );
    if ( !term.step_images() ) {
      break;
    }
    replies += client.act( term.diff_from( last ) );
  }

  // Then
  fatal_assert( same_images( client.get_fb(), term.get_fb() ) );
  fatal_assert( replies.empty() );
}

static void sends_nothing_for_images_the_client_has( void )
{
  // Given
  Terminal::Complete term( 80, 24 );
  term.act( "\033_Ga=T,U=1,i=7,c=4,r=2,f=100;AAAA\033\\" );
  term.step_images();
  const Terminal::Complete client( term );

  // When
  term.act( "hello" );
  term.step_images();

  // Then
  fatal_assert( commands_in( term.diff_from( client ) ).empty() );
}

static void sends_an_image_again_when_a_program_sends_it_again( void )
{
  // Given
  Terminal::Complete term( 80, 24 );
  term.act( "\033_Ga=t,i=7,f=100;AAAA\033\\" );
  term.step_images();
  const Terminal::Complete client( term );

  // When
  term.act( "\033_Ga=t,i=7,f=100;AAAA\033\\" );
  term.step_images();

  // Then
  std::vector<Command> commands = commands_in( term.diff_from( client ) );
  fatal_assert( commands.size() == 1 && commands[ 0 ].keys[ "a" ] == "t" );
}

static void tells_the_client_what_was_deleted( void )
{
  // Given
  Terminal::Complete term( 80, 24 );
  term.act( "\033_Ga=t,i=1,f=100;AAAA\033\\\033_Ga=T,U=1,i=2,c=1,r=1,f=100;BBBB\033\\" );
  term.step_images();
  const Terminal::Complete client( term );

  // When
  term.act( "\033_Ga=d,d=I,i=1\033\\\033_Ga=d,d=i,i=2\033\\" );
  term.step_images();

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
  term.step_images();

  // When
  const std::string diff = term.diff_from( blank );

  // Then
  fatal_assert( diff.find( "\033_G" ) < diff.find( "hello" ) );
}

static void never_draws_images_on_the_local_terminal( void )
{
  // Given
  const Terminal::Complete blank( 80, 24 );
  Terminal::Complete term( blank );
  term.act( "\033_Ga=T,U=1,i=7,c=1,r=1,f=100;AAAA\033\\" );
  const Terminal::Display local( false );

  // When
  const std::string frame = local.new_frame( true, blank.get_fb(), term.get_fb() );

  // Then
  fatal_assert( frame.find( "\033_G" ) == std::string::npos );
}

static void tells_the_client_to_drop_an_upload_deleted_in_flight( void )
{
  // Given
  Terminal::Complete term( 80, 24 );
  term.act( "\033_Ga=t,i=1,f=100;" + base64_of( 200000 ) + "\033\\" );
  term.step_images();
  const Terminal::Complete client( term );
  term.act( "\033_Ga=d,d=I,i=1\033\\" );

  // When
  term.step_images();

  // Then
  std::vector<Command> commands = commands_in( term.diff_from( client ) );
  fatal_assert( commands.size() == 1 );
  fatal_assert( commands[ 0 ].keys[ "a" ] == "d" && commands[ 0 ].keys[ "d" ] == "I" && commands[ 0 ].keys[ "i" ] == "1" );
}

int main( void )
{
  /* mosh-server runs in a UTF-8 locale; the parser decodes input with it. */
  setlocale( LC_ALL, "C.UTF-8" );
  fatal_assert( is_utf8_locale() );

  sends_a_small_image_and_its_placement_in_one_step();
  reports_no_change_when_the_view_has_caught_up();
  sends_at_most_one_slice_of_a_large_image_in_the_first_step();
  finishes_a_large_image_with_continuation_chunks_then_places_it();
  sends_no_graphics_for_text_typed_during_an_upload();
  places_another_image_only_after_the_upload_completes();
  restarts_the_upload_of_an_image_sent_again();
  reports_a_change_when_the_image_in_flight_is_deleted();
  tells_the_client_to_drop_an_upload_deleted_in_flight();
  gives_a_new_client_complete_images_first_and_the_partial_upload_last();
  keeps_the_order_of_a_diff_that_spans_several_steps();
  brings_a_client_applying_every_diff_to_the_terminal_images_without_replies();
  sends_nothing_for_images_the_client_has();
  sends_an_image_again_when_a_program_sends_it_again();
  tells_the_client_what_was_deleted();
  sends_images_before_the_cells_that_show_them();
  never_draws_images_on_the_local_terminal();
  return 0;
}
