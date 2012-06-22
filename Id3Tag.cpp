#include <SD.h>
#include <Id3Tag.h>

#define FILE_NAMES_START 32 //leave some room for persisting play info (vol, track, etc.)
#define max_name_len  13
#define max_num_songs 40

// id3v2 tags have variable-length song titles. that length is indicated in 4
// bytes within the tag. id3v1 tags also have variable-length song titles, up
// to 30 bytes maximum, but the length is not indicated within the tag. using
// 60 bytes here is a compromise between holding most titles and saving sram.

//allow file scanning to end early if all tags found
#define MAX_NUM_TAGS 3

#define BUFF_SIZE 400
#define max_title_len 60
#define max_artist_len 30
#define max_album_len 40
#define max_year_len 4
#define max_time_len 10

extern char fn[max_name_len];

// an array to hold the current_song's title in ram. it needs 1 extra char to
// hold the '\0' that indicates the end of a character string. the song title
// is found in get_title_from_id3tag().

char title[max_title_len + 1];
char artist[max_artist_len + 1];
char album[max_album_len + 1];
char time[max_time_len + 1];

//enum tagType { ID3v2, ID3v1, None }

Id3Tag::Id3Tag(){
}

char* Id3Tag::getTitle(){
	return title;
}

char* Id3Tag::getArtist(){
	return artist;
}

char* Id3Tag::getAlbum(){
	return album;
}

char* Id3Tag::getTime(){
	return time;
}

// this utility function reads id3v1 and id3v2 tags, if any are present, from
// mp3 audio files. if no tags are found, just use the title of the file. :-|

void Id3Tag::getId3Tag(SdFile* sd_file, char* value, unsigned char pb[], unsigned char c, int j){
	uint32_t origPos = sd_file->curPosition();
	sd_file->seekCur(-j);
	//Serial.println("getId3Tag");
	//Serial.println(sd_file->curPosition());
	// found an id3v2.3 frame! the length is in the next 4 bytes.
        
    sd_file->read(pb, 4);

    // only the last of these bytes is likely needed, as it can represent
    // titles up to 255 characters. but to combine these 4 bytes together
    // into the single value, we first have to shift each one over to get
    // it into its correct 'digits' position. 

    unsigned long tl = ((unsigned long) pb[0] << (8 * 3)) +
                        ((unsigned long) pb[1] << (8 * 2)) +
                        ((unsigned long) pb[2] << (8 * 1)) + pb[3];
    tl--;
    
    // skip 2 bytes (header flags that we don't use), then read in 1 byte of text encoding. 

    sd_file->read(pb, 2);
    sd_file->read(&c, 1);
        
    // if c=1, the title is in unicode, which uses 2 bytes per character.
    // skip the next 2 bytes (the byte order mark) and decrement tl by 2.
        
    if (c) {
        sd_file->read(pb, 2);
        tl -= 2;
    }
    // remember that titles are limited to only max_title_len bytes long.
    
    if (tl > max_title_len) tl = max_title_len;
        
    // read in tl bytes of the title itself. add an 'end-of-string' byte.

    sd_file->read(value, tl);
	value[tl] = '\0';

	if (value[1] == '\0'){
		// at odd indices seem to have null terminators so let's get rid of them
		for(int i = 0; i < tl; i++){
			if (i % 2 == 1) continue;
			value[i/2] = value[i];
			//Serial.print(title[i]);
		}

		//add a null terminator at the new end of the title
		value[tl/2] = '\0';
	}

	//Serial.print("value: ");
	//Serial.println(value);
	//Serial.println("END getId3Tag");
	sd_file->seekSet(origPos);
}

void Id3Tag::clearBuffers(){
	title[0] = '\0';
	artist[0] = '\0';
	album[0] = '\0';
}

