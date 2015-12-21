#include "hcRNG/mrg32k3a.h"
#include "hcRNG/hcRNG.h"
#include <stdlib.h>
#define BLOCK_SIZE 256
#if defined ( WIN32 )
#define __func__ __FUNCTION__
#endif
#define MODULAR_NUMBER_TYPE unsigned long
#define MODULAR_FIXED_SIZE 3
#include "../include/hcRNG/private/modular.c.h"

// code that is common to host and device
#include "../include/hcRNG/private/mrg32k3a.c.h"

struct hcrngMrg32k3aStreamCreator_ {
	hcrngMrg32k3aStreamState initialState;
	hcrngMrg32k3aStreamState nextState;
	/*! @brief Jump matrices for advancing the initial seed of streams
	*/
	unsigned long nuA1[3][3];
	unsigned long nuA2[3][3];
};

/*! @brief Matrices to advance to the next state
*/
static unsigned long Mrg32k3a_A1p0[3][3] = {
	{ 0, 1, 0 },
	{ 0, 0, 1 },
	{ 4294156359, 1403580, 0 }
};

static unsigned long Mrg32k3a_A2p0[3][3] = {
	{ 0, 1, 0 },
	{ 0, 0, 1 },
	{ 4293573854, 0, 527612 }
};


/*! @brief Inverse of Mrg32k3a_A1p0 mod Mrg32k3a_M1
*
*  Matrices to go back to the previous state.
*/
static unsigned long invA1[3][3] = {
	{ 184888585, 0, 1945170933 },
	{ 1, 0, 0 },
	{ 0, 1, 0 }
};

// inverse of Mrg32k3a_A2p0 mod Mrg32k3a_M1
static unsigned long invA2[3][3] = {
	{ 0, 360363334, 4225571728 },
	{ 1, 0, 0 },
	{ 0, 1, 0 }
};


/*! @brief Default initial seed of the first stream
*/
#define BASE_CREATOR_STATE { { 12345, 12345, 12345 }, { 12345, 12345, 12345 } }
/*! @brief Jump matrices for \f$2^{127}\f$ steps forward
*/
#define BASE_CREATOR_JUMP_MATRIX_1 { \
        {2427906178, 3580155704, 949770784}, \
        { 226153695, 1230515664, 3580155704}, \
        {1988835001, 986791581, 1230515664} }
#define BASE_CREATOR_JUMP_MATRIX_2 { \
        { 1464411153, 277697599, 1610723613}, \
        {32183930, 1464411153, 1022607788}, \
        {2824425944, 32183930, 2093834863} }

/*! @brief Default stream creator (defaults to \f$2^{134}\f$ steps forward)
*
*  Contains the default seed and the transition matrices to jump \f$\nu\f$ steps forward;
*  adjacent streams are spaced nu steps apart.
*  The default is \f$nu = 2^{134}\f$.
*  The default seed is \f$(12345,12345,12345,12345,12345,12345)\f$.
*/
static hcrngMrg32k3aStreamCreator defaultStreamCreator = {
	BASE_CREATOR_STATE,
	BASE_CREATOR_STATE,
	BASE_CREATOR_JUMP_MATRIX_1,
	BASE_CREATOR_JUMP_MATRIX_2
};

/*! @brief Check the validity of a seed for Mrg32k3a
*/
static hcrngStatus validateSeed(const hcrngMrg32k3aStreamState* seed)
{
	// Check that the seeds have valid values
	for (size_t i = 0; i < 3; ++i)
		if (seed->g1[i] >= Mrg32k3a_M1)
			return hcrngSetErrorString(HCRNG_INVALID_SEED, "seed.g1[%u] >= Mrg32k3a_M1", i);

	for (size_t i = 0; i < 3; ++i)
		if (seed->g2[i] >= Mrg32k3a_M2)
			return hcrngSetErrorString(HCRNG_INVALID_SEED, "seed.g2[%u] >= Mrg32k3a_M2", i);

	if (seed->g1[0] == 0 && seed->g1[1] == 0 && seed->g1[2] == 0)
		return hcrngSetErrorString(HCRNG_INVALID_SEED, "seed.g1 = (0,0,0)");

	if (seed->g2[0] == 0 && seed->g2[1] == 0 && seed->g2[2] == 0)
		return hcrngSetErrorString(HCRNG_INVALID_SEED, "seed.g2 = (0,0,0)");

	return HCRNG_SUCCESS;
}

