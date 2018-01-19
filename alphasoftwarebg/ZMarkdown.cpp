// ZMarkdown.cpp

#include <string>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef _WIN32
	#include <share.h>    // SH_DENYNO
	#include <windows.h>
	#include <io.h>
#else
	#include <stdio.h>
	#include <unistd.h>
	#include <stdexcept>
#endif
#include <sys/stat.h>
#include <map>
#include <list>

using namespace std;

#ifdef WIN32
	#define snprintf sprintf_s
#endif // WIN32

int zzz_open (const char* pathname, int flags, int mode)
{
#ifdef _WIN32
	int fileHandle;
	_sopen_s(&fileHandle, pathname, flags, SH_DENYNO, mode);
    return fileHandle;
#else
    return open(pathname, flags, mode);
#endif
}

int zzz_close (int fd)
{
#ifdef _WIN32
    return _close(fd);
#else
    return close(fd);
#endif
}

int zzz_lseek (int fd, int offset, int from)
{
#ifdef _WIN32
	return (int)_lseeki64(fd, (__int64)offset, from);
#else
	return lseek(fd, offset, from);
#endif
}

int zzz_tell (int fd)
{
	return zzz_lseek(fd,0,SEEK_CUR);
}

int zzz_filelength (int fd)
{
    int offset = zzz_tell(fd);

    zzz_lseek(fd, 0, SEEK_END);
    int length = zzz_tell(fd);

    zzz_lseek(fd, offset, SEEK_SET);

    return length;
}

int zzz_read (int fd, void* buf, int nbyte)
{
#ifdef _WIN32
    return _read(fd, buf, (unsigned int)nbyte);
#else
    return (int)read(fd, buf, (unsigned int)nbyte);
#endif
}

int zzz_write (int fd, const void* buf, int nbyte)
{
#ifdef _WIN32
    return _write(fd, buf, (unsigned int)nbyte);
#else
    return (int)write(fd, buf, (unsigned int)nbyte);
#endif
}

#define ZERO '\0'
#define LF '\n'
#define SQUARE_OPEN '['
#define SQUARE_CLOSE ']'
#define HASHTAG '#'
#define BOLD '*'

class ZMarkdown
{
private:
	char* in;
	char* out;
	int inFileLength;
	int outFileLength;
	map<string, string> links;
	list<string> contents;
	int contentsLength;

	void outRealloc(int size)
	{
		while(inFileLength < outFileLength + size + 1)
		{
			inFileLength *= 2;
			out = (char*)realloc(out, inFileLength);
		}
	}

	void load(string inFileName)
	{
		int inFile = zzz_open(inFileName.c_str(), O_RDONLY
#ifdef _WIN32
			|O_BINARY
#endif
			, S_IREAD);
		inFileLength = zzz_filelength(inFile);
		in = (char*)malloc(inFileLength+1);
		if(in == NULL)
			throw runtime_error("Unable to allocate memory for input markdown file.");
		if(zzz_read(inFile, in, inFileLength) != (int)inFileLength)
			throw runtime_error("Unable to read input markdown file.");
		zzz_close(inFile);
		in[inFileLength] = ZERO;

		inFileLength *= 2;
		out = (char*)malloc(inFileLength);
		if(out == NULL)
			throw runtime_error("Unable to allocate memory for output html file.");
	}

	void save(string outFileName)
	{
		remove(outFileName.c_str());
		int outFile = zzz_open(outFileName.c_str(), O_WRONLY|O_CREAT
#ifdef _WIN32
			|O_BINARY
#endif
			, S_IWRITE);

		char* outContents = (char*)malloc(contentsLength+1);
		if(outContents == NULL)
			throw runtime_error("Unable to allocate memory for contents of the output html file.");
		outContents[0] = ZERO;
		contentsLength = 0;
		for(list<string>::iterator itList = contents.begin(); itList != contents.end(); itList++)
		{
			memcpy(&outContents[contentsLength], itList->c_str(), itList->length());
			contentsLength += (int)itList->length();
		}
		if(zzz_write(outFile, outContents, contentsLength) != (int)contentsLength)
			throw runtime_error("Unable to write output html file.");
		zzz_close(outFile);
		free(outContents);

		outFile = zzz_open(outFileName.c_str(), O_WRONLY|O_APPEND
#ifdef _WIN32
			|O_BINARY
#endif
			, S_IWRITE);
		//for(map<string, string>::iterator it = links.begin(); it != links.end(); it++)
		//{
		//	string a = "<a href='"+it->second+"'>"+it->first+"</a><br />\n";
		//	outRealloc((int)a.length());
		//	strcpy(&out[outFileLength], a.c_str());
		//	outFileLength += (int)a.length();
		//}

		if(zzz_write(outFile, out, outFileLength) != (int)outFileLength)
			throw runtime_error("Unable to write output html file.");
		zzz_close(outFile);
	}

