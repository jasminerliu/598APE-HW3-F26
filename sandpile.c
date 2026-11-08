#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>


#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

static void die(const char *s) {
  fprintf(stderr, "%s\n", s);
  exit(1);
}

int N = 801;
static int *grid; // changed to flat array
static int *lo, *hi; // to track active range per row
static int W;

#define G(i, j) grid[((size_t)(i)+1) * W + (j) + 1]

static void precompute_row_ranges(void) {
  lo = malloc(N * sizeof(int));
  hi = malloc(N * sizeof(int));
  for (int i = 0; i < N; ++i) {
    lo[i] = N;
    hi[i] = -1;
    for (int j = 0; j <= MAX(i, N - 1 - i); ++j) {
      if ((i - N / 2) * (i - N / 2) + (j - N / 2) * (j - N / 2) <=
          (N / 2) * (N / 2)) {
        if (lo[i] == N) lo[i] = j;
        hi[i] = j;
      }
    }
  }
}
// you can also use this function to debug print your sandpile
static void render(char *filename) {
  int chan = 3;
  unsigned char *pixels = (unsigned char *)malloc(N * N * chan * sizeof(char));
  int stride = N * chan;
  if (!pixels)
    die("malloc failed for PNG pixel buffer");
  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; ++j) {
      unsigned char r = 0, g = 0, b = 0;
      switch (G(i,j)) {
      case 0: // obsidian
        r = 15;
        g = 15;
        b = 25;
        break;
      case 1: // electric cyan
        r = 0;
        g = 210;
        b = 255;
        break;
      case 2: // vibrant magenta
        r = 230;
        g = 30;
        b = 130;
        break;
      case 3: // bright gold
        r = 255;
        g = 200;
        b = 0;
        break;
      default: // white
        r = 0;
        g = 0;
        b = 0;
      }

      int pid = (i * N + j) * chan;
      pixels[pid] = r;
      pixels[pid + 1] = g;
      pixels[pid + 2] = b;
    }
  }
  if (stbi_write_png(filename, N, N, chan, pixels, stride)) {
    printf("Successfully saved sandpile identity to: %s\n", filename);
  } else {
    die("Error: stb_image_write failed to save png file\n");
  }
  free(pixels);
}

static void stabilize(void) {
  int changed;
  do {
    changed = 0;
    for (int c = 0; c < 3; ++c) {
      #pragma omp parallel for reduction(|:changed) schedule(static)
      for (int i = c; i < N; i += 3) {
        int *row = &G(i, 0);
        for (int j = lo[i]; j <= hi[i]; ++j) {
          int v = row[j];
          if (v < 4) continue;
          int t = v >> 2;
          changed = 1;
          row[j] = v & 3;
          row[j - 1] += t;
          row[j + 1] += t;
          row[j - W] += t;
          row[j + W] += t;
        }
      }
    }
  } while (changed);
}

void find_identity() {
  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; ++j) {
      G(i,j) = 6;
    }
  }
  stabilize();
  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; ++j) {
      G(i,j) = 6 - G(i,j);
    }
  }
  stabilize();
}

int main(int argc, char **argv) {
  if (argc < 2 || argc > 2) {
    fprintf(stderr, "Usage: %s N\n", argv[0]);
    return EXIT_FAILURE;
  }
  char *end;
  uintmax_t parsed = strtoumax(argv[1], &end, 10);
  if (errno || argv[1][0] < '0' || argv[1][0] > '9' || *end || !parsed ||
      parsed > INT32_MAX)
    die("grid size must be a positive int");
  N = (int)parsed;

  W = N + 2;
  grid = calloc((size_t)W * W, sizeof(int));
  if (!grid) die("malloc failed");

  precompute_row_ranges();

  find_identity();

  render("identity.png");
  free(grid); free(lo); free(hi);

  return EXIT_SUCCESS;
}
