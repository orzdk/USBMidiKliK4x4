/*
__ __| |           |  /_) |     ___|             |           |
   |   __ \   _ \  ' /  | |  / |      _ \ __ \   |      _` | __ \   __|
   |   | | |  __/  . \  |   <  |   |  __/ |   |  |     (   | |   |\__ \
  _|  _| |_|\___| _|\_\_|_|\_\\____|\___|_|  _| _____|\__,_|_.__/ ____/
  -----------------------------------------------------------------------------
  USBMIDIKLIK 4X4 - USB Midi advanced firmware for STM32F1 platform.
  Copyright (C) 2019 by The KikGen labs.
  LICENCE CREATIVE COMMONS - Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)

  This file is part of the USBMIDIKLIK-4x4 distribution
  https://github.com/TheKikGen/USBMidiKliK4x4
  Copyright (c) 2019 TheKikGen Labs team.
  -----------------------------------------------------------------------------
  Disclaimer.

  This work is licensed under the Creative Commons Attribution-NonCommercial 4.0 International License.
  To view a copy of this license, visit http://creativecommons.org/licenses/by-nc/4.0/
  or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.

  NON COMMERCIAL - PERSONAL USE ONLY : You may not use the material for pure
  commercial closed code solution without the licensor permission.

  You are free to copy and redistribute the material in any medium or format,
  adapt, transform, and build upon the material.

  You must give appropriate credit, a link to the github site
  https://github.com/TheKikGen/USBMidiKliK4x4 , provide a link to the license,
  and indicate if changes were made. You may do so in any reasonable manner,
  but not in any way that suggests the licensor endorses you or your use.

  You may not apply legal terms or technological measures that legally restrict
  others from doing anything the license permits.

  You do not have to comply with the license for elements of the material
  in the public domain or where your use is permitted by an applicable exception
  or limitation.

  No warranties are given. The license may not give you all of the permissions
  necessary for your intended use.  This program is distributed in the hope that
  it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

*/

#include "build_number_defines.h"
#include <string.h>
#include <libmaple/nvic.h>
#include <EEPROM.h>
#include <Wire_slave.h>
#include <PulseOutManager.h>
#include <midiXparser.h>
#include <l3m_mk_t.h>
#include "usbmidiklik4x4.h"
#include "usb_midi.h"
#include "ringbuffer.h"
#include <l3m_mk.h>

///////////////////////////////////////////////////////////////////////////////
// GLOBALS
///////////////////////////////////////////////////////////////////////////////

// EEPROMS parameters
EEPROM_Params_t EEPROM_Params;
// Timer
HardwareTimer timer(2);
// Serial interfaces Array
HardwareSerial * serialHw[SERIAL_INTERFACE_MAX] = {SERIALS_PLIST};
// Prepare LEDs pulse for Connect, MIDIN and MIDIOUT
// From MIDI SERIAL point of view
// Use a PulseOutManager factory to create the pulses
PulseOutManager flashLEDManager;
PulseOut* flashLED_CONNECT = flashLEDManager.factory(LED_CONNECT,LED_PULSE_MILLIS,LOW);

#ifdef HAS_MIDITECH_HARDWARE
  // LED must be declared in the same order as hardware serials
  #define LEDS_MIDI
  PulseOut* flashLED_IN[SERIAL_INTERFACE_MAX] =
	{
    flashLEDManager.factory(D4,LED_PULSE_MILLIS,LOW),
    flashLEDManager.factory(D5,LED_PULSE_MILLIS,LOW),
    flashLEDManager.factory(D6,LED_PULSE_MILLIS,LOW),
    flashLEDManager.factory(D7,LED_PULSE_MILLIS,LOW)
  };
  PulseOut* flashLED_OUT[SERIAL_INTERFACE_MAX] =
	{
    flashLEDManager.factory(D36,LED_PULSE_MILLIS,LOW),
    flashLEDManager.factory(D37,LED_PULSE_MILLIS,LOW),
    flashLEDManager.factory(D16,LED_PULSE_MILLIS,LOW),
    flashLEDManager.factory(D17,LED_PULSE_MILLIS,LOW)
  };
#endif

// Timer used to signal I2C events every 300 ms
PulseOut I2C_LedTimer(0xFF,500);

