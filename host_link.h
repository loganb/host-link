#ifndef __HOST_LINK_H_
#define __HOST_LINK_H_

#include <arduino.h>

#ifdef __cplusplus
#define restrict __restrict__
#endif

#include "cobs.h"

//Op-codes for different commands
#define LINK_NOP      0 //No-op, can be sent or received
#define LINK_HELO     1 //Sent by the client when the dtr line is strobed to tell the host what kind of device this is
#define LINK_PEEK     2 //Sent by host to read memory on device
#define LINK_POKE     3 //Sent by host to write memory on device

//
// An array of 128 of these in PROGMEM should be passed to host_link_tick
// The 0th handler in the array is for opcode 0, and so on
// Opcodes 128 - 255 are for built-in use
//
//typedef uint8_t(*link_command_f)(struct _host_link_state *restrict link, uint8_t opcode, uint8_t tid, uint8_t *data, uint8_t len);


//struct that can holds the fixed part of any packet
typedef struct {
  uint8_t  type;
  uint32_t local_clock;  //Clock at the source of the packet
  uint32_t remote_clock; //Last known clock at the far end
} link_packet_header; //Just the header fields

typedef struct link_helo_data {
  uint8_t  proto_version; //Protocol version
  uint32_t device_type; //32-bit code identifying the type of device
  uint16_t root; //Address of a root struct. The struct definition is implied by the device_type value
} link_helo_data;

typedef struct {
  uint16_t dst; //Destination address for the bytes
  uint16_t len; //Length of bytes
  //Remaining bytes in packet are bytes to write
} link_poke_data;

typedef struct {
  uint16_t src; //address to read from
  uint16_t len; //number of bytes requested  
} link_peek_data;

typedef struct {
  link_packet_header hdr;
  link_helo_data data;  
} link_helo_packet;

typedef struct {
  link_packet_header hdr;
  link_poke_data data;
} link_poke_packet;

typedef struct {
  link_packet_header hdr;

  union {
    link_helo_data helo;
    link_poke_data poke;
    link_peek_data peek;
  } data;
} link_packet;


/*******
 * Holds all the state associated with the link to the host
 */
class HostLink {
  uint8_t up         : 1; //1 if there is a link to the host
  uint8_t peek_req   : 1; //True if host has sent a peek that hasn't been fulfilled
  uint8_t clock_incr : 1; //Used to increment the local_clock only a little

  uint32_t local_clock;
  uint32_t remote_clock;

  //Recv structure
  enum {
    IDLE,
    READING_HEADER,
    HANDLING_POKE, //Reading address data out of the packet
    WRITING_POKE,  //Writing the data from the packet into memory
    HANDLING_PEEK
  } recv_state;

  link_packet_header recv_hdr;

  union {
    link_poke_data poke;
    link_peek_data peek;
  } recv_data;

  CobsDecoder   decoder;

  //Tx structure
  link_peek_data peek_req_data; //This data is valid if peek_req is true

  usb_serial_class *serial;
  uint32_t device_type;
  void *root;
  
public:
  HostLink(usb_serial_class *serial, uint32_t device_type, void *root) : serial(serial), device_type(device_type), root(root) { }

  void tick();

  // Sends a block of memory to the host
  void send_poke(void *src, uint8_t len);

protected:
  void receive_helper();
};



#endif