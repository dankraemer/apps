#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>
#include <string.h>

static FILE *fd_input;
static FILE *fd_output;

static struct timeval tv_old;
static struct timeval tv_diff;

static struct timeval tv_zero;
static struct timeval tv_zero_diff;

typedef struct ourdebug {
  uint16_t index;
  uint16_t missing;
  struct timeval tv;
} debugito;

int main(int argc, char ** argv) {
 
  int i;
  debugito d;

  if ((fd_input = fopen(argv[1], "rb")) < 0) {
    printf("Unable to open file");
    return -2;
  }

  tv_old  = d.tv;
  tv_zero = d.tv;

  while(!feof(fd_input)) {

    fread(&d, sizeof(debugito), 1, fd_input);
    
    timersub(&d.tv, &tv_old, &tv_diff);
    timersub(&d.tv, &tv_zero, &tv_zero_diff);

    if (d.missing) {

      printf("T(%05d) [%d]! %ld.%06ld %ld.%06ld %ld.%06ld \n", i++, d.index, 
                                                              d.tv.tv_sec, d.tv.tv_usec, 
                                                              tv_diff.tv_sec, tv_diff.tv_usec,
                                                              tv_zero_diff.tv_sec, tv_zero_diff.tv_usec);
    } else {

      printf("T(%05d) [%d]_ %ld.%06ld %ld.%06ld %ld.%06ld \n", i++, d.index, 
                                                              d.tv.tv_sec, d.tv.tv_usec, 
                                                              tv_diff.tv_sec, tv_diff.tv_usec,
                                                              tv_zero_diff.tv_sec, tv_zero_diff.tv_usec);
    }

    if (d.index == 0 ) {
      tv_zero = d.tv;
    }

    tv_old = d.tv;
  }

  fclose(fd_input);

  return 0;
}

