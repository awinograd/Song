/*
 * Arduino Library for VS10XX Decoder & FatFs
 * (c) 2010, David Sirkin sirkin@stanford.edu
 */
 
#ifndef JSONHANDLER_H
#define JSONHANDLER_H

class JsonHandler
{
  public:
	JsonHandler();
	void setup();
	void respond();
	void respond(bool endChar);
	void respondString(char* data);
	bool inputAvailable();
	void readCommand(char* buffer, char* data);

	void addKeyValuePair(const char* key, const char* val, bool firstPair);
	void addKeyValuePair(const char* key, const char* val);
	void addKeyValuePair(const char* key, int val);
  private:
	void readChar(char &c);
	
};

#endif