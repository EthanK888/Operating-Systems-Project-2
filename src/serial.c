#include <dirent.h> 
#include <stdio.h> 
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <time.h>
#include <pthread.h>

#define BUFFER_SIZE 1048576 // 1MB
#define NUM_CONSUMERS 8

char **files = NULL;
int nfiles = 0, fill = 0, use = 0, count = 0, total_in = 0, total_out = 0;



pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct{
	unsigned char* frame;
	int nbytes;
} compressedFrame;

compressedFrame* compressedFrames = NULL;

void* consumer(void* arg){
	

	while(1){
		pthread_mutex_lock(&lock);
		if(use == nfiles){
			pthread_mutex_unlock(&lock);
			break;
		}

		int curFile = use;
		use++;
		pthread_mutex_unlock(&lock);
		
		int len = strlen((char*)arg)+strlen(files[curFile])+2;
		char *full_path = malloc(len*sizeof(char));
		assert(full_path != NULL);
		strcpy(full_path, (char*)arg);
		strcat(full_path, "/");
		strcat(full_path, files[curFile]);

		unsigned char buffer_in[BUFFER_SIZE];
		unsigned char buffer_out[BUFFER_SIZE];

		// load file
		FILE *f_in = fopen(full_path, "r");
		assert(f_in != NULL);
		int nbytes = fread(buffer_in, sizeof(unsigned char), BUFFER_SIZE, f_in);
		fclose(f_in);
		pthread_mutex_lock(&lock);
		total_in += nbytes;
		pthread_mutex_unlock(&lock);

		// zip file
		z_stream strm;
		int ret = deflateInit(&strm, 9);
		assert(ret == Z_OK);
		strm.avail_in = nbytes;
		strm.next_in = buffer_in;
		strm.avail_out = BUFFER_SIZE;
		strm.next_out = buffer_out;

		ret = deflate(&strm, Z_FINISH);
		assert(ret == Z_STREAM_END);

		compressedFrame frame;
		frame.nbytes = BUFFER_SIZE-strm.avail_out;
		frame.frame = malloc(nbytes * sizeof(unsigned char));
		assert(frame.frame != NULL);
		memcpy(frame.frame, buffer_out, frame.nbytes);

		compressedFrames[curFile] = frame;

		free(full_path);
		
	}

	//fclose(f_out);
}

int cmp(const void *a, const void *b) {
	return strcmp(*(char **) a, *(char **) b);
}

int main(int argc, char **argv) {
	// time computation header
	struct timespec start, end;
	clock_gettime(CLOCK_MONOTONIC, &start);
	// end of time computation header

	// do not modify the main function before this point!

	assert(argc == 2);

	DIR *d;
	struct dirent *dir;
	

	d = opendir(argv[1]);
	if(d == NULL) {
		printf("An error has occurred\n");
		return 0;
	}

	// create sorted list of PPM files
	while ((dir = readdir(d)) != NULL) {
		files = realloc(files, (nfiles+1)*sizeof(char *));
		assert(files != NULL);

		int len = strlen(dir->d_name);
		if(dir->d_name[len-4] == '.' && dir->d_name[len-3] == 'p' && dir->d_name[len-2] == 'p' && dir->d_name[len-1] == 'm') {
			files[nfiles] = strdup(dir->d_name);
			assert(files[nfiles] != NULL);

			nfiles++;
		}
	}
	closedir(d);
	qsort(files, nfiles, sizeof(char *), cmp);

	// create a single zipped package with all PPM files in lexicographical order
	//int total_in = 0, total_out = 0;
	/*FILE *f_out = fopen("video.vzip", "w");
	assert(f_out != NULL);
	for(int i=0; i < nfiles; i++) {
		int len = strlen(argv[1])+strlen(files[i])+2;
		char *full_path = malloc(len*sizeof(char));
		assert(full_path != NULL);
		strcpy(full_path, argv[1]);
		strcat(full_path, "/");
		strcat(full_path, files[i]);

		unsigned char buffer_in[BUFFER_SIZE];
		unsigned char buffer_out[BUFFER_SIZE];

		// load file
		FILE *f_in = fopen(full_path, "r");
		assert(f_in != NULL);
		int nbytes = fread(buffer_in, sizeof(unsigned char), BUFFER_SIZE, f_in);
		fclose(f_in);
		total_in += nbytes;

		// zip file
		z_stream strm;
		int ret = deflateInit(&strm, 9);
		assert(ret == Z_OK);
		strm.avail_in = nbytes;
		strm.next_in = buffer_in;
		strm.avail_out = BUFFER_SIZE;
		strm.next_out = buffer_out;

		ret = deflate(&strm, Z_FINISH);
		assert(ret == Z_STREAM_END);

		// dump zipped file
		int nbytes_zipped = BUFFER_SIZE-strm.avail_out;
		fwrite(&nbytes_zipped, sizeof(int), 1, f_out);
		fwrite(buffer_out, sizeof(unsigned char), nbytes_zipped, f_out);
		total_out += nbytes_zipped;

		free(full_path);
	}
	fclose(f_out);*/

	

	compressedFrames = malloc(nfiles * sizeof(compressedFrame));
    assert(compressedFrames != NULL);

	pthread_t consumers[NUM_CONSUMERS];
	for(int i = 0; i < NUM_CONSUMERS; i++){
		pthread_create(&consumers[i], NULL, consumer, (void*)argv[1]);
	}

	for(int i = 0; i < NUM_CONSUMERS; i++){
		pthread_join(consumers[i], NULL);
	}

	FILE* f_out = fopen("video.vzip", "w");
	assert(f_out != NULL);

	for(int i = 0; i < nfiles; i++){
		// dump zipped file
		int nbytes_zipped = compressedFrames[i].nbytes;
		fwrite(&nbytes_zipped, sizeof(int), 1, f_out);
		fwrite(compressedFrames[i].frame, sizeof(unsigned char), nbytes_zipped, f_out);
		total_out += nbytes_zipped;
	}

	fclose(f_out);

	printf("Compression rate: %.2lf%%\n", 100.0*(total_in-total_out)/total_in);

	// release list of files
	for(int i=0; i < nfiles; i++)
		free(files[i]);
	free(files);

	for(int i = 0; i < nfiles; i++){
		free(compressedFrames[i].frame);
	}
	free(compressedFrames);

	// do not modify the main function after this point!

	// time computation footer
	clock_gettime(CLOCK_MONOTONIC, &end);
	printf("Time: %.2f seconds\n", ((double)end.tv_sec+1.0e-9*end.tv_nsec)-((double)start.tv_sec+1.0e-9*start.tv_nsec));
	// end of time computation footer

	return 0;
}
