Host Link
=========

Host Link is a library for communicating with an Arduino over a USB serial
interface. The library exposes the microcontroller's RAM to the host PC. The
PC issues "peek" and "poke" commands to read and write the Arduino's memory,
respectively. Additionally, the arudino can send unsolicited pokes to the host
PC whenever relevant state is updated. A host library written in Ruby is
available (URL TBD).

Example Code
============

Arduino 
-------

		//The root struct should contain pointers to all the in-memory data structures
		//that the host PC will read or write
		struct RootStruct {
		  uint8_t  *led_port;
		  uint32_t *timer;
		};

		uint32_t idle_packet_timer = 0;

		//PC will be able to directly read/write these variables
		RootStruct root = {
		  (uint8_t*)&PORTB,  
		  &idle_packet_timer
		};

		//The host link is initialized with a device ID that the host PC uses to identify 
		//the kind of device it's talking to. The host link library advertises the address 
		//of the root struct so that host PC knows where to read/write data
		HostLink host_link(&Serial, 0xDEADCAFE, &root);

		void setup() {
			Serial.begin(115200); //Speed is irrelevant on a Teensy

			//Pins in PORTB
		  pinMode(13, OUTPUT); //Red channel of an RGB LED
		  pinMode(14, OUTPUT); //Green channel of an RGB LED
		  pinMode(15, OUTPUT); //Blue channel of an RGB LED

		}

		void loop() {
		  if(millis() - idle_packet_timer > 1000) {
		  	//Send the contents of the timer variable
		  	host_link.send_poke(&idle_packet_timer, sizeof(idle_packet_timer));

		  	idle_packet_timer = millis();
		  }

			host_link.tick(); //Do link housekeeping
		}

Host PC
-------

TBD

