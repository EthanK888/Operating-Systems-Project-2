#include <dirent.h> 
#include <stdio.h> 
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <time.h>
#include <pthread.h>

#define BUFFER_SIZE 1048576 // 1MB
#define NUM_COMPRESSORS 8

//Global variable declarations
char **files = NULL;
int nfiles = 0, use = 0, total_in = 0, total_out = 0, curFrame = 0;
FILE* f_out = NULL;

//Lock and condition variables
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t ready = PTHREAD_COND_INITIALIZER;

//A struct to store compressed frame data
typedef struct{
	unsigned char* frame;
	int nbytes;
} compressedFrame;

//A global array of compressed frames
compressedFrame* compressedFrames = NULL;

//Compressor function - function for compressor threads to take the frames, compress them, and add them to compressedFrames to be written
void* compressor(void* arg){
	//Loop until all frames have been compressed
	while(1){
		//Break the loop when all threads have been compressed
		if(use == nfiles){
			break;
		}
		//Incrementing use is a critical section, so it's locked
		pthread_mutex_lock(&lock);
		int curFile = use;
		use++;
		pthread_mutex_unlock(&lock);
		
		//Code to get the frame to be compressed (from original code)
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
		//Incrementing total_in is another critical section
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

		//Create a compressedFrame to store to the compressedFram array
		compressedFrame frame;
		//Number of bytes the frame takes up
		frame.nbytes = BUFFER_SIZE-strm.avail_out;
		//Allocate memory for the fram content and save it
		frame.frame = malloc(nbytes * sizeof(unsigned char));
		assert(frame.frame != NULL);
		memcpy(frame.frame, buffer_out, frame.nbytes);

		//Add the frame to the array
		compressedFrames[curFile] = frame;

		//Signal the writer that the next frame is ready, only if that frame is the one that the writer is currently on to save a little bit of time
		if(curFile == curFrame){
			pthread_cond_signal(&ready);
		}

		free(full_path);
	}
}

//writer function - function for writer thread to take compressed frames and write them to video.vzip
void* writer(void* arg){
	//Loop until all the frames have been written to the vzip file
	while(curFrame < nfiles){
		//Lock in order to wait for the next frame to be ready
		pthread_mutex_lock(&lock);
		//Wait on the signal from a compressor
		while(compressedFrames[curFrame].frame == NULL){
			pthread_cond_wait(&ready, &lock);
		}
		pthread_mutex_unlock(&lock);

		// dump zipped file
		int nbytes_zipped = compressedFrames[curFrame].nbytes;
		fwrite(&nbytes_zipped, sizeof(int), 1, f_out);
		fwrite(compressedFrames[curFrame].frame, sizeof(unsigned char), nbytes_zipped, f_out);
		total_out += nbytes_zipped;

		//curFrame is the frame that the writer is currently on, so it is incremented when a frame is written
		curFrame++;
	}
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

	//Reading in all the frames is done outside of the threads so that the files will be in order
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

	//Allocate memory for the compressedFrames array
	compressedFrames = malloc(nfiles * sizeof(compressedFrame));
    assert(compressedFrames != NULL);
	//Set each frame's data to NULL so that the writers know that the frame isn't ready yet
	for(int i = 0; i < nfiles; i++){
		compressedFrames[i].frame = NULL;
	}
	
	//Open the video.vzip file before creating the writer thread
	f_out = fopen("video.vzip", "w");
	assert(f_out != NULL);
	//printf("test 6\n");
	pthread_t fileWriter;
	pthread_create(&fileWriter, NULL, writer, NULL);
	

	//Create the compressor threads
	pthread_t compressors[NUM_COMPRESSORS];
	for(int i = 0; i < NUM_COMPRESSORS; i++){
		pthread_create(&compressors[i], NULL, compressor, (void*)argv[1]);
	}
	
	//Join all the threads back to the main thread
	for(int i = 0; i < NUM_COMPRESSORS; i++){
		pthread_join(compressors[i], NULL);
	}
	pthread_join(fileWriter, NULL);
	
	fclose(f_out);

	printf("Compression rate: %.2lf%%\n", 100.0*(total_in-total_out)/total_in);

	// release list of files
	for(int i=0; i < nfiles; i++)
		free(files[i]);
	free(files);

	//Release the list of compressed frames
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
