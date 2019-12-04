//#define DEBUG
#include "util.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

FILE *plog;

#define file_log(...) fprintf(plog,##__VA_ARGS__)
static int wrong_frames = 0;

int getADTSframe(unsigned char* buffer, int buf_size, unsigned char* data ,int* data_size){
	int size = 0;

	if(!buffer || !data || !data_size ){
		return -1;
	}

	while(1){
		if(buf_size  < 7 ){
			return -1;
		}
		//Sync words
		if((buffer[0] == 0xff) && ((buffer[1] & 0xf0) == 0xf0) ){
			size |= ((buffer[3] & 0x03) <<11);     //high 2 bit
			size |= buffer[4]<<3;                //middle 8 bit
			size |= ((buffer[5] & 0xe0)>>5);        //low 3bit
			break;
		}
		--buf_size;
		++buffer;
	}

	if(buf_size < size){
		return 1;
	}

	memcpy(data, buffer, size);
	*data_size = size;

	return 0;
}

int simplest_aac_parser(const char *url)
{
	int data_size = 0;
	int size = 0;
	int cnt=0;
	int offset=0;

	//FILE *myout=fopen("output_log.txt","wb+");
	FILE *myout=stdout;

	unsigned char *aacframe=(unsigned char *)malloc(1024*5);
	unsigned char *aacbuffer=(unsigned char *)malloc(1024*1024);

	FILE *ifile = fopen(url, "rb");
	if(!ifile){
		printf("Open file error");
		return -1;
	}

	printf("-----+- ADTS Frame Table -+------+\n");
	printf(" NUM | Profile | Frequency| Size |\n");
	printf("-----+---------+----------+------+\n");

	while(!feof(ifile)){
		data_size = fread(aacbuffer+offset, 1, 1024*1024-offset, ifile);
		unsigned char* input_data = aacbuffer;

		while(1)
		{
			int ret=getADTSframe(input_data, data_size, aacframe, &size);
			if(ret==-1){
				break;
			}else if(ret==1){
				memcpy(aacbuffer,input_data,data_size);
				offset=data_size;
				break;
			}

			char profile_str[10]={0};
			char frequence_str[10]={0};

			unsigned char profile=aacframe[2]&0xC0;
			profile=profile>>6;
			switch(profile){
			case 0: sprintf(profile_str,"Main");break;
			case 1: sprintf(profile_str,"LC");break;
			case 2: sprintf(profile_str,"SSR");break;
			default:sprintf(profile_str,"unknown");break;
			}

			unsigned char sampling_frequency_index=aacframe[2]&0x3C;
			sampling_frequency_index=sampling_frequency_index>>2;
			switch(sampling_frequency_index){
			case 0: sprintf(frequence_str,"96000Hz");break;
			case 1: sprintf(frequence_str,"88200Hz");break;
			case 2: sprintf(frequence_str,"64000Hz");break;
			case 3: sprintf(frequence_str,"48000Hz");break;
			case 4: sprintf(frequence_str,"44100Hz");break;
			case 5: sprintf(frequence_str,"32000Hz");break;
			case 6: sprintf(frequence_str,"24000Hz");break;
			case 7: sprintf(frequence_str,"22050Hz");break;
			case 8: sprintf(frequence_str,"16000Hz");break;
			case 9: sprintf(frequence_str,"12000Hz");break;
			case 10: sprintf(frequence_str,"11025Hz");break;
			case 11: sprintf(frequence_str,"8000Hz");break;
			default:sprintf(frequence_str,"unknown");break;
			}


			fprintf(myout,"%5d| %8s|  %8s| %5d|\n",cnt,profile_str ,frequence_str,size);
			data_size -= size;
			input_data += size;
			cnt++;
		}   

	}
	fclose(ifile);
	free(aacbuffer);
	free(aacframe);

	return 0;
}


struct adts_header
{
	uint16_t syncword :12;
	uint16_t id :1;
	uint16_t layer :2;
	uint16_t absent :1;

	uint16_t profile :2;
	uint16_t sampling_freq_index :4;
	uint16_t private_bit :1;
	uint16_t original_copy : 1;

	uint16_t channel_config : 3;	
	uint16_t home : 1;
	/*for variable*/
	uint16_t id_bit:1;
	uint16_t id_start:1;
	uint16_t no_raw_block_inframe:2;

	
	uint16_t frame_length;
	uint16_t buffer_fllness;
	uint32_t frame_index;
};

struct adts_variable_header
{
	uint16_t id_start:1;
	uint16_t frame_len :13;
};


static int debug_adts_header(struct adts_header *header)
{
	if (header->layer != 0) {
		red_bug("headr is wrong\r\n");
	}else{
		return 0;
	}
	ibug("syncword:%X\t", header->syncword);
	ibug("%s\t", header->id == 0 ? "MPEG-4":"MPEG-2");
	switch (header->profile) {
		case 0:
			ibug("Main");
			break;
		case 1:
			ibug("LC");
			break;	
		case 2:
			ibug("SSR");
			break;		
		default:
			ibug("Unknown");
	}
	ibug("\t");
	switch (header->sampling_freq_index) {
		case 0: ibug("96000Hz");break;
		case 1: ibug("88200Hz");break;
		case 2: ibug("64000Hz");break;
		case 3: ibug("48000Hz");break;
		case 4: ibug("44100Hz");break;
		case 5: ibug("32000Hz");break;
		case 6: ibug("24000Hz");break;
		case 7: ibug("22050Hz");break;
		case 8: ibug("16000Hz");break;
		case 9: ibug("12000Hz");break;
		case 10: ibug("11025Hz");break;
		case 11: ibug("8000Hz");break;
		default:ibug("unknown");break;
	}
	ibug("\t\tchannels:%d\t", header->channel_config);
	ibug("Frame:%d\t", header->frame_length);

	ibug("No raw data:%d\t", header->no_raw_block_inframe);
	ibug("\r\n");
	return -1;
}

