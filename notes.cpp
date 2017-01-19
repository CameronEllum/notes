// notes.cpp : Defines the entry point for the console application.
//

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "boost/program_options.hpp"

#define TAGLIB_STATIC
#include "taglib\taglib.h"
#include "taglib\mpegfile.h"
#include "taglib\attachedpictureframe.h"
#include "taglib\textidentificationframe.h"
#include "taglib\id3v1tag.h"
#include "taglib\id3v2tag.h"
#include "taglib\mp4file.h"
#include "taglib\mp4tag.h"
#include "taglib\mp4coverart.h"

#ifdef _DEBUG
#  pragma comment(lib, "taglib/Debug/libtag.a")
#else
#  pragma comment(lib, "taglib/Release/libtag.a")
#endif

// cmake -G"Visual Studio 12 2013" -DCMAKE_INSTALL_PREFIX=C:\Libraries\taglib -DENABLE_STATIC=ON -DENABLE_STATIC_RUNTIME=ON

/*

https://github.com/gogglesmm/gogglesmm/blob/master/src/GMTag.cpp
https://bitbucket.org/lazka/mutagen/src/7c6543b03d64e834dc0e5202cae127f5231e9e31/mutagen/m4a.py?at=default
http://mutagen.readthedocs.org/en/latest/api/mp4.html
http://help.mp3tag.de/main_tags.html
http://taglib.github.io/api/classTagLib_1_1MP4_1_1File.html

 */

using namespace std;
using namespace boost;

int
ParseCommandLine(
   int argc,
   char* argv[],
   program_options::variables_map& vm);


//==============================================================================
class
IFile
{
public:
   virtual void SetTitle( const std::string& title ) = 0;
   virtual void SetTrack( int track, int total = -1 ) = 0;
   virtual void SetGenre( const std::string genre ) = 0;
   virtual void SetPerformers( const std::string& performers ) = 0;
   virtual void SetComposers( const std::string& composers ) = 0;
   virtual void SetYear( int year ) = 0;
   virtual void SetDisc( int disc, int total = -1 ) = 0;
   virtual void SetAlbumTitle( const std::string& title ) = 0;
   virtual void SetAlbumArtist( const std::string& title ) = 0;
   virtual void SetArt( std::string& data ) {}

   virtual void Write() = 0;
};

//==============================================================================
class
MP3File
   : public IFile
{
private:
   MP3File() {}
   virtual ~MP3File() {}

public:
   static MP3File* Open( const std::string& title )
   {
      auto file = new MP3File();

      file->file_ = make_unique<TagLib::MPEG::File>( title.c_str() );
      if ( !file->file_->audioProperties() )
      {
         delete file;
         return nullptr;
      }

      auto length = file->file_->audioProperties()->length();
      if ( length == 0 )
      {
         delete file;
         return nullptr;
      }

      file->file_->strip();

      return file;
   }

   virtual void SetTitle( const std::string& title )
   {
      file_->ID3v2Tag(true)->setTitle( title.c_str() );
   }

   virtual void SetTrack( int track, int total = -1 )
   {
      stringstream ss;
      ss << track;
      if ( total != -1 )
         ss << "/" << total;

      auto frame = new TagLib::ID3v2::TextIdentificationFrame( "TRCK", TagLib::String::Latin1 );
      frame->setText( ss.str().c_str() );
      file_->ID3v2Tag( true )->addFrame( frame );
   }

   virtual void SetGenre( const std::string genre )
   {
      file_->ID3v2Tag( true )->setGenre( genre.c_str() );
   }

   virtual void SetPerformers( const std::string& performers )
   {
      file_->ID3v2Tag( true )->setArtist( performers.c_str() );
   }

   virtual void SetComposers( const std::string& composers )
   {
      auto frame = new TagLib::ID3v2::TextIdentificationFrame("TCOM", TagLib::String::Latin1);
      frame->setText( composers.c_str() );
      file_->ID3v2Tag( true )->addFrame( frame );
   }

   virtual void SetYear( int year )
   {
      stringstream s;
      s << year;

      auto frame = new TagLib::ID3v2::TextIdentificationFrame( "TYER", TagLib::String::Latin1 );
      frame->setText( s.str().c_str() );
      file_->ID3v2Tag( true )->addFrame( frame );

      file_->ID3v2Tag( true )->setYear( year );
   }

   virtual void SetDisc( int disc, int total = -1 )
   {
      stringstream ss;
      ss << disc;
      if ( total != -1 )
         ss << "/" << total;

      auto frame = new TagLib::ID3v2::TextIdentificationFrame( "TPOS", TagLib::String::Latin1 );
      frame->setText( ss.str().c_str() );
      file_->ID3v2Tag( true )->addFrame( frame );
   }

   virtual void SetAlbumTitle( const std::string& title )
   {
      file_->ID3v2Tag( true )->setAlbum( title.c_str() );
   }

   virtual void SetAlbumArtist( const std::string& artist )
   {
      auto frame = new TagLib::ID3v2::TextIdentificationFrame( "TPE2", TagLib::String::Latin1 );
      frame->setText( artist.c_str() );
      file_->ID3v2Tag( true )->addFrame( frame );
   }

   virtual void SetArt( std::string& data )
   {
      TagLib::ID3v2::AttachedPictureFrame *frame = new TagLib::ID3v2::AttachedPictureFrame;

      TagLib::ByteVector bv( data.c_str(), data.length() );

      frame->setTextEncoding( TagLib::String::Type::Latin1 );
      frame->setMimeType( "image/jpeg" );
      frame->setDescription( "Cover" );
      frame->setType( TagLib::ID3v2::AttachedPictureFrame::FrontCover );
      frame->setPicture( bv );

      file_->ID3v2Tag( true )->addFrame( frame );
   }

   virtual void Write()
   {
      file_->save( TagLib::MPEG::File::ID3v2, false, 3 );
   }

private:
   std::unique_ptr<TagLib::MPEG::File> file_;
};

