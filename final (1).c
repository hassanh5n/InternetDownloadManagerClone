#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <curl/curl.h>
#include <semaphore.h>
#include <unistd.h>

#define threadnumber 4

typedef struct {
    char url[2048];
    char filename[1024];
    long start;
    long end;
    int id;
} downloadsegmentdata;

long totaldownloadbytes = 0;
long totalsize = 0;
pthread_mutex_t writemutex;
sem_t writesemaphore;

size_t write_data(void* ptr, size_t size, size_t numberOfmemb, void* stream) {
    FILE* fp = (FILE*)stream;
    size_t writtenbytes = fwrite(ptr, size, numberOfmemb, fp);

    pthread_mutex_lock(&writemutex);
    totaldownloadbytes += writtenbytes;
    pthread_mutex_unlock(&writemutex);

    return writtenbytes;
}

long get_file_size(const char* url) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return -1;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L); 
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);  
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/58.0.3029.110 Safari/537.3"); // Add user-agent

    CURLcode result = curl_easy_perform(curl);

    if (result != CURLE_OK) {
        curl_easy_cleanup(curl);
        return -1;
    }

    // Get the content length
    curl_off_t size = 0;
    result = curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &size);
    
    if (result != CURLE_OK || size == -1) {
        curl_easy_cleanup(curl);
        return -1;
    }

    curl_easy_cleanup(curl);
    return (long)size;
}



void* download(void* arg) {
    downloadsegmentdata* segment = (downloadsegmentdata*)arg;
    CURL* curl = curl_easy_init();
    if (!curl) {
        pthread_exit(NULL);
    }

    char range[64];
    snprintf(range, sizeof(range), "%ld-%ld", segment->start, segment->end); 

    FILE* filep = fopen(segment->filename, "rb+");
    if (!filep) {
        fprintf(stderr, "Failed to open file for segment %d\n", segment->id); 
        pthread_exit(NULL);
    }

    fseek(filep, segment->start, SEEK_SET);

    curl_easy_setopt(curl, CURLOPT_URL, segment->url);
    curl_easy_setopt(curl, CURLOPT_RANGE, range); 
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, filep);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    sem_wait(&writesemaphore);
    CURLcode res = curl_easy_perform(curl);
    sem_post(&writesemaphore);

    if (res != CURLE_OK) {
        fprintf(stderr, "Thread failed (%d): %s\n", segment->id, curl_easy_strerror(res));
    }
    sleep(1);

    fclose(filep);
    curl_easy_cleanup(curl);
    pthread_exit(NULL);
}

void* display_progress(void* arg) {
    while (1) {
        pthread_mutex_lock(&writemutex);
        long progress = totaldownloadbytes;
        pthread_mutex_unlock(&writemutex);

        if (progress >= totalsize) break;

        printf("\rProgress: %ld%%", progress * 100 / totalsize);
        fflush(stdout);
        sleep(1);
    }

    printf("\r\nDownload Complete! 100%%\n");
    pthread_exit(NULL);
}

int main() {
    char url[2048];
    printf("Enter file URL: ");
    scanf("%s", url);

    totalsize = get_file_size(url);
    if (totalsize <= 0) {
        fprintf(stderr, "Failed to get file size. Exiting.\n");
        return 404;
    }

    char filename[100] = "output.mkv"; 

    FILE* file = fopen(filename, "wb");
    fseek(file, totalsize - 1, SEEK_SET);
    fputc('\0', file);
    fclose(file);

    pthread_mutex_init(&writemutex, NULL);
    sem_init(&writesemaphore, 0, threadnumber);

    pthread_t threads[threadnumber], progressthread;
    downloadsegmentdata segmnts[threadnumber];

    long chunk_size = totalsize / threadnumber;

    pthread_create(&progressthread, NULL, display_progress, NULL);

    for (int i = 0; i < threadnumber; ++i) {
        strcpy(segmnts[i].url, url);
        strcpy(segmnts[i].filename, filename);
        segmnts[i].start = i * chunk_size;
        segmnts[i].end = (i == threadnumber - 1) ? totalsize - 1 : (i + 1) * chunk_size - 1;
        segmnts[i].id = i;

        pthread_create(&threads[i], NULL, download, &segmnts[i]); 
    }

    for (int i = 0; i < threadnumber; ++i) {
        pthread_join(threads[i], NULL);
    }

    pthread_join(progressthread, NULL);
    sem_destroy(&writesemaphore);
    pthread_mutex_destroy(&writemutex);

    printf("File downloaded: %s\n", filename);
    return 0;
}
