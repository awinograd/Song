/*
 * example sketch to play audio file(s) in a directory, using the mp3 library
 * for playback and the arduino sd library to read files from a microsd card.
 * pins are setup to work well for teensy 2.0. double-check if using arduino.
 * 
 * originally based on frank zhao's player: http://frank.circleofcurrent.com/
 * utilities adapted from previous versions of the functions by matthew seal.
 *
 * (c) 2011 david sirkin sirkin@cdr.stanford.edu
 *          akil srinivasan akils@stanford.edu
 */

// first step is to include (arduino) sd, eeprom, and (our own) mp3 and lcd libraries.

#include <SD.h>
#include <EEPROM.h>

#include <mp3.h>
#include <mp3conf.h>
#include <Song.h>

// setup microsd, decoder, and lcd chip pins

#define sd_cs         12         // 'chip select' for microsd card
#define mp3_cs        21         // 'command chip select' to cs pin

#define dcs           20         // 'data chip select' to bsync pin
#define rst           18         // 'reset' to decoder's reset pin
#define dreq          19         // 'data request line' to dreq pin

// read_buffer is the amount of data read from microsd, then sent to decoder.

#define read_buffer   512        // size of the microsd read buffer
#define mp3_vol       175        // default volume: 0=min, 254=max
#define MAX_VOL       254

#define EEPROM_INIT_ID    33
#define EEPROM_FIRSTRUN 0
#define EEPROM_VOLUME   1
#define EEPROM_TRACK    2
#define EEPROM_STATE    3

// file names are 13 bytes max (8 + '.' + 3 + '\0'), and the file list should
// fit into the eeprom. for example, 13 * 40 = 520 bytes of eeprom are needed
// to store a list of 40 songs. if you use shorter file names, or if your mcu
// has more eeprom, you can change these.

#define FILE_NAMES_START 32 //leave some room for persisting play info (vol, track, etc.)
#define max_name_len  13
#define max_num_songs 40

// next steps, declare the variables used later to represent microsd objects.

Sd2Card  card;                   // top-level represenation of card
SdVolume volume;                 // sd partition, not audio volume
SdFile   sd_root, sd_file;       // sd_file is the child of sd_root

// store the number of songs in this directory, and the current song to play.

unsigned char num_songs = 0, current_song = 0;

// an array to hold the current_song's file name in ram. every file's name is
// stored longer-term in the eeprom. this array is used for sd_file.open().

char fn[max_name_len];

// the program runs as a state machine. the 'state' enum includes the states.
// 'current_state' is the default as the program starts. add new states here.

enum state { 
  DIR_PLAY, MP3_PLAY, IDLE };
state current_state = DIR_PLAY;
state last_state = DIR_PLAY;

bool repeat = true;

// you must open any song file that you want to play using sd_file_open prior
// to fetching song data from the file. you can only open one file at a time.

int mp3Volume = mp3_vol;

void Song::sd_file_open() {
	sd_file.close();
  map_current_song_to_fn();
  sd_file.open(&sd_root, fn, FILE_READ);

  /*sd_file.seekSet(getFileSize() - 128);

  struct TAGdata* tag = new struct TAGdata;
  sd_file.read(tag, 128);
  Serial.println(tag->title);*/

  // if you prefer to work with the current song index (only) instead of file
  // names, this version of the open command should also work for you:

  //sd_file.open(&sd_root, current_song, FILE_READ);
}

bool Song::nextFileExists(){
  if (current_song < (num_songs - 1) || repeat){
    return true; 
  }
  return false;
}

bool Song::nextFile(){
  if (!nextFileExists()){
	  return false;
  }

  current_song++;// = (current_song + 1) % num_songs; 
  sd_file_open();

  EEPROM.write(EEPROM_TRACK, current_song);
  return true;
}

bool Song::prevFileExists(){
  if (current_song > 0){
    return true; 
  }
  return false;
}

bool Song::prevFile(){
  if (!prevFileExists()) {
	  return false;
  }

  current_song--;
  sd_file_open();

  EEPROM.write(EEPROM_TRACK, current_song);
  return true;
}

void Song::mp3_play() {
  unsigned char bytes[read_buffer]; // buffer to read and send to the decoder
  unsigned int bytes_to_read;       // number of bytes read from microsd card

    // send read_buffer bytes to be played. Mp3.play() tracks the index pointer
  // within the song being played of where to get the next read_buffer bytes.

  bytes_to_read = sd_file.read(bytes, read_buffer);
  Mp3.play(bytes, bytes_to_read);

  // bytes_to_read should only be less than read_buffer when the song's over.

  if(bytes_to_read < read_buffer) {
    sd_file.close();
    current_state = IDLE;
  }
}

int Song::getFileSize(){
	return sd_file.fileSize();
}