// USB Midi object & globals
USBMidi MidiUSB;
volatile bool					midiUSBCx      = false;
volatile bool         midiUSBIdle    = false;
bool                  midiUSBLaunched= false;
bool 					        isSerialBusy   = false ;
unsigned long         midiUSBLastPacketMillis    = 0;
// MIDI Parsers for serial 1 to n
midiXparser midiSerial[SERIAL_INTERFACE_MAX];

uint8_t sysExInternalHeader[] = {SYSEX_INTERNAL_HEADER} ;
uint8_t sysExInternalIdentityRqReply[] = {SYSEX_INTERNAL_IDENTITY_RQ_REPLY};
uint8_t sysExInternalBuffer[SYSEX_INTERNAL_BUFF_SIZE] ;


// Intelligent midi thru mode
volatile bool intelliThruActive = false;
unsigned long intelliThruDelayMillis = DEFAULT_INTELLIGENT_MIDI_THRU_DELAY_PERIOD * 15000;

// Bus Mode globals
uint8_t I2C_DeviceIdActive[B_MAX_NB_DEVICE-1]; // Minus the master
uint8_t I2C_DeviceActiveCount=0;

boolean I2C_MasterReady = false;
volatile unsigned long I2C_MasterReadyTimeoutMillis = 0;
volatile uint8_t I2C_Command = B_CMD_NONE;

// Master to slave synchonization globals
volatile boolean I2C_SlaveSyncStarted = false;
volatile boolean I2C_SlaveSyncDoUpdate = false;

// Templated RingBuffers to manage I2C slave reception/transmission outside I2C ISR
// Volatile by default and RESERVED TO SLAVE
RingBuffer<uint8_t,B_RING_BUFFER_PACKET_SIZE> I2C_QPacketsFromMaster;
RingBuffer<uint8_t,B_RING_BUFFER_MPACKET_SIZE> I2C_QPacketsToMaster;

RingBuffer<uint8_t,sizeof(midiPacket_t)> LoopbackPacketsQ;

///////////////////////////////////////////////////////////////////////////////
//  CODE MODULES
//-----------------------------------------------------------------------------
// Due to the unusual make process of Arduino platform, modules are included
// directly here as "h" type. This allows a better code separation and readability.
///////////////////////////////////////////////////////////////////////////////
// DO NOT REMOVE OR CHANGE THE ORDER !

#include "mod_macros.h"
#include "mod_eeprom.h"
#include "mod_configui.h"
#include "mod_i2cbus.h"
#include "mod_sysexcfg.h"

///////////////////////////////////////////////////////////////////////////////
//  CORE FUNCTIONS
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Timer2 interrupt handler
///////////////////////////////////////////////////////////////////////////////
void Timer2Handler(void)
{
     // Update LEDS & timer
     flashLEDManager.update(millis());
     I2C_LedTimer.update(millis());
     #ifdef DEBUG_MODE
     I2C_DebugTimer.update(millis());
     #endif
}

///////////////////////////////////////////////////////////////////////////////
// FlashAllLeds . 0 = Alls. 1 = In. 2 = Out
///////////////////////////////////////////////////////////////////////////////
void FlashAllLeds(uint8_t mode)
{
  for ( uint8_t f=0 ; f!= 4 ; f++ ) {
		#ifdef LEDS_MIDI
			for ( uint8_t i=0 ; i< SERIAL_INTERFACE_MAX ; i++ ) {
					if ( mode == 0 || mode ==1 ) flashLED_IN[i]->start();
					if ( mode == 0 || mode ==2 ) 	flashLED_OUT[i]->start();
			}
		#else
			flashLED_CONNECT->start();
		#endif
		delay(100);
	}
}

///////////////////////////////////////////////////////////////////////////////
// Send a midi msg to serial MIDI. 0 is Serial1.
///////////////////////////////////////////////////////////////////////////////
void SerialMidi_SendMsg(uint8_t *msg, uint8_t serialNo)
{
  if (serialNo >= SERIAL_INTERFACE_MAX ) return;

	uint8_t msgLen = midiXparser::getMidiStatusMsgLen(msg[0]);

	if ( msgLen > 0 ) {
	  serialHw[serialNo]->write(msg,msgLen);
		FLASH_LED_OUT(serialNo);
	}
}