//==============================================================================
class
AACFile
   : public IFile
{
private:
   AACFile() {}
   virtual ~AACFile() {}

public:
   static AACFile* Open( const std::string& title )
   {
      auto file = new AACFile();

      file->file_ = make_unique<TagLib::MP4::File>( title.c_str(), true );

      if ( !file->file_->audioProperties() )
      {
         delete file;
         return NULL;
      }

      auto length = file->file_->audioProperties()->length();
      if ( length == 0 )
      {
         delete file;
         return NULL;
      }

      auto tag = file->file_->tag();
      if ( !tag )
      {
         delete file;
         return NULL;
      }

      file->file_->tag()->itemListMap().clear();

      return file;
   }

   virtual void SetTitle( const std::string& title )
   {
      file_->tag()->setTitle( title.c_str() );
   }

   virtual void SetTrack( int track, int total = -1 )
   {
      if ( total < 0 )
         total = 0;

      file_->tag()->itemListMap().insert( "trkn", 
         TagLib::MP4::Item( track, total ) );
   }

   virtual void SetGenre( const std::string genre )
   {
      file_->tag()->setGenre( genre.c_str() );
   }

   virtual void SetPerformers( const std::string& performers )
   {
      file_->tag()->setArtist( performers.c_str() );
   }

   virtual void SetComposers( const std::string& composers )
   {
      file_->tag()->itemListMap().insert( "\251wrt", 
         TagLib::StringList( TagLib::String( composers.c_str(), TagLib::String::Latin1 ) ) );
   }

   virtual void SetYear( int year )
   {
      file_->tag()->setYear( year );
   }

   virtual void SetDisc( int disc, int total = -1 )
   {
      if ( total < 0 )
         total = 0;

      file_->tag()->itemListMap().insert( "disk", 
         TagLib::MP4::Item( disc, total ) );
   }

   virtual void SetAlbumTitle( const std::string& title )
   {
      file_->tag()->setAlbum( title.c_str() );
   }

   virtual void SetAlbumArtist( const std::string& artist )
   {
      file_->tag()->itemListMap().insert( "aART", 
         TagLib::StringList( TagLib::String( artist.c_str(), TagLib::String::Latin1 ) ) );
   }

   virtual void SetArt( std::string& data )
   {
      // From: http://stackoverflow.com/questions/4752020/how-do-i-use-taglib-to-read-write-coverart-in-different-audio-formats

      TagLib::ByteVector bv( data.c_str(), data.length() );
      TagLib::MP4::CoverArt coverArt( TagLib::MP4::CoverArt::JPEG, bv );

      TagLib::MP4::CoverArtList coverArtList;
      coverArtList.append( coverArt );

      TagLib::MP4::Item coverItem( coverArtList );

      file_->tag()->itemListMap().insert( "covr", 
         TagLib::MP4::Item( coverArtList ) );
   }

   virtual void Write()
   {
      file_->save();
   }

private:
   std::unique_ptr<TagLib::MP4::File> file_;
};


