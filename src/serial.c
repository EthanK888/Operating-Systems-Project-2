#include <dirent.h> 
#include <stdio.h> 
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>

#define BUFFER_SIZE 1048576 // 1MB
#define MAX_FILES 100
#define NUM_CONSUMERS 8
#define NUM_PRODUCERS 2

char **files = NULL;
int nfiles = 0, fill = 0, use = 0, count = 0, total_in = 0, total_out = 0;
int producersComplete = 0;
int curCompressedFrame = 1;

//sem_t s;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t full = PTHREAD_COND_INITIALIZER;
pthread_cond_t empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t ready = PTHREAD_COND_INITIALIZER;

typedef struct{

} frame;

typedef struct{
	unsigned char* frame;
	int nbytes;
} compressedFrame;

compressedFrame* compressedFrames = NULL;

DIR *d;
FILE* f_out;

void* producer(void* arg){
	
	struct dirent *dir;


	

	// create sorted list of PPM files
	while ((dir = readdir(d)) != NULL) {
		pthread_mutex_lock(&lock);
		//while(count == MAX_FILES) pthread_cond_wait(&empty, &lock);
		files = realloc(files, (fill+1)*sizeof(char *));

		int len = strlen(dir->d_name);
		
		if(dir->d_name[len-4] == '.' && dir->d_name[len-3] == 'p' && dir->d_name[len-2] == 'p' && dir->d_name[len-1] == 'm') {
			//files[fill] = malloc(len * sizeof(char));
			files[fill] = strdup(dir->d_name);
			assert(files[fill] != NULL);

			//printf("file name: %s\n", dir->d_name);
			count++;
			//printf("count: %d\n", count);
			fill++;
			pthread_cond_signal(&full);
		}
		pthread_mutex_unlock(&lock);
	}
	
}

void* consumer(void* arg){
	while(1){
		pthread_mutex_lock(&lock);
		while(count == 0 && !producersComplete) pthread_cond_wait(&full, &lock);
		
		if(count == 0 && producersComplete){
			pthread_mutex_unlock(&lock);
			break;
		}

		int curFile = use;
		use++;
		count--;
		//printf("count: %d\n", count);
		pthread_cond_signal(&empty);
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
		//sem_wait(&s);
		total_in += nbytes;
		//sem_post(&s);
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

		/*compressedFrame frame;
		frame.nbytes = BUFFER_SIZE-strm.avail_out;
		frame.frame = malloc(nbytes * sizeof(unsigned char));
		assert(frame.frame != NULL);
		memcpy(frame.frame, buffer_out, frame.nbytes);*/

		char fileNum_string[5];
		fileNum_string[0] = files[curFile][0];
		fileNum_string[1] = files[curFile][1];
		fileNum_string[2] = files[curFile][2];
		fileNum_string[3] = files[curFile][3];
		fileNum_string[4] = '\0';

		int fileNum = atoi(fileNum_string);
		//printf("file name: %s fileNum: %d\n", files[curFile], fileNum);

		if(fileNum > nfiles){
			pthread_mutex_lock(&lock);
			//sem_wait(&s);
			nfiles = fileNum;
			compressedFrames = realloc(compressedFrames, fileNum * sizeof(compressedFrame));
			//sem_post(&s);
			pthread_mutex_unlock(&lock);
		}
		
		compressedFrame frame;
		frame.nbytes = BUFFER_SIZE-strm.avail_out;
		frame.frame = malloc(frame.nbytes * sizeof(unsigned char*));
		memcpy(frame.frame, buffer_out, frame.nbytes);

		/*pthread_mutex_lock(&lock);
		//while(fileNum != curCompressedFrame) pthread_cond_wait(&ready, &lock);
		
		int nbytes_zipped = BUFFER_SIZE-strm.avail_out;
		fwrite(&nbytes_zipped, sizeof(int), 1, f_out);
		fwrite(buffer_out, sizeof(unsigned char), nbytes_zipped, f_out);
		total_out += nbytes_zipped;
		curCompressedFrame++;
		pthread_cond_signal(&ready);
		pthread_mutex_unlock(&lock);*/

		compressedFrames[fileNum-1] = frame;

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
	//sem_init(&s, 0, 1);
	
	//printf("test 1\n");
	assert(argc == 2);

	d = opendir(argv[1]);
	if(d == NULL) {
		printf("An error has occurred\n");
		return 0;
	}

	//printf("test 2\n");

	f_out = fopen("video.vzip", "w");
	assert(f_out != NULL);

	

	//qsort(files, nfiles, sizeof(char *), cmp);


	

	//compressedFrames = malloc(MAX_FILES * sizeof(compressedFrame));
    //assert(compressedFrames != NULL);

	//files = malloc(MAX_FILES * sizeof(char*));

	pthread_t producers[NUM_PRODUCERS];
	for(int i = 0; i < NUM_PRODUCERS; i++){
		pthread_create(&producers[i], NULL, producer, (void*)argv[1]);
	}
	//printf("test 3\n");
	pthread_t consumers[NUM_CONSUMERS];
	for(int i = 0; i < NUM_CONSUMERS; i++){
		pthread_create(&consumers[i], NULL, consumer, (void*)argv[1]);
	}
	//printf("test 4\n");
	for(int i = 0; i < NUM_PRODUCERS; i++){
		pthread_join(producers[i], NULL);
	}
	//printf("test 5\n");
	producersComplete = 1;
	for(int i = 0; i < NUM_CONSUMERS; i++){
		pthread_cond_signal(&full);
	}
	//printf("test 6\n");
	for(int i = 0; i < NUM_CONSUMERS; i++){
		pthread_join(consumers[i], NULL);
	}
	//printf("test 7\n");
	closedir(d);

	

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
	for(int i=0; i < fill; i++)
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