///////////////////////////////////////////////////////////////////////////////
// Send a USB midi packet to ad-hoc serial MIDI
///////////////////////////////////////////////////////////////////////////////
void SerialMidi_SendPacket(midiPacket_t *pk, uint8_t serialNo)
{
  if (serialNo >= SERIAL_INTERFACE_MAX ) return;

	uint8_t msgLen = USBMidi::CINToLenTable[pk->packet[0] & 0x0F] ;

 	if ( msgLen > 0 ) {
		serialHw[serialNo]->write(&pk->packet[1],msgLen);
		FLASH_LED_OUT(serialNo);
	}
}

///////////////////////////////////////////////////////////////////////////////
// Prepare a packet and route it to the right USB midi cable
///////////////////////////////////////////////////////////////////////////////
 void SerialMidi_RouteMsg( uint8_t cable, midiXparser* xpMidi )
{

    midiPacket_t pk = { .i = 0 };
    uint8_t msgLen = xpMidi->getMidiMsgLen();
    uint8_t msgType = xpMidi->getMidiMsgType();

    pk.packet[0] = cable << 4;
    memcpy(&pk.packet[1],&(xpMidi->getMidiMsg()[0]),msgLen);

    // Real time single byte message CIN F->
    if ( msgType == midiXparser::realTimeMsgTypeMsk ) pk.packet[0]   += 0xF;
    else

    // Channel voice message => CIN A-E
    if ( msgType == midiXparser::channelVoiceMsgTypeMsk )
        pk.packet[0]  += ( (xpMidi->getMidiMsg()[0]) >> 4);
    else

    // System common message CIN 2-3
    if ( msgType == midiXparser::systemCommonMsgTypeMsk ) {

        // 5 -  single-byte system common message (Tune request is the only case)
        if ( msgLen == 1 ) pk.packet[0] += 5;

        // 2/3 - two/three bytes system common message
        else pk.packet[0] += msgLen;
    }

    else return; // We should never be here !

    RoutePacketToTarget( FROM_SERIAL,&pk);
}

///////////////////////////////////////////////////////////////////////////////
// Parse sysex flows and make a packet for USB
// ----------------------------------------------------------------------------
// We use the midiXparser 'on the fly' mode, allowing to tag bytes as "captured"
// when they belong to a midi SYSEX message, without storing them in a buffer.
///////////////////////////////////////////////////////////////////////////////
 void SerialMidi_RouteSysEx( uint8_t cable, midiXparser* xpMidi )
{
  static midiPacket_t pk[SERIAL_INTERFACE_MAX];
  static uint8_t packetLen[SERIAL_INTERFACE_MAX];
  static bool firstCall = true;

  uint8_t readByte = xpMidi->getByte();

  // Initialize everything at the first call
  if (firstCall ) {
    firstCall = false;
    memset(pk,0,sizeof(midiPacket_t)*SERIAL_INTERFACE_MAX);
    memset(packetLen,0,sizeof(uint8_t)*SERIAL_INTERFACE_MAX);
  }

  // Normal End of SysEx or : End of SysEx with error.
  // Force clean end of SYSEX as the midi usb driver
  // will not understand if we send the packet as is
  if ( xpMidi->wasSysExMode() ) {
      // Force the eox byte in case we have a SYSEX error.
      packetLen[cable]++;
      pk[cable].packet[ packetLen[cable] ] = midiXparser::eoxStatus;
      // CIN = 5/6/7  sysex ends with one/two/three bytes,
      pk[cable].packet[0] = (cable << 4) + (packetLen[cable] + 4) ;
      RoutePacketToTarget( FROM_SERIAL,&pk[cable]);
      packetLen[cable] = 0;
      pk[cable].i = 0;
			return;
  } else

  // Fill USB sysex packet
  if ( xpMidi->isSysExMode() ) {
	  packetLen[cable]++;
	  pk[cable].packet[ packetLen[cable] ] = readByte ;

	  // Packet complete ?
	  if (packetLen[cable] == 3 ) {
	      pk[cable].packet[0] = (cable << 4) + 4 ; // Sysex start or continue
	      RoutePacketToTarget( FROM_SERIAL,&pk[cable]);
	      packetLen[cable] = 0;
	      pk[cable].i = 0;
	  }
	}
}