void Id3Tag::scan(SdFile* sd_file){
	//Serial.println("Id3Tag::scan()");
	clearBuffers();
	int numTagsFound = 0;

  unsigned char id3[3];       // pointer to the first 3 characters to read in

  // visit http://www.id3.org/id3v2.3.0 to learn all(!) about the id3v2 spec.
  // move the file pointer to the beginning, and read the first 3 characters.

  sd_file->seekSet(0);
  sd_file->read(id3, 3);
  
  // if these first 3 characters are 'ID3', then we have an id3v2 tag. if so,
  // a 'TIT2' (for ver2.3) or 'TT2' (for ver2.2) frame holds the song title.
  if (id3[0] == 'I' && id3[1] == 'D' && id3[2] == '3') {
	  //Serial.println("ID3");
    unsigned char pb[4];       // pointer to the last 4 characters we read in
    unsigned char c;           // the next 1 character in the file to be read
    
    // our first task is to find the length of the (whole) id3v2 tag. knowing
    // this means that we can look for 'TIT2' or 'TT2' frames only within the
    // tag's length, rather than the entire file (which can take a while).

    // skip 3 bytes (that we don't use), then read in the last 4 bytes of the
    // header, which contain the tag's length.

    sd_file->read(pb, 3);
    sd_file->read(pb, 4);
    
    // to combine these 4 bytes together into the single value, we first have
    // to shift each one over to get it into its correct 'digits' position. a
    // quirk of the spec is that bit 7 (the msb) of each byte is set to 0.
    
    unsigned long v2l = ((unsigned long) pb[0] << (7 * 3)) +
                        ((unsigned long) pb[1] << (7 * 2)) +
                        ((unsigned long) pb[2] << (7 * 1)) + pb[3];
	v2l /= 8; 
    // we just moved the file pointer 10 bytes into the file, so we reset it.
    
    sd_file->seekSet(0);

	//Serial.print("id3 header length: ");
	//Serial.println(v2l);
	
	char buff[BUFF_SIZE+1];

    for(int i = 0; i < v2l; i+=BUFF_SIZE){
		sd_file->read(buff, BUFF_SIZE);
		buff[BUFF_SIZE] = 0;
		for(int j = 0; j < BUFF_SIZE; j++){
      // read in bytes of the file, one by one, so we can check for the tags.
      
      //sd_file->read(&c, 1);
	  c = buff[j];
      // keep shifting over previously-read bytes as we read in each new one.
      // that way we keep testing if we've found a 'TIT2' or 'TT2' frame yet.
      
      pb[0] = pb[1];
      pb[1] = pb[2];
      pb[2] = pb[3];
      pb[3] = c;

	  //Serial.print(c);

      if (pb[0] == 'T' && pb[1] == 'I' && pb[2] == 'T' && pb[3] == '2') {
		  numTagsFound++;
		  //Serial.println("title");
		  getId3Tag(sd_file, title, pb, c, BUFF_SIZE-j-1);
      }
	  else if (pb[0] == 'T' && pb[1] == 'P' && pb[2] == 'E' && pb[3] == '1') {
		  numTagsFound++;
		  //Serial.println("artist");
		  getId3Tag(sd_file, artist, pb, c, BUFF_SIZE-j-1);
	  }
	  else if (pb[0] == 'T' && pb[1] == 'A' && pb[2] == 'L' && pb[3] == 'B') {
		  numTagsFound++;
		  //Serial.println("album");
		  getId3Tag(sd_file, album, pb, c, BUFF_SIZE-j-1);
		  
	  }
	  /*else if (pb[0] == 'T' && pb[1] == 'I' && pb[2] == 'M' && pb[3] == 'E') {
		  Serial.println("time");
		  getId3Tag(time, pb, c);
	  }*/
      else if (pb[1] == 'T' && pb[2] == 'T' && pb[3] == '2') {
		  numTagsFound++;
		  //Serial.println("TT2");
        // found an id3v2.2 frame! the title's length is in the next 3 bytes,
        // but we read in 4 then ignore the last, which is the text encoding.
        
        sd_file->read(pb, 4);
        
        // shift each byte over to get it into its correct 'digits' position. 
        
        unsigned long tl = ((unsigned long) pb[0] << (8 * 2)) +
                           ((unsigned long) pb[1] << (8 * 1)) + pb[2];
        tl--;
        
        // remember that titles are limited to only max_title_len bytes long.

        if (tl > max_title_len) tl = max_title_len;

        // there's no text encoding, so read in tl bytes of the title itself.
        
        sd_file->read(title, tl);
        title[tl] = '\0';
        break;
      }
      else
      if (sd_file->curPosition() == v2l) {
		  Serial.println("EOT");
        // we reached the end of the id3v2 tag. use the file name as a title.

        strncpy(title, fn, max_name_len);
        break;
      }
    }
	}
  }
  else {
    // the file doesn't have an id3v2 tag so search for an id3v1 tag instead.
    // an id3v1 tag begins with the 3 characters 'TAG'. if these are present,
    // then they are located exactly 128 bits from the end of the file.
    
    sd_file->seekSet(sd_file->fileSize() - 128);
    sd_file->read(id3, 3);
    
    if (id3[0] == 'T' && id3[1] == 'A' && id3[2] == 'G') {
		Serial.println("TAG");
      // found it! now read in the full title, which is always 30 bytes long.
      
      sd_file->read(title, 30);
      
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
  
  sd_file->seekSet(0);
  //Serial.println("exit id3tag");
}