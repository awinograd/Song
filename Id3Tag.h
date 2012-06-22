#ifndef ID3TAG_H
#define ID3TAG_H

class Id3Tag
{
  public:
	Id3Tag();
	void scan(SdFile* sd_file);

	char* getTitle();
	char* getArtist();
	char* getAlbum();
	char* getTime();
	char* getTag(const char* tag);
  private:
	void getId3Tag(SdFile* sd_file, char* value, unsigned char pb[], unsigned char c, int j);
	void clearBuffers();
};

#endif