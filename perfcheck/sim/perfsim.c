#include <stdio.h>
#include <sys/time.h>
#include <string.h>


static FILE *fd_output;

struct timeval tv;

int main(int argc, char ** argv) {
 
  int i;

  if ((fd_output = fopen("simulated", "wb")) < 0) {
    printf("Unable to open file");
    return -2;
  }

  for (i = 0; i < 10; i++ ) {

    tv.tv_sec  = i;
    tv.tv_usec = i*100;
    printf("Timestamp %ld.%06ld\n", tv.tv_sec, tv.tv_usec);

    fwrite(&tv, sizeof(struct timeval), 1, fd_output);

  }

  fclose(fd_output);
  return 0;
}

