#ifndef ID3TAG_H
#define ID3TAG_H

class Id3Tag
{
  public:
	Id3Tag();
	//Id3Tag(SdFile* file);
	void setSDFile(SdFile* file);
	void scan();

	char* getTitle();
  private:
	void getId3Tag(char* value, unsigned char pb[], unsigned char c);

};

#endif