hcrngMrg32k3aStreamCreator* hcrngMrg32k3aCopyStreamCreator(const hcrngMrg32k3aStreamCreator* creator, hcrngStatus* err)
{
	hcrngStatus err_ = HCRNG_SUCCESS;

	// allocate creator
	hcrngMrg32k3aStreamCreator* newCreator = (hcrngMrg32k3aStreamCreator*)malloc(sizeof(hcrngMrg32k3aStreamCreator));

	if (newCreator == NULL)
		// allocation failed
		err_ = hcrngSetErrorString(HCRNG_OUT_OF_RESOURCES, "%s(): could not allocate memory for stream creator", __func__);
	else {
		if (creator == NULL)
			creator = &defaultStreamCreator;
		// initialize creator
		*newCreator = *creator;
	}

	// set error status if needed
	if (err != NULL)
		*err = err_;

	return newCreator;
}

hcrngStatus hcrngMrg32k3aDestroyStreamCreator(hcrngMrg32k3aStreamCreator* creator)
{
	if (creator != NULL)
		free(creator);
	return HCRNG_SUCCESS;
}

hcrngStatus hcrngMrg32k3aRewindStreamCreator(hcrngMrg32k3aStreamCreator* creator)
{
	if (creator == NULL)
		creator = &defaultStreamCreator;
	creator->nextState = creator->initialState;
	return HCRNG_SUCCESS;
}

hcrngStatus hcrngMrg32k3aSetBaseCreatorState(hcrngMrg32k3aStreamCreator* creator, const hcrngMrg32k3aStreamState* baseState)
{
	//Check params
	if (creator == NULL)
		return hcrngSetErrorString(HCRNG_INVALID_STREAM_CREATOR, "%s(): modifying the default stream creator is forbidden", __func__);
	if (baseState == NULL)
		return hcrngSetErrorString(HCRNG_INVALID_VALUE, "%s(): baseState cannot be NULL", __func__);

	hcrngStatus err = validateSeed(baseState);

	if (err == HCRNG_SUCCESS) {
		// initialize new creator
		creator->initialState = creator->nextState = *baseState;
	}

	return err;
}

hcrngStatus hcrngMrg32k3aChangeStreamsSpacing(hcrngMrg32k3aStreamCreator* creator, int e, int c)
{
	//Check params
	if (creator == NULL)
		return hcrngSetErrorString(HCRNG_INVALID_STREAM_CREATOR, "%s(): modifying the default stream creator is forbidden", __func__);
	if (e < 0)
		return hcrngSetErrorString(HCRNG_INVALID_VALUE, "%s(): e must be >= 0", __func__);

	unsigned long B[3][3];

	if (c >= 0)
		modMatPow(Mrg32k3a_A1p0, creator->nuA1, Mrg32k3a_M1, c);
	else
		modMatPow(invA1, creator->nuA1, Mrg32k3a_M1, -c);
	if (e > 0) {
		modMatPowLog2(Mrg32k3a_A1p0, B, Mrg32k3a_M1, e);
		modMatMat(B, creator->nuA1, creator->nuA1, Mrg32k3a_M1);
	}

	if (c >= 0)
		modMatPow(Mrg32k3a_A2p0, creator->nuA2, Mrg32k3a_M2, c);
	else
		modMatPow(invA2, creator->nuA2, Mrg32k3a_M2, -c);
	if (e > 0) {
		modMatPowLog2(Mrg32k3a_A2p0, B, Mrg32k3a_M2, e);
		modMatMat(B, creator->nuA2, creator->nuA2, Mrg32k3a_M2);
	}

	return HCRNG_SUCCESS;
}

hcrngMrg32k3aStream* hcrngMrg32k3aAllocStreams(size_t count, size_t* bufSize, hcrngStatus* err)
{
	hcrngStatus err_ = HCRNG_SUCCESS;
	size_t bufSize_ = count * sizeof(hcrngMrg32k3aStream);

	// allocate streams
	hcrngMrg32k3aStream* buf = (hcrngMrg32k3aStream*)malloc(bufSize_);

	if (buf == NULL) {
		// allocation failed
		err_ = hcrngSetErrorString(HCRNG_OUT_OF_RESOURCES, "%s(): could not allocate memory for streams", __func__);
		bufSize_ = 0;
	}

	// set buffer size if needed
	if (bufSize != NULL)
		*bufSize = bufSize_;

	// set error status if needed
	if (err != NULL)
		*err = err_;

	return buf;
}

hcrngStatus hcrngMrg32k3aDestroyStreams(hcrngMrg32k3aStream* streams)
{
	if (streams != NULL)
		free(streams);
	return HCRNG_SUCCESS;
}

static hcrngStatus Mrg32k3aCreateStream(hcrngMrg32k3aStreamCreator* creator, hcrngMrg32k3aStream* buffer)
{
	//Check params
	if (buffer == NULL)
		return hcrngSetErrorString(HCRNG_INVALID_VALUE, "%s(): buffer cannot be NULL", __func__);

	// use default creator if not given
	if (creator == NULL)
		creator = &defaultStreamCreator;

	// initialize stream
	buffer->current = buffer->initial = buffer->substream = creator->nextState;

	// advance next state in stream creator
	modMatVec(creator->nuA1, creator->nextState.g1, creator->nextState.g1, Mrg32k3a_M1);
	modMatVec(creator->nuA2, creator->nextState.g2, creator->nextState.g2, Mrg32k3a_M2);

	return HCRNG_SUCCESS;
}