int Song::seek(int percent) {
  if (percent < 0 || percent > 100) return 0;
  uint16_t size = sd_file.fileSize();
  uint16_t seekPos = percent * (getFileSize() / 100);
  bool seeked = sd_file.seekSet(seekPos);  
  Serial.println(seeked);
  Serial.println(percent);
  Serial.println(seekPos);
  Serial.println(getFileSize());
  return seekPos;
}

// continue to play the current (playing) song, until there are no more songs
// in the directory to play. 

void Song::dir_play() {
  if (current_song < num_songs) {
    mp3_play();

    // if current_state is IDLE, then the currently playing song just ended.
    // in that case, increment to get the next song to play, open that file,
    // and return to the DIR_PLAY state (which will then play that song).
    // if we played the last part of the last song, we don't do anything,
    // and the current_state is already set to IDLE from mp3_play()

    if (current_state == IDLE && nextFileExists()) {
      nextFile();
      current_state = DIR_PLAY;
    }
  }
}

bool Song::isPlaying(){
	return current_state == MP3_PLAY || current_state == DIR_PLAY;
}

double Song::setVolume(int volume_percentage){
	double vol = volume_percentage /100.0;
	double vol2 = pow(2.7182818, vol) * 93.8;
	mp3Volume = vol2;
	Mp3.volume(mp3Volume);
	EEPROM.write(EEPROM_VOLUME, volume_percentage);
	return mp3Volume;
}

int Song::getVolume(){
	double toReturn = mp3Volume/ 93.8;
	toReturn = log(toReturn) * 100;
	return toReturn;
}

// setup is pretty straightforward. initialize serial communication (used for
// the following error messages), microsd card objects, mp3 library, and open
// the first song in the root library to play.

Song::Song() {
}

void Song::initPlayerStateFromEEPROM(){
  if (EEPROM.read(EEPROM_FIRSTRUN) == EEPROM_INIT_ID){
	//read persisted states from EEPROM
	mp3Volume = EEPROM.read(EEPROM_VOLUME);
	current_song = EEPROM.read(EEPROM_TRACK) - 1;
	current_state = (state)EEPROM.read(EEPROM_STATE);
	Serial.println("Reading player state from EEPROM");
	Serial.print("Volume: ");
	Serial.println(mp3Volume);
	Serial.print("Song: ");
	Serial.println(current_song);
	Serial.print("State: ");
	Serial.println(current_state);
  }
  else{
	  mp3Volume = mp3_vol;
	  current_song = -1;
	  current_state = DIR_PLAY;
	  EEPROM.write(EEPROM_FIRSTRUN, EEPROM_INIT_ID);
	  EEPROM.write(EEPROM_VOLUME, mp3Volume);
	  EEPROM.write(EEPROM_TRACK, current_song);
	  EEPROM.write(EEPROM_STATE, current_state);
	  Serial.println("First run: Initializing EEPROM state");
  }
}

void Song::setup(){
  Serial.begin(9600);
  //move current_song to -1 so that nextFile() will start at file 0
  //current_song = 0;

  initPlayerStateFromEEPROM();

  pinMode(SS_PIN, OUTPUT);  //SS_PIN must be output to use SPI

  // the default state of the mp3 decoder chip keeps the SPI bus from 
  // working with other SPI devices, so we have to initialize it first.
  Mp3.begin(mp3_cs, dcs, rst, dreq);

  // initialize the microsd (which checks the card, volume and root objects).
  sd_card_setup();

  // initialize the mp3 library, and set default volume. 'mp3_cs' is the chip
  // select, 'dcs' is data chip select, 'rst' is reset and 'dreq' is the data
  // request. the decoder raises the dreq line (automatically) to signal that
  // it's input buffer can accommodate 32 more bytes of incoming song data.
  // we need to set the SPI speed with the mp3 initialize function since
  // it is the limiting factor, so we call its init function again.

  Mp3.begin(mp3_cs, dcs, rst, dreq);
  setVolume(mp3Volume);

  // putting all of the root directory's songs into eeprom saves flash space.

  sd_dir_setup();

  // the program is setup to enter DIR_PLAY mode immediately, so this call to
  // open the root directory before reaching the state machine is needed.

  sd_file_open();
  Serial.println("Song setup");
}

void Song::pause(){
	if (current_state != IDLE){
		last_state = current_state;
		current_state = IDLE;
		EEPROM.write(EEPROM_STATE, current_state);
	}
}

void Song::play(){
	if (current_state == IDLE){
		//set current_state to last_state unless last_state was also IDLE, then set to DIR_PLAY
		current_state = last_state != IDLE ? last_state : DIR_PLAY;
		EEPROM.write(EEPROM_STATE, current_state);
	}
}

