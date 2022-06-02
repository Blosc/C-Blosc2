/*
  Copyright (C) 2021 The Blosc Developers
  http://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/

#include <stdio.h>
#include "blosc2.h"


void swap_store(void *dest, const void *pa, int size) {
  uint8_t *pa_ = (uint8_t *) pa;
  uint8_t pa2_[8];
  int i = 1; /* for big/little endian detection */
  char *p = (char *) &i;

  if (p[0] == 1) {
    /* little endian */
    switch (size) {
      case 8:
        pa2_[0] = pa_[7];
        pa2_[1] = pa_[6];
        pa2_[2] = pa_[5];
        pa2_[3] = pa_[4];
        pa2_[4] = pa_[3];
        pa2_[5] = pa_[2];
        pa2_[6] = pa_[1];
        pa2_[7] = pa_[0];
        break;
      case 4:
        pa2_[0] = pa_[3];
        pa2_[1] = pa_[2];
        pa2_[2] = pa_[1];
        pa2_[3] = pa_[0];
        break;
      case 2:
        pa2_[0] = pa_[1];
        pa2_[1] = pa_[0];
        break;
      case 1:
        pa2_[0] = pa_[0];
        break;
      default:
        fprintf(stderr, "Unhandled nitems: %d\n", size);
    }
  }
  memcpy(dest, pa2_, size);
}

int32_t deserialize_meta(uint8_t *smeta, int32_t smeta_len, int8_t *ndim, int64_t *shape,
                         int32_t *chunkshape, int32_t *blockshape) {
  BLOSC_UNUSED_PARAM(smeta_len);
  uint8_t *pmeta = smeta;

  // Check that we have an array with 5 entries (version, ndim, shape, chunkshape, blockshape)
  pmeta += 1;

  // version entry
  // int8_t version = (int8_t)pmeta[0];  // positive fixnum (7-bit positive integer) commented to avoid warning
  pmeta += 1;

  // ndim entry
  *ndim = (int8_t)pmeta[0];
  int8_t ndim_aux = *ndim;  // positive fixnum (7-bit positive integer)
  pmeta += 1;

  // shape entry
  // Initialize to ones, as required by Caterva
  for (int i = 0; i < 8; i++) shape[i] = 1;
  pmeta += 1;
  for (int8_t i = 0; i < ndim_aux; i++) {
    pmeta += 1;
    swap_store(shape + i, pmeta, sizeof(int64_t));
    pmeta += sizeof(int64_t);
  }

  // chunkshape entry
  // Initialize to ones, as required by Caterva
  for (int i = 0; i < 8; i++) chunkshape[i] = 1;
  pmeta += 1;
  for (int8_t i = 0; i < ndim_aux; i++) {
    pmeta += 1;
    swap_store(chunkshape + i, pmeta, sizeof(int32_t));
    pmeta += sizeof(int32_t);
  }

  // blockshape entry
  // Initialize to ones, as required by Caterva
  for (int i = 0; i < 8; i++) blockshape[i] = 1;
  pmeta += 1;
  for (int8_t i = 0; i < ndim_aux; i++) {
    pmeta += 1;
    swap_store(blockshape + i, pmeta, sizeof(int32_t));
    pmeta += sizeof(int32_t);
  }
  int32_t slen = (int32_t)(pmeta - smeta);
  return slen;
}

static void index_unidim_to_multidim(uint8_t ndim, int32_t *shape, int64_t i, int64_t *index) {
    int64_t strides[8];
    strides[ndim - 1] = 1;
    for (int j = ndim - 2; j >= 0; --j) {
        strides[j] = shape[j + 1] * strides[j + 1];
    }

    index[0] = i / strides[0];
    for (int j = 1; j < ndim; ++j) {
        index[j] = (i % strides[j - 1]) / strides[j];
    }
}

void index_multidim_to_unidim(const int64_t *index, int8_t ndim, const int64_t *strides, int64_t *i) {
  *i = 0;
  for (int j = 0; j < ndim; ++j) {
    *i += index[j] * strides[j];
  }
}
