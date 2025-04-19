#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <curl/curl.h>
#include <semaphore.h>
#include <unistd.h>

#define threadnumber 4

typedef struct 
{
     char url[2048];
     char filename[1024];
     long start;
     long end;
     int id;
    
}downloadsegmentdata;

long downloaded_bytes =0;
long total_size=0;
pthread_mutex_t write_mutex;
sem_t write_semaphore;

size_t write_data(void* ptr, size_t size, size_t numberOfmemb, void* stream)
{
    FILE* fp = (FILE*)stream;

    size_t written_bytes = fwrite(ptr, size, numberOfmemb, fp);

    pthread_mutex_lock(&write_mutex);

    downloaded_bytes += written_bytes;

    pthread_mutex_unlock(&write_mutex);


    return written_bytes;
}

long get_file_size(const char* url)
{
    CURL* curl = curl_easy_init();
    if (!curl) 
    {
        return -1;
    }
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode result=curl_easy_perform(curl);

    double size=0;

    if(result==CURLE_OK)
    {
        curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &size);

    }
    curl_easy_cleanup(curl);
    return (long)size;

}

