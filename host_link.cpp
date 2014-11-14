#include "host_link.h"
#include <alloca.h>

#define MIN(a,b) ((a) > (b) ? (b) : (a))

void HostLink::tick() {
  if(this->up) {
    //Take down the link if USB goes away
    if(!this->serial->dtr()) {
      this->up = 0;
      this->recv_state = IDLE;
      return;
    }

    if(this->clock_incr) {
      this->local_clock += 1;
      this->clock_incr   = 0;
    }

    //TX logic
    if(this->peek_req) {
      this->send_poke((uint8_t*)this->peek_req_data.src, this->peek_req_data.len);
      this->peek_req = 0;
    }
    
    //Receive logic 
    uint8_t remaining;
    recv_switch: 
    switch(this->recv_state) {
    case IDLE:
      this->decoder.set_dst(&this->recv_hdr);
      this->recv_state = READING_HEADER; //Deliberately fall through to reading the header
    case READING_HEADER:
      remaining = this->decoder.decode_upto( *this->serial, sizeof(link_packet_header) );
      if(remaining != 0) break;

      //Figure out what to do with the packet
      switch(this->recv_hdr.type) {
      case LINK_POKE:
        this->recv_state = HANDLING_POKE;
        this->decoder.set_dst(&this->recv_data.poke);
        break;
      case LINK_PEEK:
        this->recv_state = HANDLING_PEEK;
        this->decoder.set_dst(&this->recv_data.peek);
        break;
      case LINK_NOP:
      default:
        this->recv_state = IDLE;
        this->decoder.next_frame();
      }

      if(this->recv_state != IDLE) {
        this->remote_clock = this->recv_hdr.local_clock;
        this->local_clock++;
        goto recv_switch; //continue processing the packet
      }
      break;
    case HANDLING_POKE:
      remaining = this->decoder.decode_upto( *this->serial, sizeof(link_poke_data));
      if(remaining != 0) break;

      this->clock_incr = 1; //Clock is incremented on incoming messages that mutate state
      this->decoder.set_dst((uint8_t*)this->recv_data.poke.dst); //Write directly into uC memory
      this->recv_state = WRITING_POKE;
      //Deliberately fall through
    case WRITING_POKE:
      remaining = this->decoder.decode_upto( *this->serial, 0xFF); //Readers out the rest of the packet into memory
      if(remaining != 0) break;

      this->decoder.next_frame();
      this->recv_state = IDLE;
      break;
    case HANDLING_PEEK:
      if(this->peek_req != 0) break; //There's no way we can handle the peek if a req is already in the hopper
      remaining = this->decoder.decode_upto( *this->serial, sizeof(link_peek_data));
      if(remaining != 0) break;

      this->clock_incr = 1; //Any time a read is 
      this->peek_req_data = this->recv_data.peek;
      this->peek_req      = 1;
      this->decoder.next_frame();
      this->recv_state    = IDLE;
      break;
    }
  } else {
    if(this->serial->dtr()) {
      this->up = 1;
      this->local_clock  = 0;
      this->remote_clock = 0;
      this->recv_state = IDLE;
      this->decoder = CobsDecoder();

      //Greet the host
      link_helo_packet pkt = {
        {
          LINK_HELO,
          this->local_clock,
          this->remote_clock
        },
        {
          1, //Protocol version
          this->device_type,
          (uint16_t)this->root
        }
      };
      TECobsEncoder encoder(*this->serial);
      encoder.encode((uint8_t*)&pkt, sizeof(pkt));
      encoder.end_frame();
    }
  }
}

void HostLink::send_poke(void *src, uint8_t len) {
  if(!this->up) return;

  link_poke_packet pkt = {
    {
      LINK_POKE,
      this->local_clock,
      this->remote_clock
    },
    {
      (uint16_t)src,
      len
    }
  };
  TECobsEncoder encoder(*this->serial);
  encoder.encode((uint8_t*)&pkt, sizeof(pkt));
  encoder.encode((uint8_t*)src, len);
  encoder.end_frame();

  this->clock_incr = 1; //So that next time host_link() is called, the local_clock advances
}

