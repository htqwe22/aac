#include <stdint.h>
#define DEBUG
#include "util.h"


static unsigned const samplingFrequencyTable[16] = {
  96000, 88200, 64000, 48000,
  44100, 32000, 24000, 22050,
  16000, 12000, 11025, 8000,
  7350, 0, 0, 0
};

struct aacHeadElement
{
	uint8_t isValid;
	uint8_t profile;
	uint8_t sampling_frequency_index;
	uint8_t channel_configuration;
	uint8_t protection_absent;
	uint16_t frame_length;
}header;

int testing(FILE *pfile)
{

#define ANA_SIZE 7
	unsigned char fixedHeader[ANA_SIZE] = { 0 }; // it's actually 3.5 bytes long
	unsigned int read_pos = 0, start_pos;
#define HEAD(x) fixedHeader[(x) % ANA_SIZE]

	long deadLine = 0;
	header.isValid = 0;
	int readLen = 0;

	/*Note: if ANA_SIZE is not a number power of 2 , if the   read_pos big enough than a unsigned int ,then this method will not supported */
	for (read_pos = 0; ; read_pos++) {
		readLen = fread(&HEAD(read_pos), 1, 1, pfile);

		if (readLen == 0) {
		//	read_pos--;
			break ;

		}
		ibug("%02X", HEAD(read_pos));
		/*read at least 4 bytes*/
		if (read_pos + 1 < ANA_SIZE)
			continue;

		/* point to the start pos*/
		start_pos = read_pos - (ANA_SIZE -1);
		if (HEAD(start_pos) != 0xff || (HEAD(start_pos+1)& 0xf0) != 0xf0)
			continue;
		/*this is the layer ... layer always 00b*/
		if (HEAD(start_pos + 1) & 0x06)
			continue;

		header.protection_absent = HEAD(start_pos + 1) & 1;

		header.profile = (HEAD(start_pos + 2) & 0xc0) >> 6;
		/*profile 0 is MAIN, 1 is LC, 2 is SSR*/
		if (header.profile == 3) 
			continue ;

		header.sampling_frequency_index = (HEAD(start_pos+2) & 0x3C)>>2; // 4 bits
		if (samplingFrequencyTable[header.sampling_frequency_index] == 0) 
			continue;

		header.channel_configuration = ((HEAD(start_pos+2)&0x01)<<2)|((HEAD(start_pos+3)&0xC0)>>6); // 3 bits	
#if ANA_SIZE > 4
		// we can add more information to help us get the right ADTS
		header.frame_length = ((HEAD(start_pos+3)&0x03)<<11) | (HEAD(start_pos+4)<<3) | ((HEAD(start_pos+5)&0xE0)>>5);
#endif
		header.isValid = 1;
		fseek(pfile, 0 - ANA_SIZE, SEEK_CUR);
		break;

	}
	if (header.isValid == 1) {

		ibug("\r\n");

		debug("header drop %u bytes\r\n >> ", start_pos);

		for (read_pos = 0; read_pos < 7; read_pos++) 

		{
			ibug("%02X",HEAD(start_pos+read_pos));
		}

		ibug(" <<\r\n");
		return start_pos;
	}

	return -1;
}




int main(int argc, char **argv)
{
	int nread, i;
	unsigned char tmp_data[1024];
	const char *aac_file = "test.aac";
	if (argc > 1)
		aac_file = argv[1];
	FILE *fp = fopen(aac_file, "rb");
	int frames = 0;
for (;;) {
	if (testing(fp) < 0) {
		ebug("can not find start...\r\n");
		break ;
	}
	
	debug("header.frame_length = %d\r\n", header.frame_length);
	unsigned numBytesToRead = header.frame_length > ANA_SIZE ? header.frame_length - ANA_SIZE : 0;
	if (!header.protection_absent) {
		nread = fread(tmp_data, 1, 2, fp);
		if (nread != 2) {
			ebug("no mor data\r\n");
		}
		debug("CRC:%02X %02X\n",tmp_data[0], tmp_data[1]);
		numBytesToRead = numBytesToRead > 2 ? numBytesToRead - 2 : 0;
	}
	if (numBytesToRead > sizeof(tmp_data)) {
		debug("trancated %d bytes\r\n", numBytesToRead - sizeof(tmp_data));
	}
	debug("numBytesToRead  = %d (%d frames)\n", numBytesToRead, ++frames);
	numBytesToRead += ANA_SIZE;
	nread = fread(tmp_data, 1, numBytesToRead, fp);
	for (i = 0; i< nread; i++) {
		if ((i & 0xf) == 0xf)
			ibug("\r\n");
		ibug("%02X ", tmp_data[i]);
	}
	if (frames > 5)
		return 0;
	if (nread < numBytesToRead)
		break;
	ibug("\r\n=================================================\r\n");
	nread = fread(tmp_data, 1, 7, fp);
	if ((i & 0xf) == 0xf)
			ibug("\r\n");
		for (i = 0; i< nread; i++) {
		ibug("%02X ", tmp_data[i]);
	}
	if (nread < 7)
		break;
	fseek(fp, 0- nread, SEEK_CUR);
	ibug("\r\n=================================================\r\n");
}	
	return 0;
}


