#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <hcRNG/lfsr113.h>
#include <hcRNG/hcRNG.h>
#include <amp.h>
using namespace Concurrency;

int main()
{
        hcrngLfsr113Stream* stream = NULL;
        hcrngStatus status = HCRNG_SUCCESS;
        bool ispassed = 1;
        size_t streamBufferSize;
        size_t streamCount = 10;
        size_t numberCount = 100;
        int stream_length = -5;
        size_t streams_per_thread = 2;
        float *Random1 = (float*) malloc(sizeof(float) * numberCount);    
        float *Random2 = (float*) malloc(sizeof(float) * numberCount);
        Concurrency::array_view<float> outBufferDevice(numberCount, Random1);
        Concurrency::array_view<float> outBufferHost(numberCount, Random2);
        hcrngLfsr113Stream *streams = hcrngLfsr113CreateStreams(NULL, streamCount, &streamBufferSize, NULL);
        Concurrency::array_view<hcrngLfsr113Stream> streams_buffer(streamCount, streams);
        status = hcrngLfsr113DeviceRandomU01Array_single(streamCount, streams_buffer, numberCount, outBufferDevice);
        if(status) std::cout << "TEST FAILED" << std::endl;
        for (size_t i = 0; i < numberCount; i++)
            outBufferHost[i] = hcrngLfsr113RandomU01(&streams[i % streamCount]);   
        for(int i =0; i < numberCount; i++) {
           if (outBufferDevice[i] != outBufferHost[i]) {
                ispassed = 0;
                std::cout <<" RANDDEVICE[" << i<< "] " << outBufferDevice[i] << "and RANDHOST[" << i <<"] mismatches"<< outBufferHost[i] << std::endl;
                break;
            }
            else
                continue;
        }
        if(!ispassed) std::cout << "TEST FAILED" << std::endl;
        float *Random3 = (float*) malloc(sizeof(float) * numberCount);
        float *Random4 = (float*) malloc(sizeof(float) * numberCount);
        Concurrency::array_view<float> outBufferDevice_substream(numberCount, Random3);
        status = hcrngLfsr113DeviceRandomU01Array_single(streamCount, streams_buffer, numberCount, outBufferDevice_substream, stream_length, streams_per_thread);
        if(status) std::cout << "TEST FAILED" << std::endl;
      /*  for( int i =0 ; i < numberCount; i++)
            std::cout <<" RANDDEVICE[" << i<< "] " << outBufferDevice_substream[i] <<std::endl;//"\t and RANDHOST[" << i <<"]"<< Random4[i] << std::endl;
      */  return 0;
}


