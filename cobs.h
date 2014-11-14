#ifndef __COBS_H__
#define __COBS_H__
#include <arduino.h>



class CobsDecoder {
public:
	uint8_t *dst ;   //Destination buffer
	uint8_t bytes; //number of bytes already decoded into the destination buffer

	uint8_t code;  //The last COBS code
	uint8_t count; //Count of bytes read since the last code point
	uint8_t start_of_chunk : 1; //true if the next input byte is a COBS code
	uint8_t end_of_frame  : 1;  //true if an EOF-zero was encountered

	uint8_t buf_len : 6; //Number of bytes in the in_buf
	uint8_t seeking_frame_end : 1; //true if next_frame() has been called and we're dropping bytes to find the end of the frame
	uint8_t buf_pos : 6; //Number of bytes consumed from in_buf
	uint8_t in_buf[32];  //Incoming bytes are buffered here for decoding

public:

	CobsDecoder() : dst(0), bytes(0), code(0), count(0), start_of_chunk(1), end_of_frame(0), buf_len(0), seeking_frame_end(0), buf_pos(0) { };

	/*
	 * Continues to attempt to decode into dst until max_bytes have been copied
	 *
	 * Returns the number of bytes remaining. Can return zero even if less than max_bytes has been copied due to reaching the end of a frame
	 *
	 */
	uint16_t decode_upto(Stream &stream, uint16_t max_bytes);

	/*
	 * Resets the buffer currently being filled by decode commands. Offset into this buffer is preserved
	 * across calls to decode_upto()
	 */
	void set_dst(void * __restrict dst) {
		this->dst   = (uint8_t*)dst;
		this->bytes = 0;
	}

	/*
	 * Returns true if the decoder is at the end of a frame
	 */
	bool is_end_of_frame() {
		return this->end_of_frame;
	}

	/*
	 * Advances to the next frame, discarding any remaining data in the current frame
	 */
	void next_frame() {
		if(this->end_of_frame) {
			this->end_of_frame = 0;
			this->start_of_chunk = 1;
		} else {
			this->seeking_frame_end = 1;
		}
	}
};

/**
 * Tail-end COBS encoding. To allow completely linear encoding, COBS bytes
 * are backwards-looking rather than forwards. If a 0xFF is encoded (because a run 
 * of more than 254 bytes w/o a 0), the last byte of the frame is the next count
 *
 * NOTE: Needs to be optimized to use more pointers, less counters
 *
 */
class TECobsEncoder {
	uint8_t tail[8]; //Enough for ~2kB
	uint8_t tail_count;
	uint16_t run_count;

	Stream &serial;
public:
	//run_count starts at 1 so that the last 0 encodes as before the start of the frame
	TECobsEncoder(Stream &s) : tail_count(0), run_count(1), serial(s) { };

	void encode(const uint8_t *, uint16_t len);
	void end_frame();
};

#endif