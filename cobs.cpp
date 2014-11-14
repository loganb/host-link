#include "cobs.h"

#include <alloca.h>

#define restrict __restrict

static inline void swap(uint8_t *a, uint8_t *b) { uint8_t tmp = *a; *a = *b; *b = tmp;  }

/*****
 * Writes bytes to the tail buffer for the frame. Must always be called with count > 0
 * Should be called with inc_byte = 1 when writing out the final byte
 *
 */
static inline uint8_t *write_tail(uint16_t count, uint8_t *const tail, uint8_t inc_byte) {
  uint8_t *ptr = tail;
  
  while(1) {
    if(count > 0xFE) {
      count += inc_byte;
      *ptr++ = 0xFF;
      count = count - 0xFE;
    } else {
      *ptr++ = (uint8_t)count;
      break;
    }
  }
  //The first and last get swapped because the tail is read in reverse order and all the center bytes are 0xFF
  swap(tail, ptr - 1);

  return ptr;
}

#define BUF_SIZE 32
#define MIN(a, b) ((a) < (b) ? (a) : (b))

uint16_t CobsDecoder::decode_upto(Stream &stream, uint16_t len)  {
	if(this->bytes >= len || this->end_of_frame) return 0;

	do {
		if(buf_pos >= buf_len) { //Out of bytes in the input buffer
			uint16_t avail  = MIN((uint16_t)stream.available(), sizeof(this->in_buf));
	
			if(avail == 0)  return len - this->bytes;
			stream.readBytes(this->in_buf, avail);
			this->buf_len = avail;
			this->buf_pos = 0;
		}

		if(this->seeking_frame_end) {
			//Consume bytes until we find a 0
			while(this->buf_pos < this->buf_len) {
				if(this->in_buf[this->buf_pos++] == 0) {
					this->seeking_frame_end = 0;
					this->start_of_chunk = 1;
					break;
				}
			}
			continue;
		}


		if(this->start_of_chunk) { //Next byte is a COBS code
			this->code  = this->in_buf[this->buf_pos++];
			if(this->code == 0) this->end_of_frame = 1; //0 in the stream denotes the end of the frame

			this->count = 1;
			this->start_of_chunk = 0;
		}

		if(this->count == this->code) { //Time to write a 0 based on COBS code
			if(this->code != 0xFF) {
				this->dst[this->bytes++] = 0;
			}
			this->start_of_chunk = 1;
			continue;
		}

		uint8_t uncoded_copy_count = MIN(
      MIN(
        (uint16_t)(len - this->bytes), 
        (uint8_t)(this->code - 1)
      ), 
      (uint8_t)(this->buf_len - this->buf_pos)
    );
		uint8_t *dst = this->dst + this->bytes;
		uint8_t *src = this->in_buf + this->buf_pos;
		uint8_t tmp;
    this->count += uncoded_copy_count;
		while(uncoded_copy_count-- && (tmp = *src++) != 0) {
			if(tmp != 0) 
				*dst++ = tmp;
			else {
				this->end_of_frame = 1;
				break;
			}
		}
		this->bytes   = dst - this->dst;
		this->buf_pos = src - this->in_buf;
	} while(this->bytes < len);

	return len - this->bytes;
}



void TECobsEncoder::encode(const uint8_t *restrict src, uint16_t len) {
	const uint8_t *end = src + len;
	uint8_t *buf = (uint8_t*)alloca(BUF_SIZE); //A smallish amount of buffer
	uint8_t  buf_offset = 0, c;

	while(src < end) {
		c = *src++;
		if(c == 0) {
			if(this->run_count  < 0xFF) {
				buf[buf_offset++] = this->run_count;
			} else {
				buf[buf_offset++] = 0xFF;
				this->run_count -= 0xFE;
				//TODO: Undo this pointer math
				uint8_t *tail_ptr = write_tail(this->run_count, this->tail + this->tail_count, 0);
				this->tail_count = tail_ptr - this->tail;
			}
			this->run_count = 1;
		} else {
			buf[buf_offset++] = c;
			this->run_count++;
		}

		//Flush the buffer if it's full
		if(buf_offset == BUF_SIZE) {
			this->serial.write(buf, BUF_SIZE);
			buf_offset = 0;
		}
	}

	//Write out anything remaining in the buffer
	if(buf_offset > 0) {
		this->serial.write(buf, buf_offset);
	}

}

void TECobsEncoder::end_frame() {
  //Write out the tail of overflow bytes
  this->run_count += this->tail_count;

  //Write out initial (last) count, it may require additional tail bytes to be written
  uint8_t *tmp = write_tail(this->run_count, this->tail + this->tail_count, 1);
  *tmp++ = 0;

  this->serial.write(this->tail, tmp - this->tail);
  this->tail_count = 0;
  this->run_count  = 1;
}