// the state machine is setup (at least, at first) to open the microsd card's
// root directory, play all of the songs within it, close the root directory,
// and then stop playing. change these, or add new actions here.

// the DIR_PLAY state plays all of the songs in a directory and then switches
// into IDLE when done. the MP3_PLAY state plays one specified song, and then
// switches into IDLE. this example program doesn't enter the MP3_PLAY state,
// as its goal (for now) is just to play all the songs. you can change that.

void Song::loop() {
  switch(current_state) {

  case DIR_PLAY:
    dir_play();
    break;

  case MP3_PLAY:
    mp3_play();
    break;

  case IDLE:
    break;
  }
}




















/*
 * example sketch to play audio file(s) in a directory, using the mp3 library
 * for playback and the arduino sd library to read files from a microsd card.
 * pins are setup to work well for teensy 2.0. double-check if using arduino.
 * 
 * originally based on frank zhao's player: http://frank.circleofcurrent.com/
 * utilities adapted from previous versions of the functions by matthew seal.
 *
 * (c) 2011 david sirkin sirkin@cdr.stanford.edu
 *          akil srinivasan akils@stanford.edu
 */

// check that the microsd card is present, can be initialized and has a valid
// root volume. a pointer to the card's root object is returned as sd_root.

void Song::sd_card_setup() {
  if (!card.init(SPI_HALF_SPEED, sd_cs)) {
    Serial.println("Card found, but initialization failed.");
    return;
  }
  if (!volume.init(card)) {
    Serial.println("Initialized, but couldn't find partition.");
    return;
  }
  if (!sd_root.openRoot(&volume)) {
    Serial.println("Partition found, but couldn't open root");
    return;
  }
}

// for each song file in the current directory, store its file name in eeprom
// for later retrieval. this saves on using program memory for the same task,
// which is helpful as you add more functionality to the program. it also allows
// users to change the songs on the SD card, and not have to change the code to
// play new songs. if you would like to store subdirectories, talk to an 
// instructor.

void Song::sd_dir_setup() {
  dir_t p;
  num_songs = 0;
  
  sd_root.rewind();
  
  while (sd_root.readDir(&p) > 0 && num_songs < max_num_songs) {
    // break out of while loop when we wrote all files (past the last entry).

    if (p.name[0] == DIR_NAME_FREE) {
      break;
    }
    
    // only store current (not deleted) file entries, and ignore the . and ..
    // sub-directory entries. also ignore any sub-directories.
    
    if (p.name[0] == DIR_NAME_DELETED || p.name[0] == '.' || !DIR_IS_FILE(&p)) {
      continue;
    }

    // only store mp3 or wav files in eeprom (for now). if you add other file
    // types, you should add their extension here.

    // it's okay to hard-code the 8, 9 and 10 as indices here, since SdFatLib
    // pads shorter file names with a ' ' to fill 8 characters. the result is
    // that file extensions are always stored in the last 3 positions.
    
    if ((p.name[8] == 'M' && p.name[9] == 'P' && p.name[10] == '3') ||
        (p.name[8] == 'W' && p.name[9] == 'A' && p.name[10] == 'V')) {

      // store each character of the file name into an individual byte in the
      // eeprom. sd_file->name doesn't return the '.' part of the name, so we
      // add that back later when we read the file from eeprom.
    
      unsigned char pos = 0;

      for (unsigned char i = 0; i < 11; i++) {
        if (p.name[i] != ' ') {
          EEPROM.write(FILE_NAMES_START + num_songs * max_name_len + pos, p.name[i]);
          pos++;
        }
      }
    
      // add an 'end of string' character to signal the end of the file name.
    
      EEPROM.write(FILE_NAMES_START + num_songs * max_name_len + pos, '\0');
      num_songs++;
    }
  }
}

char* Song::getCurrentSong(){
	return fn;
}

// given the numerical index of a particular song to play, go to its location
// in eeprom, retrieve its file name and set the global variable 'fn' to it.

void Song::map_current_song_to_fn() {
  int null_index = max_name_len - 1;
  
  // based on the current_song index, get song's name and null index position from eeprom.
  
  for (int i = 0; i < max_name_len; i++) {
    fn[i] = EEPROM.read(FILE_NAMES_START + current_song * max_name_len + i);
    
    // break if we reach the end of the file name.
    // keep track of the null index position, so we can put the '.' back.
    
    if (fn[i] == '\0') {
      null_index = i;
      break;
    }
  }
  
  // now restore the '.' that dir_t->name didn't store in its array for us.
  
  if (null_index > 3) {
    fn[null_index + 1] = '\0';
    fn[null_index]     = fn[null_index - 1];
    fn[null_index - 1] = fn[null_index - 2];
    fn[null_index - 2] = fn[null_index - 3];
    fn[null_index - 3] = '.';
  }
}