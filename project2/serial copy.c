#include <dirent.h> 
#include <stdio.h> 
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <time.h>
#include <pthread.h>

#define BUFFER_SIZE 1048576 // 1MB
#define MAX_FRAMES 10
#define NUM_CONSUMERS 8

char **files = NULL;
int nfiles = 0, fill = 0, use = 0, count = 0, total_in = 0, total_out = 0, curFrame = 1, writtenFrames = 0;
FILE* f_out = NULL;



pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t full = PTHREAD_COND_INITIALIZER;
pthread_cond_t ready = PTHREAD_COND_INITIALIZER;

typedef struct{
	unsigned char* frame;
	int nbytes;
	int frameNum;
} compressedFrame;

compressedFrame* compressedFrames = NULL;

void* consumer(void* arg){
	

	while(1){
		pthread_mutex_lock(&lock);
		if(writtenFrames == nfiles){
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

		char string_frameNum[5];
		string_frameNum[0] = files[curFile][0];
		string_frameNum[1] = files[curFile][1];
		string_frameNum[2] = files[curFile][2];
		string_frameNum[3] = files[curFile][3];
		string_frameNum[4] = '\0';
		frame.frameNum = atoi(string_frameNum);

		compressedFrames[curFile] = frame;

		//if(frame.frameNum == curFrame){
			pthread_cond_signal(&ready);
		//}

		free(full_path);
		
	}

	//fclose(f_out);
}

void* writer(void* arg){
	//printf("test 1\n");
	while(curFrame < nfiles){
		pthread_mutex_lock(&lock);
		//printf("test 2\n");
		while(compressedFrames[curFrame-1].frame == NULL){
			//printf("test 3\n");
			pthread_cond_wait(&ready, &lock);
		}
		pthread_mutex_unlock(&lock);
		//printf("test 4\n");
		// dump zipped file
		int nbytes_zipped = compressedFrames[curFrame-1].nbytes;
		//printf("test 5\n");
		fwrite(&nbytes_zipped, sizeof(int), 1, f_out);
		//printf("test 6\n");
		fwrite(compressedFrames[curFrame-1].frame, sizeof(unsigned char), nbytes_zipped, f_out);
		//printf("test 7\n");
		total_out += nbytes_zipped;
		//printf("test 8\n");
		curFrame++;
		//printf("test 9\n");
	}

	/*while(writtenFrames < nfiles){
        //pthread_mutex_lock(&lock);
        // Find the next frame to write based on frameNum
        for(int i = 0; i < nfiles; i++){
            if(compressedFrames[i].frame != NULL && compressedFrames[i].frameNum == curFrame){
                // Write the frame
                int nbytes_zipped = compressedFrames[i].nbytes;
                fwrite(&nbytes_zipped, sizeof(int), 1, f_out);
                fwrite(compressedFrames[i].frame, sizeof(unsigned char), nbytes_zipped, f_out);
                total_out += nbytes_zipped;
                curFrame++;
                writtenFrames++;
                compressedFrames[i].frame = NULL; // Mark as written
                break; // Break after writing to start the search for the next frame
            }
        }*/
        //pthread_mutex_unlock(&lock);
        // Optionally, sleep for a short duration if no frame was written in this iteration
    //}

	/*while(writtenFrames < nfiles){
		int nbytes_zipped = compressedFrames[i].nbytes;
		fwrite(&nbytes_zipped, sizeof(int), 1, f_out);
		fwrite(compressedFrames[i].frame, sizeof(unsigned char), nbytes_zipped, f_out);
		total_out += nbytes_zipped;
		curFrame++;
		writtenFrames++;
		compressedFrames[i].frame = NULL; // Mark as written
		break; // Break after writing to start the search for the next frame
	}*/

    /*while(writtenFrames < nfiles){
        pthread_mutex_lock(&lock);
        // Check if the next frame is ready
        int found = 0;
        for(int i = 0; i < nfiles; i++){
            if(compressedFrames[i].frame != NULL && compressedFrames[i].frameNum == curFrame){
                // Prepare to write the frame, but don't write it yet
                found = 1;
                break; // Found the next frame to write
            }
        }
        if (!found) {
            // If the next frame isn't ready, wait for a signal
            pthread_cond_wait(&ready, &lock);
            // After waking up, re-check the condition in the next iteration of the while loop
            pthread_mutex_unlock(&lock);
            continue;
        }
        // If we found the frame, we still hold the lock and can safely access shared data

        // Extract necessary data for writing outside the lock
        int frameIndex = curFrame - 1; // Adjust based on your indexing
        int nbytes_zipped = compressedFrames[frameIndex].nbytes;
        unsigned char* frameData = compressedFrames[frameIndex].frame;
        compressedFrames[frameIndex].frame = NULL; // Mark as written
        curFrame++;
        writtenFrames++;

        pthread_mutex_unlock(&lock);

        // Perform the actual file writing outside of the critical section
        fwrite(&nbytes_zipped, sizeof(int), 1, f_out);
        fwrite(frameData, sizeof(unsigned char), nbytes_zipped, f_out);
        free(frameData); // Don't forget to free the memory
    }*/

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
	//printf("test 1\n");
	DIR *d;
	struct dirent *dir;
	

	d = opendir(argv[1]);
	if(d == NULL) {
		printf("An error has occurred\n");
		return 0;
	}
	//printf("test 2\n");
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
	//qsort(files, nfiles, sizeof(char *), cmp);

	//printf("test 3\n");

	compressedFrames = malloc(nfiles * sizeof(compressedFrame));
    assert(compressedFrames != NULL);
	for(int i = 0; i < nfiles; i++){
		compressedFrames[i].frame = NULL;
	}
	//printf("test 4\n");
	pthread_t consumers[NUM_CONSUMERS];
	for(int i = 0; i < NUM_CONSUMERS; i++){
		pthread_create(&consumers[i], NULL, consumer, (void*)argv[1]);
	}
	//printf("test 5\n");
	f_out = fopen("video.vzip", "w");
	assert(f_out != NULL);
	//printf("test 6\n");
	pthread_t fileWriter;
	pthread_create(&fileWriter, NULL, writer, NULL);
	
	for(int i = 0; i < NUM_CONSUMERS; i++){
		pthread_join(consumers[i], NULL);
	}
	
	pthread_join(fileWriter, NULL);
	//printf("test 7\n");
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