hcrngStatus hcrngMrg32k3aCreateOverStreams(hcrngMrg32k3aStreamCreator* creator, size_t count, hcrngMrg32k3aStream* streams)
{
	// iterate over all individual stream buffers
	for (size_t i = 0; i < count; i++) {

		hcrngStatus err = Mrg32k3aCreateStream(creator, &streams[i]);

		// abort on error
		if (err != HCRNG_SUCCESS)
			return err;
	}

	return HCRNG_SUCCESS;
}

hcrngMrg32k3aStream* hcrngMrg32k3aCreateStreams(hcrngMrg32k3aStreamCreator* creator, size_t count, size_t* bufSize, hcrngStatus* err)
{
	hcrngStatus err_;
	size_t bufSize_;
	hcrngMrg32k3aStream* streams = hcrngMrg32k3aAllocStreams(count, &bufSize_, &err_);

	if (err_ == HCRNG_SUCCESS)
		err_ = hcrngMrg32k3aCreateOverStreams(creator, count, streams);

	if (bufSize != NULL)
		*bufSize = bufSize_;

	if (err != NULL)
		*err = err_;

	return streams;
}

hcrngMrg32k3aStream* hcrngMrg32k3aCopyStreams(size_t count, const hcrngMrg32k3aStream* streams, hcrngStatus* err)
{
	hcrngStatus err_ = HCRNG_SUCCESS;
	hcrngMrg32k3aStream* dest = NULL;

	//Check params
	if (streams == NULL)
		err_ = hcrngSetErrorString(HCRNG_INVALID_VALUE, "%s(): stream cannot be NULL", __func__);

	if (err_ == HCRNG_SUCCESS)
		dest = hcrngMrg32k3aAllocStreams(count, NULL, &err_);

	if (err_ == HCRNG_SUCCESS)
		err_ = hcrngMrg32k3aCopyOverStreams(count, dest, streams);

	if (err != NULL)
		*err = err_;

	return dest;
}

hcrngMrg32k3aStream* hcrngMrg32k3aMakeSubstreams(hcrngMrg32k3aStream* stream, size_t count, size_t* bufSize, hcrngStatus* err)
{
	hcrngStatus err_;
	size_t bufSize_;
	hcrngMrg32k3aStream* substreams = hcrngMrg32k3aAllocStreams(count, &bufSize_, &err_);

	if (err_ == HCRNG_SUCCESS)
		err_ = hcrngMrg32k3aMakeOverSubstreams(stream, count, substreams);
	if (bufSize != NULL)
		*bufSize = bufSize_;

	if (err != NULL)
		*err = err_;

	return substreams;
}

hcrngStatus hcrngMrg32k3aAdvanceStreams(size_t count, hcrngMrg32k3aStream* streams, int e, int c)
{
	//Check params
	if (streams == NULL)
		return hcrngSetErrorString(HCRNG_INVALID_VALUE, "%s(): streams cannot be NULL", __func__);

	//Advance Stream
	unsigned long B1[3][3], C1[3][3], B2[3][3], C2[3][3];

	// if e == 0, do not add 2^0; just behave as in docs
	if (e > 0) {
		modMatPowLog2(Mrg32k3a_A1p0, B1, Mrg32k3a_M1, e);
		modMatPowLog2(Mrg32k3a_A2p0, B2, Mrg32k3a_M2, e);
	}
	else if (e < 0) {
		modMatPowLog2(invA1, B1, Mrg32k3a_M1, -e);
		modMatPowLog2(invA2, B2, Mrg32k3a_M2, -e);
	}

	if (c >= 0) {
		modMatPow(Mrg32k3a_A1p0, C1, Mrg32k3a_M1, c);
		modMatPow(Mrg32k3a_A2p0, C2, Mrg32k3a_M2, c);
	}
	else {
		modMatPow(invA1, C1, Mrg32k3a_M1, -c);
		modMatPow(invA2, C2, Mrg32k3a_M2, -c);
	}

	if (e) {
		modMatMat(B1, C1, C1, Mrg32k3a_M1);
		modMatMat(B2, C2, C2, Mrg32k3a_M2);
	}

	for (size_t i = 0; i < count; i++) {
		modMatVec(C1, streams[i].current.g1, streams[i].current.g1, Mrg32k3a_M1);
		modMatVec(C2, streams[i].current.g2, streams[i].current.g2, Mrg32k3a_M2);
	}

	return HCRNG_SUCCESS;
}