///////////////////////////////////////////////////////////////////////////////
// THE MIDI PACKET ROUTER
//-----------------------------------------------------------------------------
// Route a packet from a midi IN jack / USB OUT to
// a midi OUT jacks / USB IN  or I2C remote serial midi on another device
///////////////////////////////////////////////////////////////////////////////
 void RoutePacketToTarget(uint8_t source,  midiPacket_t *pk)
{
  // NB : we use the same routine to route USB and serial/ I2C .
	// The Cable can be the serial port # if coming from local serial
  uint8_t sourcePort  = pk->packet[0] >> 4;

	// Check at the physical level (i.e. not the bus)
  if ( source == FROM_USB && sourcePort >= USBCABLE_INTERFACE_MAX ) return;
	if ( source == FROM_SERIAL ) {
    
    if ( sourcePort >= SERIAL_INTERFACE_MAX ) return;

    // If bus mode active, the local port# must be translated according
		// to the device Id, before routing
    if (EEPROM_Params.I2C_BusModeState == B_ENABLED ) {
			sourcePort = GET_BUS_SERIALNO_FROM_LOCALDEV(EEPROM_Params.I2C_DeviceId,sourcePort);
    }
  }
  uint8_t cin   = pk->packet[0] & 0x0F ;

	FLASH_LED_IN(sourcePort);

	// Sysex is a particular case when using packets.
	// Internal sysex Jack 1/Cable = 0 ALWAYS!! are checked whatever filters are
	// This insures that the internal sysex will be always interpreted.
	// If the MCU is resetted, the msg will not be sent
	uint8_t  msgType=0;

	if (cin >= 4 && cin <= 7  ) {
		if (sourcePort == 0) SysExInternalParse(source, pk);
		msgType =  midiXparser::sysExMsgTypeMsk;
	} else {
			msgType =  midiXparser::getMidiStatusMsgTypeMsk(pk->packet[1]);
	}

	// ROUTING tables
	uint16_t *cableInTargets ;
	uint16_t *serialOutTargets ;
	uint8_t *inFilters ;

  // Save intelliThruActive and USBCx state as it could be changed in an interrupt
  // (when slave)
  //boolean ithru = intelliThruActive;

  if (source == FROM_SERIAL ){
    // IntelliThru active ? If so, take the good routing rules
   //  if ( ithru ) {
			// if ( ! EEPROM_Params.intelliThruJackInMsk ) return; // Double check.
   //    serialOutTargets = &EEPROM_Params.midiRoutingRulesIntelliThru[sourcePort].jackOutTargetsMsk;
   //    inFilters = &EEPROM_Params.midiRoutingRulesIntelliThru[sourcePort].filterMsk;
   //  }
   //  else {
      cableInTargets = &EEPROM_Params.midiRoutingRulesSerial[sourcePort].cableInTargetsMsk;
      serialOutTargets = &EEPROM_Params.midiRoutingRulesSerial[sourcePort].jackOutTargetsMsk;
      inFilters = &EEPROM_Params.midiRoutingRulesSerial[sourcePort].filterMsk;

      if ((sts_fns(0).gateFn)(pk)) (sts_fns(0).modFn)(pk, sts_parms(0));
      if ((sts_fns(1).gateFn)(pk)) (sts_fns(1).modFn)(pk, sts_parms(1));

    //}   
  }
  else if (source == FROM_USB ) {
      cableInTargets = &EEPROM_Params.midiRoutingRulesCable[sourcePort].cableInTargetsMsk;
      serialOutTargets = &EEPROM_Params.midiRoutingRulesCable[sourcePort].jackOutTargetsMsk;
      inFilters = &EEPROM_Params.midiRoutingRulesCable[sourcePort].filterMsk;
 
      if ((cts_fns(0).gateFn)(pk)) (cts_fns(0).modFn)(pk, cts_parms(0));
      if ((cts_fns(1).gateFn)(pk)) (cts_fns(1).modFn)(pk, cts_parms(1));
  }

  else return; // Error.

	// Apply midi filters
	if (! (msgType & *inFilters) ) return;

  // ROUTING FROM ANY SOURCE PORT TO SERIAL TARGETS //////////////////////////
	// A target match ?
  if (*serialOutTargets) {
			for (	uint16_t t=0; t != 15; t++) {
					if ( (*serialOutTargets & ( 1 << t ) ) ) {
<<<<<<< HEAD
                // Route Loopbacks -------------------------------------------------------------------
                //
                // Copy packet, change destination cable (sourceport), add to ringbuffer, 
                // cease physical routing to the specific target, and continue to next 
                //
                //----------------------------------------------------------------------------------
                //
                // First time packet is here is because a physical Jack/Cable connection has specified 
                // a virtual/echo jack (by definition all non-physically available jacks < 16) as a routing target. 
                // In that case the source is changed from the actual source to the virtual jack, and packet put in ringbuffer.
                //
                // In SerialMidi_Process() the packet is then retrieved from the buffer and once again routed with RoutePacketToTarget() (this function),
                // but this second time around, the packet looks like it comes from the virtual jack, and will use the routing table
                // for the virtual jack, which tells the packet to route to another physical cable or jack, locally or on bus 
                //
                // The packet has already been sampled and the cable and serial targets has been identified from the sourceport,
                // so the copy may not be nescessary. Serial MIDI does not use first byte, and USB (cable) targets are 
                // set below in packet copies case there are any
                //------------------------------------------------------------------------------------
                if (t >= SERIAL_INTERFACE_COUNT){
                    midiPacket_t pk2 = { .i = pk->i }; 
                    pk2.packet[0] = (t << 4) + cin;                    
                    LoopbackPacketsQ.write(pk2.packet,sizeof(midiPacket_t));
=======

                //Route virtual
                if (t >= SERIAL_INTERFACE_COUNT){

                    midiPacket_t pk2 = { .i = pk->i };
                    pk2.packet[0] = ( t << 4 ) + cin;

                    LoopbackPacketsQ.write(pk2,sizeof(pk2));
>>>>>>> parent of 8669ec1... Loopback works
                }
                else
								// Route via the bus
								if (EEPROM_Params.I2C_BusModeState == B_ENABLED ) {
                     I2C_BusSerialSendMidiPacket(pk, t);
								}
								// Route to local serial if bus mode disabled
								else SerialMidi_SendPacket(pk,t);               
					}
<<<<<<< HEAD
			}
	} 
=======
	} /

  // Stop here if IntelliThru active (no USB active but maybe connected)
  // Intellithru is always activated by the master in bus mode!.

  //if ( ithru ) return;
>>>>>>> parent of 8669ec1... Loopback works

  // Stop here if no USB connection (owned by the master).
  // If we are a slave, the master should have notified us
  if ( ! midiUSBCx ) return;

	// Apply cable routing rules from serial or USB
	// Only if USB connected and thru mode inactive

  if (  *cableInTargets  ) {
    	midiPacket_t pk2 = { .i = pk->i }; // packet copy to change the dest cable
			for (uint8_t t=0; t != USBCABLE_INTERFACE_MAX ; t++) {
	      if ( *cableInTargets & ( 1 << t ) ) {
	          pk2.packet[0] = ( t << 4 ) + cin;
            // Only the master has USB midi privilege in bus MODE
            // Everybody else if an usb connection is active
            if (! B_IS_SLAVE ) {
                MidiUSB.writePacket(&pk2.i);
            } else
            // A slave in bus mode ?
            // We need to add a master packet to the Master's queue.
            {
                masterMidiPacket_t mpk;
                mpk.mpk.dest = TO_USB;
                // Copy the midi packet to the master packet
                mpk.mpk.pk.i = pk2.i;
                I2C_QPacketsToMaster.write(mpk.packet,sizeof(masterMidiPacket_t));
            }

	          #ifdef LEDS_MIDI
	          flashLED_IN[t]->start();
	          #endif
	    	}
			}
	}

}