#define search_not_over(index ,size) ((index) + 8 < (size)) 


int find_adts_fixed_head(const uint8_t *in, int size)
{
	int start = 0;
	
	/*fixed head at least 7 bytes length*/
	for (start = 0; search_not_over(start, size); start++) {
		if (in[start] == 0xff && (in[start +1]>>4) == 0xf) { 
			debug("find head\r\n");
			break;
		}
	}
	return start;
}




/*return value is the byte handled.*/
static int start_anasys(const uint8_t *data, int size, int *index)
{
	struct adts_header header;
	const uint8_t *in = data;
	int i, this_idx = *index;
	/*we analysis as many as possible*/
	int start = 0;
	for (; ;) {
		start = find_adts_fixed_head(in, size);
		if (search_not_over(start, size)) {
			/*find head, and move to head*/
			in += start;
			size -= start;
			debug("start is %d\r\n",start);
			header.syncword = (in[0]<<4) | (in[1] >>4);
			header.id = in[1]>>3;  
			header.layer = in[1]>>1;  
			header.absent= in[1]; 
			
			header.profile = in[2] >>6;
			header.sampling_freq_index= in[2] >>2;
			header.private_bit = in[2] >>1;
			header.channel_config = (in[3]>>6) + ((in[2] &1) <<3);
			header.original_copy = in[3]>>5;
			header.home = in[3]>>4; 
			header.id_bit = in[3] >> 1;
			header.id_start = in[3] ;
			header.frame_length = (in[4] << 3) |  (in[5] >> 5);
			
			header.buffer_fllness = ((in[5] & 0x1f)<< 6) |  (in[6] >> 2);
			header.no_raw_block_inframe = in[7] >> 6;
			this_idx++;
//			ibug("{%4d}\t", this_idx);
			if (debug_adts_header(&header)) {
				wrong_frames++;
				file_log("[%-4d] format wrong :",this_idx);	
				for (i = 0; i< 7; i++){
					ibug("%02X ",in[i]);
					file_log("%02X ", in[i]);
				}
				file_log("\r\n");
				ibug("\r\n");
			}
			in += 8;
			size -= 8;
		}else {
			break;
		}
	}
	*index = this_idx;
	return in-data;
}



int parse_adts(const char *filename)
{
#define FILE_BUFFER_SIZE 512
//1024*1024
#define AAC_BUFFER_SIZE	 10240
	int index = 0;
	uint8_t *aac_buffer = NULL, *file_buffer = NULL;

	FILE *pfile = fopen(filename, "rb");
	int ret, nread, offset = 0, skip;
	
	if (pfile == NULL) {
		red_bug("open file %s error\r\n",filename);
		ret = -1;
		goto FINISH;
	}
	
	aac_buffer = (uint8_t *)malloc(AAC_BUFFER_SIZE);
	file_buffer = (uint8_t *)malloc(FILE_BUFFER_SIZE);
	if (aac_buffer == NULL || file_buffer == NULL) {
		red_bug("alloc memory failed\r\n");
		ret = -2;
		goto FINISH;
	}

	while (!feof(pfile)) {
		nread = fread(file_buffer + offset, 1, FILE_BUFFER_SIZE - offset, pfile);
		debug("read %d data\r\n",nread);
		if (nread < 0) {
			red_bug("read file error (%d)\r\n", nread);
			goto FINISH;
		}
		skip = start_anasys(file_buffer, nread + offset, &index);
		offset = nread + offset - skip;
	}
	ibug("%d wrong frames in %d frames\r\n",wrong_frames, index);
	
FINISH:
	fclose(pfile);
	free(aac_buffer);
	free(file_buffer);
	return 0;
}




int main (int argc ,char **argv)
{
	const char *url = "test.aac";
	
	const char *log_file = "log.txt";
	if (argc > 1)
		url = argv[1];
	if (argc > 2)
		log_file = argv[2];
	plog = fopen(log_file, "wb");
	if (plog == NULL) {
		ebug("open log file error\r\n");
		return -1;
	}
#if 0
	parse_adts(url);
#else
	FILE *pfile = fopen(url, "rb");
	int ret, nread, offset = 0, skip;
	int first = 1, i = 0;

	if (pfile == NULL) {
		red_bug("open file %s error\r\n",url);
		return -2;
	}

	uint8_t *aac_buffer = (uint8_t *)malloc(1024*1024);
	uint8_t blank = 0xC0;
	while (!feof(pfile)) {
		nread = fread(aac_buffer , 1, 1024*1024, pfile);
		for (i = 0; i+1 < nread ;i++) {
			if (aac_buffer[i] == 0xff && (aac_buffer[i + 1]>>4) == 0xf) { 
				if (first == 0) {
					fwrite(&blank, 1, 1, plog);
				}else {
					first = 0;
				}
			}
			fwrite(aac_buffer+i, 1, 1, plog);
		}	
		fwrite(aac_buffer+i, 1, 1, plog);
	}



fclose(plog);

#endif
	return 0;
}

