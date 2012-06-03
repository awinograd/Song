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

// id3v2 tags have variable-length song titles. that length is indicated in 4
// bytes within the tag. id3v1 tags also have variable-length song titles, up
// to 30 bytes maximum, but the length is not indicated within the tag. using
// 60 bytes here is a compromise between holding most titles and saving sram.

// if you increase this above 255, look for and change 'for' loop index types
// so as to not to overflow the unsigned char data type.

#define max_title_len 60

// next steps, declare the variables used later to represent microsd objects.

Sd2Card  card;                   // top-level represenation of card
SdVolume volume;                 // sd partition, not audio volume
SdFile   sd_root, sd_file;       // sd_file is the child of sd_root

// store the number of songs in this directory, and the current song to play.

unsigned char num_songs = 0, current_song = 0;

// an array to hold the current_song's file name in ram. every file's name is
// stored longer-term in the eeprom. this array is used for sd_file.open().

char fn[max_name_len];

// an array to hold the current_song's title in ram. it needs 1 extra char to
// hold the '\0' that indicates the end of a character string. the song title
// is found in get_title_from_id3tag().

char title[max_title_len + 1];

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
	get_title_from_id3tag();
	return title;
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

// this utility function reads id3v1 and id3v2 tags, if any are present, from
// mp3 audio files. if no tags are found, just use the title of the file. :-|

void Song::get_title_from_id3tag() {
	Serial.println("-----------id3------------");
  unsigned char id3[3];       // pointer to the first 3 characters to read in

  // visit http://www.id3.org/id3v2.3.0 to learn all(!) about the id3v2 spec.
  // move the file pointer to the beginning, and read the first 3 characters.

  sd_file.seekSet(0);
  sd_file.read(id3, 3);
  
  // if these first 3 characters are 'ID3', then we have an id3v2 tag. if so,
  // a 'TIT2' (for ver2.3) or 'TT2' (for ver2.2) frame holds the song title.

  if (id3[0] == 'I' && id3[1] == 'D' && id3[2] == '3') {
    unsigned char pb[4];       // pointer to the last 4 characters we read in
    unsigned char c;           // the next 1 character in the file to be read
    
    // our first task is to find the length of the (whole) id3v2 tag. knowing
    // this means that we can look for 'TIT2' or 'TT2' frames only within the
    // tag's length, rather than the entire file (which can take a while).

    // skip 3 bytes (that we don't use), then read in the last 4 bytes of the
    // header, which contain the tag's length.

    sd_file.read(pb, 3);
    sd_file.read(pb, 4);
    
    // to combine these 4 bytes together into the single value, we first have
    // to shift each one over to get it into its correct 'digits' position. a
    // quirk of the spec is that bit 7 (the msb) of each byte is set to 0.
    
    unsigned long v2l = ((unsigned long) pb[0] << (7 * 3)) +
                        ((unsigned long) pb[1] << (7 * 2)) +
                        ((unsigned long) pb[2] << (7 * 1)) + pb[3];
                        
    // we just moved the file pointer 10 bytes into the file, so we reset it.
    
    sd_file.seekSet(0);

    while (1) {
      // read in bytes of the file, one by one, so we can check for the tags.
      
      sd_file.read(&c, 1);

      // keep shifting over previously-read bytes as we read in each new one.
      // that way we keep testing if we've found a 'TIT2' or 'TT2' frame yet.
      
      pb[0] = pb[1];
      pb[1] = pb[2];
      pb[2] = pb[3];
      pb[3] = c;

	  //if (pb[0] == 'T' && pb[1] == 'P' && pb[2] == 'E' && pb[3] == '1') {
      if (pb[0] == 'T' && pb[1] == 'I' && pb[2] == 'T' && pb[3] == '2') {
		  Serial.println("TIT2");
        // found an id3v2.3 frame! the title's length is in the next 4 bytes.
        
        sd_file.read(pb, 4);

        // only the last of these bytes is likely needed, as it can represent
        // titles up to 255 characters. but to combine these 4 bytes together
        // into the single value, we first have to shift each one over to get
        // it into its correct 'digits' position. 

        unsigned long tl = ((unsigned long) pb[0] << (8 * 3)) +
                           ((unsigned long) pb[1] << (8 * 2)) +
                           ((unsigned long) pb[2] << (8 * 1)) + pb[3];
        tl--;
        Serial.println(tl);
        // skip 2 bytes (we don't use), then read in 1 byte of text encoding. 

        sd_file.read(pb, 2);
        sd_file.read(&c, 1);
        
        // if c=1, the title is in unicode, which uses 2 bytes per character.
        // skip the next 2 bytes (the byte order mark) and decrement tl by 2.
        
        if (c) {
          sd_file.read(pb, 2);
          tl -= 2;
        }
        // remember that titles are limited to only max_title_len bytes long.
        Serial.println(tl);
        if (tl > max_title_len) tl = max_title_len;
        
        // read in tl bytes of the title itself. add an 'end-of-string' byte.

        sd_file.read(title, tl);

		// at odd indices title seems to have '\0's so let's get rid of them
		for(int i = 0; i < tl; i++){
			if (i % 2 == 1) continue;
			title[i/2] = title[i];
			//Serial.print(title[i]);
		}

		//add a null terminator at the new end of the title
		title[tl/2] = '\0';
        break;
      }
      else
      if (pb[1] == 'T' && pb[2] == 'T' && pb[3] == '2') {
		  Serial.println("TT2");
        // found an id3v2.2 frame! the title's length is in the next 3 bytes,
        // but we read in 4 then ignore the last, which is the text encoding.
        
        sd_file.read(pb, 4);
        
        // shift each byte over to get it into its correct 'digits' position. 
        
        unsigned long tl = ((unsigned long) pb[0] << (8 * 2)) +
                           ((unsigned long) pb[1] << (8 * 1)) + pb[2];
        tl--;
        
        // remember that titles are limited to only max_title_len bytes long.

        if (tl > max_title_len) tl = max_title_len;

        // there's no text encoding, so read in tl bytes of the title itself.
        
        sd_file.read(title, tl);
        title[tl] = '\0';
        break;
      }
      else
      if (sd_file.curPosition() == v2l) {
		  Serial.println("EOT");
        // we reached the end of the id3v2 tag. use the file name as a title.

        strncpy(title, fn, max_name_len);
        break;
      }
    }
  }
  else {
    // the file doesn't have an id3v2 tag so search for an id3v1 tag instead.
    // an id3v1 tag begins with the 3 characters 'TAG'. if these are present,
    // then they are located exactly 128 bits from the end of the file.
    
    sd_file.seekSet(sd_file.fileSize() - 128);
    sd_file.read(id3, 3);
    
    if (id3[0] == 'T' && id3[1] == 'A' && id3[2] == 'G') {
		Serial.println("TAG");
      // found it! now read in the full title, which is always 30 bytes long.
      
      sd_file.read(title, 30);
      
      // strip spaces and non-printable characters from the end of the title.
      // you may have to expand this range to incorporate unicode characters.
      
      for (char i = 30 - 1; i >= 0; i--) {
        if (title[i] <= ' ' || title[i] > 126) {
          title[i] = '\0';
        }
        else {
          break;
        }
      }
    }
    else {
      // we reached the end of the id3v1 tag. use the file name as a title.
      
      strncpy(title, fn, max_name_len);
    }
  }
}