///////////////////////////////////////////////////////////////////////////////
// Send a SYSEX midi message to USB Cable 0
///////////////////////////////////////////////////////////////////////////////
void SysExSendMsgPacket( uint8_t buff[],uint16_t sz) 
{ 

  midiPacket_t pk { .i = 0};
  uint8_t b=0;
  bool endPk;
  // Build sysex packets
  for ( uint16_t i = 0; i != sz ; i ++ ) {
    pk.packet[++b] = buff[i];
    endPk = ( i+2 > sz );
    if (b == 3 ||  endPk ) {
        pk.packet[0]  = endPk ?  b + 4 : 4 ;
        MidiUSB.writePacket(&pk.i);
        FLASH_LED_OUT(0);
        b=0; pk.i = 0;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// Check what is the current boot mode.
// Will never come back if config mode.
///////////////////////////////////////////////////////////////////////////////
void CheckBootMode()
{
	// Does the config menu boot mode is active ?
	// if so, prepare the next boot in MIDI mode and jump to menu
	if  ( EEPROM_Params.nextBootMode == bootModeConfigMenu ) {

      // Next boot on Midi
      EEPROM_Params.nextBootMode = bootModeMidi;
      EEPROM_ParamsSave();

			#ifdef HAS_MIDITECH_HARDWARE
				// Assert DISC PIN (PA8 usually for Miditech) to enable USB
				gpio_set_mode(PIN_MAP[PA8].gpio_device, PIN_MAP[PA8].gpio_bit, GPIO_OUTPUT_PP);
				gpio_write_bit(PIN_MAP[PA8].gpio_device, PIN_MAP[PA8].gpio_bit, 1);
			#endif

			// start USB serial
			Serial.begin(115200);
			delay(500);

			// Start Wire as a master for config & tests purpose in the menu
			Wire.begin();
			Wire.setClock(B_FREQ) ;

			// wait for a serial monitor to be connected.
			// 3 short flash

			while (!Serial) {
					flashLED_CONNECT->start();delay(100);
					flashLED_CONNECT->start();delay(100);
					flashLED_CONNECT->start();delay(300);
				}
			digitalWrite(LED_CONNECT, LOW);
			ShowConfigMenu(); // INFINITE LOOP
	}
}

///////////////////////////////////////////////////////////////////////////////
// MIDI USB initiate connection if master
// + Set USB descriptor strings
///////////////////////////////////////////////////////////////////////////////
void USBMidi_Init()
{
	usb_midi_set_vid_pid(EEPROM_Params.vendorID,EEPROM_Params.productID);
	usb_midi_set_product_string((char *) &EEPROM_Params.productString);

	MidiUSB.begin() ;
  delay(4000); // Note : Usually around 4 s to fully detect USB Midi on the host
}

///////////////////////////////////////////////////////////////////////////////
// MIDI USB Loop Process
///////////////////////////////////////////////////////////////////////////////
void USBMidi_Process()
{
	// Try to connect/reconnect USB if we detect a high level on USBDM
	// This is to manage the case of a powered device without USB active or suspend mode for ex.
	if ( MidiUSB.isConnected() ) {

    if (! midiUSBCx) digitalWrite(LED_CONNECT, LOW);
    midiUSBCx = true;

		// Do we have a MIDI USB packet available ?
		if ( MidiUSB.available() ) {
			midiUSBLastPacketMillis = millis()  ;
			midiUSBIdle = false;
			intelliThruActive = false;

			// Read a Midi USB packet .
			if ( !isSerialBusy ) {
				midiPacket_t pk ;
				pk.i = MidiUSB.readPacket();
				RoutePacketToTarget( FROM_USB,  &pk );
			} else {
					isSerialBusy = false ;
			}
		} else
		if (!midiUSBIdle && millis() > ( midiUSBLastPacketMillis + intelliThruDelayMillis ) )
				midiUSBIdle = true;
	}
	// Are we physically connected to USB
	else {
       if (midiUSBCx) digitalWrite(LED_CONNECT, HIGH);
       midiUSBCx = false;
		   midiUSBIdle = true;
  }

  /*
	if ( midiUSBIdle && !intelliThruActive && EEPROM_Params.intelliThruJackInMsk) {
			intelliThruActive = true;
			FlashAllLeds(0); // All leds when Midi intellithru mode active
	}
  */

}

///////////////////////////////////////////////////////////////////////////////
// I2C Loop Process for SERIAL MIDI
///////////////////////////////////////////////////////////////////////////////
void SerialMidi_Process()
{
	// LOCAL SERIAL JACK MIDI IN PROCESS
<<<<<<< HEAD

//  Process loopback packages
  midiPacket_t pk;
  while (LoopbackPacketsQ.available()) {                     
    LoopbackPacketsQ.readBytes(pk.packet,sizeof(midiPacket_t));
    RoutePacketToTarget(FROM_SERIAL,&pk);
  }
  
	for ( uint8_t s = 0; s < SERIAL_INTERFACE_MAX; s++ )
=======
	for ( uint8_t s = 0; s< SERIAL_INTERFACE_MAX  ; s++ )
>>>>>>> parent of 8669ec1... Loopback works
	{
        //Do we have any loopback packages available
        midiPacket_t pk;
        if (LoopbackPacketsQ.available()) {
          LoopbackPacketsQ.readBytes(pk,sizeof(midiPacket_t));
          RoutePacketToTarget(FROM_LOOPBACK,&pk);
        }

				// Do we have any MIDI msg on Serial 1 to n ?
				if ( serialHw[s]->available() ) {
					 if ( midiSerial[s].parse( serialHw[s]->read() ) ) {
								// We manage sysEx "on the fly". Clean end of a sysexe msg ?
								if ( midiSerial[s].getMidiMsgType() == midiXparser::sysExMsgTypeMsk )
									SerialMidi_RouteSysEx(s, &midiSerial[s]) ;

								// Not a sysex. The message is complete.
								else {
                  SerialMidi_RouteMsg( s, &midiSerial[s]);
                }

					 }
					 else
					 // Acknowledge any sysex error
					 if ( midiSerial[s].isSysExError() )
						 SerialMidi_RouteSysEx(s, &midiSerial[s]) ;
					 else
					 // Check if a SYSEX mode active and send bytes on the fly.
					 if ( midiSerial[s].isSysExMode() && midiSerial[s].isByteCaptured() ) {
							SerialMidi_RouteSysEx(s, &midiSerial[s]) ;
					 }
				}

				// Manage Serial contention vs USB
				// When one or more of the serial buffer is full, we block USB read one round.
				// This implies to use non blocking Serial.write(buff,len).
				if (  midiUSBCx &&  !serialHw[s]->availableForWrite() ) isSerialBusy = true; // 1 round without reading USB
	}
}

///////////////////////////////////////////////////////////////////////////////
// SETUP
///////////////////////////////////////////////////////////////////////////////
void setup()
{

		// EEPROM initialization
		// Set EEPROM parameters for the STMF103RC
	  EEPROM.PageBase0 = 0x801F000;
	  EEPROM.PageBase1 = 0x801F800;
	  EEPROM.PageSize  = 0x800;
		//EEPROM.Pages     = 1;


		EEPROM.init();

		// Retrieve EEPROM parameters
    EEPROM_ParamsInit();

    #ifndef DEBUG_MODE
        EEPROM_Params.debugMode = false;
    #endif

    // Configure the TIMER2
    timer.pause();
    timer.setPeriod(TIMER2_RATE_MICROS); // in microseconds
    // Set up an interrupt on channel 1
    timer.setChannel1Mode(TIMER_OUTPUT_COMPARE);
    timer.setCompare(TIMER_CH1, 1);  // Interrupt 1 count after each update
    timer.attachCompare1Interrupt(Timer2Handler);
    timer.refresh();   // Refresh the timer's count, prescale, and overflow
    timer.resume();    // Start the timer counting

    // Start the LED Manager
    flashLEDManager.begin();

		CheckBootMode();

    // MIDI MODE START HERE ==================================================

    //intelliThruDelayMillis = EEPROM_Params.intelliThruDelayPeriod * 15000;

    // MIDI SERIAL PORTS set Baud rates and parser inits
    // To compile with the 4 serial ports, you must use the right variant : STMF103RC
    // + Set parsers filters in the same loop.  All messages including on the fly SYSEX.

    for ( uint8_t s=0; s != SERIAL_INTERFACE_MAX ; s++ ) {
      serialHw[s]->begin(31250);
      midiSerial[s].setMidiMsgFilter( midiXparser::allMsgTypeMsk );
    }

		// I2C bus checks that could disable the bus mode
  	I2C_BusChecks();

    // Midi USB only if master when bus is enabled or master/slave
    if ( ! B_IS_SLAVE  ) {
        midiUSBLaunched = true;
        USBMidi_Init();
        #ifdef DEBUG_MODE
        if (EEPROM_Params.debugMode ) {
            DEBUG_SERIAL.end();
            DEBUG_SERIAL.begin(115200);
            DEBUG_SERIAL.flush();
            delay(500);
        }
        #endif
  	}

		I2C_BusStartWire();		// Start Wire if bus mode enabled. AFTER MIDI !

}

///////////////////////////////////////////////////////////////////////////////
// LOOP
///////////////////////////////////////////////////////////////////////////////
void loop()
{
		if ( midiUSBLaunched ) USBMidi_Process();

		SerialMidi_Process();

		// I2C BUS MIDI PACKET PROCESS
		if ( B_IS_SLAVE ) I2C_ProcessSlave();
		else if ( B_IS_MASTER ) I2C_ProcessMaster();
}