hcrngStatus hcrngMrg32k3aWriteStreamInfo(const hcrngMrg32k3aStream* stream, FILE *file)
{
	//Check params
	if (stream == NULL)
		return hcrngSetErrorString(HCRNG_INVALID_VALUE, "%s(): stream cannot be NULL", __func__);
	if (file == NULL)
		return hcrngSetErrorString(HCRNG_INVALID_VALUE, "%s(): file cannot be NULL", __func__);

	// The Initial state of the Stream
	fprintf(file, "\n   initial = { ");
	for (size_t i = 0; i < 3; i++)
		fprintf(file, "%lu, ", stream->initial.g1[i]);

	for (size_t i = 0; i < 2; i++)
		fprintf(file, "%lu, ", stream->initial.g2[i]);

	fprintf(file, "%lu }\n", stream->initial.g2[2]);
	//The Current state of the Stream
	fprintf(file, "\n   Current = { ");
	for (size_t i = 0; i < 3; i++)
		fprintf(file, "%lu, ", stream->current.g1[i]);

	for (size_t i = 0; i < 2; i++)
		fprintf(file, "%lu, ", stream->current.g2[i]);

	fprintf(file, "%lu }\n", stream->current.g2[2]);

	return HCRNG_SUCCESS;
}

hcrngStatus hcrngMrg32k3aDeviceRandomU01Array_single(size_t streamCount, Concurrency::array_view<hcrngMrg32k3aStream> &streams,
	size_t numberCount, Concurrency::array_view<float> &outBuffer)
{
#define HCRNG_SINGLE_PRECISION
	//Check params
	if (streamCount < 1)
		return hcrngSetErrorString(HCRNG_INVALID_VALUE, "%s(): streamCount cannot be less than 1", __func__);
	if (numberCount < 1)
		return hcrngSetErrorString(HCRNG_INVALID_VALUE, "%s(): numberCount cannot be less than 1", __func__);
        std::vector<Concurrency::accelerator>acc = Concurrency::accelerator::get_all();
        accelerator_view accl_view = (acc[1].create_view());
        hcrngStatus status = HCRNG_SUCCESS;
        long size = (streamCount + BLOCK_SIZE - 1) & ~(BLOCK_SIZE - 1);
        Concurrency::extent<1> grdExt(size);
        Concurrency::tiled_extent<BLOCK_SIZE> t_ext(grdExt);
        std::cout << "inside function" << std::endl;
        Concurrency::parallel_for_each(accl_view, t_ext, [ = ] (Concurrency::tiled_index<BLOCK_SIZE> tidx) restrict(amp) {
          int gid = tidx.global[0];
          hcrngMrg32k3aStream local_stream;
          hcrngMrg32k3aCopyOverStreamsFromGlobal(1, &local_stream, &streams[gid]);
           if(gid < streamCount){
            for(int i =0; i < numberCount/streamCount; i++)
              outBuffer[i * streamCount + gid] = hcrngMrg32k3aRandomU01(&local_stream);
           }
        });
#undef HCRNG_SINGLE_PRECISION
        return status;
}


hcrngStatus hcrngMrg32k3aDeviceRandomU01Array_double(size_t streamCount, Concurrency::array_view<hcrngMrg32k3aStream> &streams,
        size_t numberCount, Concurrency::array_view<double> &outBuffer)
{
        //Check params
        if (streamCount < 1)
                return hcrngSetErrorString(HCRNG_INVALID_VALUE, "%s(): streamCount cannot be less than 1", __func__);
        if (numberCount < 1)
                return hcrngSetErrorString(HCRNG_INVALID_VALUE, "%s(): numberCount cannot be less than 1", __func__);
        std::vector<Concurrency::accelerator>acc = Concurrency::accelerator::get_all();
        accelerator_view accl_view = (acc[1].create_view());
        hcrngStatus status = HCRNG_SUCCESS;
        long size = (streamCount + BLOCK_SIZE - 1) & ~(BLOCK_SIZE - 1);
        Concurrency::extent<1> grdExt(size);
        Concurrency::tiled_extent<BLOCK_SIZE> t_ext(grdExt);
        Concurrency::parallel_for_each(accl_view, t_ext, [ = ] (Concurrency::tiled_index<BLOCK_SIZE> tidx) restrict(amp) {
           int gid = tidx.global[0];
           hcrngMrg32k3aStream local_stream;
           hcrngMrg32k3aCopyOverStreamsFromGlobal(1, &local_stream, &streams[gid]);
           if(gid < streamCount){
            for(int i =0; i < numberCount/streamCount; i++)
              outBuffer[i * streamCount + gid] = hcrngMrg32k3aRandomU01(&local_stream);
           }
        });
        return status;
}

#if 0
hcrngMrg32k3aStream* Mrg32k3aGetStreamByIndex(hcrngMrg32k3aStream* stream, unsigned int index)
{

	return &stream[index];

}
#endif