	void mapLinks()
	{
		char* markdown = in;
		char previous = ZERO;
		char* mode = NULL;
		string link;
		string href;
		while(*markdown)
		{
			if(*markdown != LF)
			{
				if(previous == LF && *markdown == SQUARE_OPEN)
				{
					mode = markdown;
					link.clear();
				}
				else if(mode)
				{
					if(*markdown == SQUARE_CLOSE)
					{
						if(links.find(link) == links.end())
						{
							href.clear();
							markdown++;
							while(*markdown && *markdown != LF)
							{
								href += *markdown;
								markdown++;
							}
							links.insert(std::pair<string,string>(link, href));
							if(!*markdown)
							{
								break;
							}
						}
						memset(mode, 1, markdown-mode);
						mode = NULL;
						link.clear();
					}
					else
					{
						link += *markdown;
					}
				}
			}
			else
			{
				mode = NULL;
			}
			previous = *markdown;
			markdown++;
		}
	}

	void markdownToHTML()
	{
		char* markdown = in;
		char previous = ZERO;
		string link;
		string bold;
		string paragraph;
		while(*markdown)
		{
			if(*markdown == BOLD)
			{
				while(*++markdown && *markdown != BOLD && *markdown != LF)
				{
					bold += *markdown;
				}
				if(*markdown == BOLD)
				{
					markdown++;
				}
				if(!bold.empty())
				{
					paragraph += "<b>"+bold+"</b>";
					bold.clear();
				}
			}

			if(*markdown == SQUARE_OPEN)
			{
				while(*++markdown && *markdown != SQUARE_CLOSE && *markdown != LF)
				{
					link += *markdown;
				}
				if(*markdown == SQUARE_CLOSE)
				{
					markdown++;
				}
				if(!link.empty())
				{
					map<string, string>::iterator it = links.find(link);
					if(it != links.end())
					{
						paragraph += "<a href='"+it->second+"'>"+it->first+"</a>";
					}
					link.clear();
				}
			}
			if(previous == LF && *markdown == LF)
			{
				if(!paragraph.empty())
				{
					outRealloc((int)paragraph.length()+8);

					memcpy(&out[outFileLength], "<p>", 3);
					outFileLength += 3;
					memcpy(&out[outFileLength], paragraph.c_str(), paragraph.length());
					outFileLength += (int)paragraph.length();
					memcpy(&out[outFileLength], "</p>\n", 5);
					outFileLength += 5;
					paragraph.clear();
				}
			}
			if(previous <= LF && *markdown == HASHTAG)
			{
				int hashTags = 1;
				while(*++markdown && *markdown == HASHTAG)
				{
					hashTags++;
				}
				if(hashTags <= 6)
				{
					string header;
					if(markdown)
					{
						while(*markdown && *markdown != LF)
						{
							header += *markdown;
							markdown++;
						}
						if(!header.empty())
						{
							char buffer [33];
                            snprintf(buffer, 33, "%ld", contents.size()+1);

							char hType = (char)('0'+hashTags);
							string h = "<h";
							h += hType;
							h += " id='"+string(buffer)+"'>"+header+"</h";
							h += hType;
							h += ">\n";

							outRealloc((int)h.length()+8);
							memcpy(&out[outFileLength], h.c_str(), h.length());
							outFileLength += (int)h.length();

							string a = "<a href='#"+string(buffer)+"'>"+header+"</a><br />\n";
							contents.push_back(a);
							contentsLength += (int)a.length();
						}
					}
				}
				else
				{
					for(;hashTags; hashTags--)
					{
						paragraph += HASHTAG;
						outFileLength++;
					}
				}
			}
			if(*markdown > LF)
			{
				paragraph += *markdown;
			}
			previous = *markdown;
			markdown++;
		}
	}

public:
	ZMarkdown(string inFileName, string outFileName)
		: in(NULL), out(NULL), inFileLength(0), outFileLength(0), contentsLength(0)
	{
		load(inFileName);
		mapLinks();
		markdownToHTML();
		save(outFileName);
	}

	~ZMarkdown()
	{
		if(in)
		{
			free(in);
			in = NULL;
		}
		if(out)
		{
			free(out);
			out = NULL;
		}
	}
};

int main(int argc, char* argv[])
{
	string inFileName = "test.md";
	string outFileName = inFileName+".html";

	if(argc >= 2)
		inFileName = argv[1];
	if(argc >= 3)
		outFileName = argv[2];

	ZMarkdown zMarkdown(inFileName, outFileName);

	return 0;
}
