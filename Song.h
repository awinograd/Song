/*
 * Arduino Library for VS10XX Decoder & FatFs
 * (c) 2010, David Sirkin sirkin@stanford.edu
 */
 
#ifndef SONG_H
#define SONG_H

class Song
{
  public:

/*struct TAGdata
{
	char tag[3];
	char title[30];
 	char artist[30];
	char album[30];
	char year[4];
	char comment[30];
	char genre;
};*/

	Song();
	void setup();
	void loop();
	void pause();
	void play();
	int seek(int percent);
	double setVolume(int volume_percentage);
	int getVolume();
	char* getCurrentSong();
	bool nextFile();
	bool prevFile();
	int getFileSize();
	bool isPlaying();
	//struct TAGData getID3Data();

  private:
	void sd_file_open();
	bool nextFileExists();
	bool prevFileExists();

	void dir_play();
	void mp3_play();

	void sd_card_setup();
	void sd_dir_setup();
	void map_current_song_to_fn();

	void initPlayerStateFromEEPROM();
	void get_title_from_id3tag();
};

#endif