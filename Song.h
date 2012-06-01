/*
 * Arduino Library for VS10XX Decoder & FatFs
 * (c) 2010, David Sirkin sirkin@stanford.edu
 */
 
#ifndef SONG_H
#define SONG_H

class Song
{
  public:
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

  private:
	void sd_file_open();
	bool nextFileExists();
	bool prevFileExists();

	void dir_play();
	void mp3_play();

	void sd_card_setup();
	void sd_dir_setup();
	void map_current_song_to_fn();
};

#endif