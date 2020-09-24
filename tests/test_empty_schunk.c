/*
  Copyright (C) 2019  Francesc Alted
  http://blosc.org
  License: BSD (see LICENSE.txt)

  Creation date: 2019-08-06

  See LICENSE.txt for details about copyright and rights to use.
*/

#include <stdio.h>
#include "test_common.h"

#define CHUNKSIZE (200 * 1000)
#define NTHREADS (2)

/* Global vars */
int tests_run = 0;
int nchunks;
bool copy;


static char* test_schunk(void) {
  static int32_t data[CHUNKSIZE];
  static int32_t data_dest[CHUNKSIZE];
  size_t isize = CHUNKSIZE * sizeof(int32_t);
  int dsize;
  int64_t nbytes, cbytes;
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_schunk* schunk;

  /* Initialize the Blosc compressor */
  blosc_init();

  /* Create a super-chunk container */
  cparams.typesize = sizeof(int32_t);
  cparams.compcode = BLOSC_BLOSCLZ;
  cparams.clevel = 5;
  cparams.nthreads = NTHREADS;
  dparams.nthreads = NTHREADS;
  schunk = blosc2_empty_schunk(cparams, dparams, nchunks, NULL);

  // Add a couple of metalayers
  blosc2_add_metalayer(schunk, "metalayer1", (uint8_t*)"my metalayer1", sizeof("my metalayer1"));
  blosc2_add_metalayer(schunk, "metalayer2", (uint8_t*)"my metalayer1", sizeof("my metalayer1"));

  uint8_t *chunk;
  bool needs_free;
  cbytes = blosc2_schunk_get_chunk(schunk, nchunks / 2, &chunk, &needs_free);
  mu_assert("ERROR: chunk cannot be retrieved correctly.\n", cbytes == 0);

  int32_t datasize = sizeof(int32_t) * CHUNKSIZE;
  int32_t chunksize = sizeof(int32_t) * CHUNKSIZE + BLOSC_MAX_OVERHEAD;

  // Feed it with data
  for (int nchunk = 0; nchunk < nchunks; nchunk++) {
    for (int i = 0; i < CHUNKSIZE; i++) {
      data[i] = i + nchunk * CHUNKSIZE;
    }

    chunk = malloc(chunksize);
    int csize = blosc2_compress_ctx(schunk->cctx, data, datasize, chunk, chunksize);
    mu_assert("ERROR: chunk cannot be compressed", csize >= 0);

    int nchunks_ = blosc2_schunk_update_chunk(schunk, nchunk, chunk, copy);
    mu_assert("ERROR: bad append in schunk", nchunks_ == nchunks);
    if (!copy) {
      chunk = malloc(chunksize);
    }
    nchunks_ = blosc2_schunk_update_chunk(schunk, nchunk, chunk, copy);

    mu_assert("ERROR: bad append in schunk", nchunks_ == nchunks);

    if (copy) {
      free(chunk);
    }
  }

  blosc2_update_metalayer(schunk, "metalayer2", (uint8_t*)"my metalayer2", sizeof("my metalayer2"));
  // Attach some user metadata into it
  blosc2_update_usermeta(schunk, (uint8_t *) "testing the usermeta", 16, BLOSC2_CPARAMS_DEFAULTS);

  /* Gather some info */
  nbytes = schunk->nbytes;
  cbytes = schunk->cbytes;
  if (nchunks > 0) {
    mu_assert("ERROR: bad compression ratio in frame", nbytes > 10 * cbytes);
  }

  // Exercise the metadata retrieval machinery
  size_t nbytes_, cbytes_, blocksize;
  nbytes = 0;
  cbytes = 0;
  for (int nchunk = 0; nchunk < nchunks; nchunk++) {
    cbytes_ = blosc2_schunk_get_chunk(schunk, nchunk, &chunk, &needs_free);
    mu_assert("ERROR: chunk cannot be retrieved correctly.", cbytes_ >= 0);
    blosc_cbuffer_sizes(chunk, &nbytes_, &cbytes_, &blocksize);
    nbytes += nbytes_;
    cbytes += cbytes_;
    if (needs_free) {
      free(chunk);
    }
  }
  mu_assert("ERROR: nbytes is not correct", nbytes == schunk->nbytes);
  mu_assert("ERROR: cbytes is not correct", cbytes == schunk->cbytes);

  // Check that the chunks have been decompressed correctly
  for (int nchunk = 0; nchunk < nchunks; nchunk++) {
    cbytes = blosc2_schunk_decompress_chunk(schunk, nchunk, (void *) data_dest, isize);
    mu_assert("ERROR: chunk cannot be decompressed correctly.", cbytes >= 0);
    for (int i = 0; i < CHUNKSIZE; i++) {
      mu_assert("ERROR: bad roundtrip",data_dest[i] == i + nchunk * CHUNKSIZE);
    }
  }

  // metalayers
  uint8_t* content;
  uint32_t content_len;
  blosc2_get_metalayer(schunk, "metalayer1", &content, &content_len);
  mu_assert("ERROR: bad metalayer content", strncmp((char*)content, "my metalayer1", content_len) == 0);
  free(content);
  blosc2_get_metalayer(schunk, "metalayer2", &content, &content_len);
  mu_assert("ERROR: bad metalayer content", strncmp((char*)content, "my metalayer2", content_len) == 0);
  free(content);

  // Check the usermeta
  uint8_t* content2;
  int32_t content2_len = blosc2_get_usermeta(schunk, &content2);
  mu_assert("ERROR: bad usermeta", strncmp((char*)content2, "testing the usermeta", 16) == 0);
  mu_assert("ERROR: bad usermeta_len", content2_len == 16);
  free(content2);

  /* Free resources */
  blosc2_free_schunk(schunk);
  /* Destroy the Blosc environment */
  blosc_destroy();

  return EXIT_SUCCESS;
}

static char *all_tests(void) {
  nchunks = 6;
  copy = true;
  mu_run_test(test_schunk);

  nchunks = 22;
  copy = false;
  mu_run_test(test_schunk);

  nchunks = 10;
  copy = true;
  mu_run_test(test_schunk);

  return EXIT_SUCCESS;
}


int main(void) {
  char *result;

  install_blosc_callback_test(); /* optionally install callback test */
  blosc_init();

  /* Run all the suite */
  result = all_tests();
  if (result != EXIT_SUCCESS) {
    printf(" (%s)\n", result);
  }
  else {
    printf(" ALL TESTS PASSED");
  }
  printf("\tTests run: %d\n", tests_run);

  blosc_destroy();

  return result != EXIT_SUCCESS;
}
