/*
Copyright (c) 2015-2016 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

// Example on Multistream random number generation with Mrg31k3p generator
#include <assert.h>
#include <hc.hpp>
#include <hcRNG/hcRNG.h>
#include <hcRNG/mrg31k3p.h>
#include <hc_am.hpp>
#include <iostream>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HCRNG_SINGLE_PRECISION
#ifdef HCRNG_SINGLE_PRECISION
typedef float fp_type;
#else
typedef double fp_type;
#endif

// Multistream generation with host
void multistream_fill_array_normal(size_t spwi, size_t gsize, size_t quota,
                                   int substream_length,
                                   hcrngMrg31k3pStream *streams,
                                   fp_type *out_) {
  for (size_t i = 0; i < quota; i++) {
    for (size_t gid = 0; gid < gsize; gid++) {
      // Create streams
      hcrngMrg31k3pStream *s = &streams[spwi * gid];
      fp_type *out = &out_[spwi * (i * gsize + gid)];
      // Do nothing when subtsream_length is equal to 0
      if ((i > 0) && (substream_length > 0) && (i % substream_length == 0))
        // Forward to next substream when substream_length is greater than 0
        hcrngMrg31k3pForwardToNextSubstreams(spwi, s);
      else if ((i > 0) && (substream_length < 0) &&
               (i % (-substream_length) == 0))
        // Rewind substreams when substream_length is smaller than 0
        hcrngMrg31k3pRewindSubstreams(spwi, s);
      // Generate Random Numbers
      for (size_t sid = 0; sid < spwi; sid++) {
        out[sid] = hcrngMrg31k3pRandomN(&s[sid], &s[sid + 1], 0.0, 1.0);
      }
    }
  }
}

int main() {
  hcrngStatus status = HCRNG_SUCCESS;
  bool ispassed = 1;
  size_t streamBufferSize;
  // Number of streams
  size_t streamCount = 10;
  // Number of Random numbers to be generated (numberCount should be a multiple
  // of streamCount)
  size_t numberCount = 100;
  // Substream length
  // Substream_length       = 0   // do not use substreams
  // Substream_length       = > 0   // go to next substreams after
  // Substream_length values
  // Substream_length       = < 0  // restart substream after Substream_length
  // values
  int stream_length = 5;
  size_t streams_per_thread = 2;

  // Enumerate the list of accelerators
  std::vector<hc::accelerator> acc = hc::accelerator::get_all();
  hc::accelerator_view accl_view = (acc[1].get_default_view());
  // Allocate Host pointers
  fp_type *Random1 = (fp_type *)malloc(sizeof(fp_type) * numberCount);
  fp_type *Random2 = (fp_type *)malloc(sizeof(fp_type) * numberCount);
  // Allocate buffer for Device output
  fp_type *outBufferDevice_substream =
      hc::am_alloc(sizeof(fp_type) * numberCount, acc[1], 0);
  hcrngMrg31k3pStream *streams =
      hcrngMrg31k3pCreateStreams(NULL, streamCount, &streamBufferSize, NULL);
  hcrngMrg31k3pStream *streams_buffer =
      hc::am_alloc(sizeof(hcrngMrg31k3pStream) * streamCount, acc[1], 0);
  accl_view.copy(streams, streams_buffer,
                 streamCount * sizeof(hcrngMrg31k3pStream));

// Invoke Random number generator function in Device
#ifdef HCRNG_SINGLE_PRECISION
  status = hcrngMrg31k3pDeviceRandomNArray_single(
      accl_view, streamCount, streams_buffer, numberCount, 0.0, 1.0,
      outBufferDevice_substream, stream_length, streams_per_thread);
#else
  status = hcrngMrg31k3pDeviceRandomNArray_double(
      accl_view, streamCount, streams_buffer, numberCount, 0.0, 1.0,
      outBufferDevice_substream, stream_length, streams_per_thread);
#endif
  // Status check
  if (status) std::cout << "TEST FAILED" << std::endl;
  accl_view.copy(outBufferDevice_substream, Random1,
                 numberCount * sizeof(fp_type));

  // Invoke random number generator from Host
  multistream_fill_array_normal(
      streams_per_thread, streamCount / streams_per_thread,
      numberCount / streamCount, stream_length, streams, Random2);

  // Compare Host and device outputs
  for (int i = 0; i < numberCount; i++) {
    fp_type diff = std::abs(Random1[i] - Random2[i]);
    if (diff > 0.00001) {
      ispassed = 0;
      std::cout << " RANDDEVICE_SUBSTREAM[" << i << "] " << Random1[i]
                << "and RANDHOST_SUBSTREAM[" << i << "] mismatches"
                << Random2[i] << std::endl;
      break;
    } else {
      continue;
    }
  }
  if (!ispassed) std::cout << "TEST FAILED" << std::endl;

  // Free host resources
  free(Random1);
  free(Random2);

  // Release device resources
  hc::am_free(outBufferDevice_substream);
  hc::am_free(streams_buffer);

  return 0;
}

