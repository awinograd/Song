/*
 * Arduino Library for VS10XX Decoder & FatFs
 * (c) 2010, David Sirkin sirkin@stanford.edu
 */
 
#ifndef SONG_H
#define SONG_H

#include <JsonHandler.h>

class Song
{
  public:
	Song();
	void setup(JsonHandler *handler);
	void loop();
	void pause();
	void play();
	int seek(int percent);
	double setVolume(int volume_percentage);
	int getVolume();
	bool nextFile();
	bool prevFile();
	uint32_t getFileSize();
	bool isPlaying();

	char* getTitle();
	char* getArtist();
	char* getAlbum();
	char* getTime();

	void sendPlayerState();
	void sendSongInfo();
  private:
	JsonHandler *handler;

	void sd_file_open();
	bool nextFileExists();
	bool prevFileExists();

	void dir_play();
	void mp3_play();

	void sd_card_setup();
	void sd_dir_setup();
	void map_current_song_to_fn();

	void initPlayerStateFromEEPROM();
	void sendSongInfo(bool first);
};

#endif