//------------------------------------------------------------------------------
int 
main(
   int argc, 
   char* argv[])
{
   program_options::variables_map vm;

   ParseCommandLine(argc, argv, vm);

   if (vm.count("input-file") == 0)
   {
      std::cerr << "No input file specified!\n";
      return EXIT_FAILURE;
   }

   const std::string& inputPath = vm["input-file"].as< string >();

   cout << "Input file: " << inputPath << "\n";

   IFile* file = nullptr;

#if 0 
   // Doesn't work: some MP4 files are identified as MP3 files
   file = MP3File::Open( inputPath );
   if ( !file )
      file = AACFile::Open( inputPath );
#endif

   const std::string& extension = inputPath.substr( inputPath.length() - 3 );
   if ( extension == "mp3" )
      file = MP3File::Open( inputPath );
   else if ( extension == "m4a" )
      file = AACFile::Open( inputPath );

   if ( !file )
   {
      std::cerr << "Unable to open or determine file type!\n";
      return EXIT_FAILURE;
   }

   if ( vm.count( "title" ) )
      file->SetTitle( vm["title"].as< string >() );
   if ( vm.count( "track" ) )
   {
      int track=-1, total=-1;
      stringstream ss( vm["track"].as< string >() );
      ss >> track;
      ss.ignore(1);
      ss >> total;

      file->SetTrack( track, total );
   }
   if ( vm.count( "performers" ) )
      file->SetPerformers( vm["performers"].as< string >() );
   if ( vm.count( "composers" ) )
      file->SetComposers( vm["composers"].as< string >() );
   if ( vm.count( "genre" ) )
      file->SetGenre( vm["genre"].as< string >() );
   if ( vm.count( "year" ) )
      file->SetYear( vm["year"].as< int >() );

   if ( vm.count( "album-title" ) )
      file->SetAlbumTitle( vm["album-title"].as< string >() );
   if ( vm.count( "album-artist" ) )
      file->SetAlbumArtist( vm["album-artist"].as< string >() );
   if ( vm.count( "disc" ) )
   {
      int disc = -1, total = 1;
      stringstream ss( vm["disc"].as< string >() );
      ss >> disc;
      ss.ignore( 1 );
      ss >> total;

      if ( disc > total )
         total = -1;

      file->SetDisc( disc, total );
   }

   if ( vm.count( "art" ) )
   {
      const std::string& coverFilePath = vm["art"].as< string >();

      std::ifstream ifs( coverFilePath, std::ios_base::binary );
      if ( !ifs )
      {
         std::cerr << "Unable to open cover art file \"" << coverFilePath << "\"!\n";
         return 0;
      }

      // Copy image data into a string. Not sure why I used a string for storage.
      std::string content( 
         (std::istreambuf_iterator<char>( ifs )),
         (std::istreambuf_iterator<char>()) );

      file->SetArt( content );
   }

   file->Write();

   return EXIT_SUCCESS;
}

//------------------------------------------------------------------------------
static int
ParseCommandLine(
   int argc,
   char* argv[],
   program_options::variables_map& vm )
{
   using namespace boost::program_options;

   // Declare the supported options.
   options_description desc("Allowed options");
   desc.add_options()
      ("help", "produce help message")

      ("strip,s", "Strip existing tags")

      ("title,t", value<string>(), "Title")
      ("track,n", value<string>(), "Track")
      ("performers,p", value<string>(), "Performer(s)")
      ("composers,c", value<string>(), "Composer(s)")
      ("genre,g", value<string>(), "Genre")
      ("year,y", value<int>(), "Year")
      ("disc,d", value<string>(), "Disc")

      ("album-title,a", value<string>(), "Album title")
      ("album-artist,r", value<string>(), "Album artist")
      ("art", value<string>(), "Art (cover)")

      ("input-file,i", value<string>(), "Input file")
      ;

   positional_options_description p;
   p.add("input-file", -1);

   try
   {
      store( command_line_parser(argc, argv).
         options(desc).positional(p).run(), vm);   
      notify(vm);
   }
   catch( std::exception& e )
   {
      cerr << e.what();
   }

   if (vm.count("help")) 
   {
      cout << desc << "\n";
      return 1;
   }

   return 1;
}