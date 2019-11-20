
#ifndef _MOD_SYSEXCFG_H_
#define _MOD_SYSEXCFG_H_
#pragma once

uint8_t SysexInternalDumpConf(uint32_t fnId, uint8_t port,uint8_t *buff) 
{

  uint8_t src;
  uint8_t dest = 0 ;
  uint16_t msk ;
  uint8_t i;
  uint8_t c;
  uint8_t *buff2 = buff;

  memcpy(buff2,sysExInternalHeader,sizeof(sysExInternalHeader));
  buff2+=sizeof(sysExInternalHeader);
  *buff2 = fnId >> 24;

  switch (fnId) {

    // Function 0B - Change USB Product String
    case 0x0B000000:
          strcpy((char*)++buff2,(char*)EEPROM_Params.productString);
          buff2+=strlen((char*)EEPROM_Params.productString)-1;
          break;

    // Function 0C - Change USB Vendor ID and Product ID
    case 0x0C000000:
          *(++buff2) = EEPROM_Params.vendorID >> 12;
          *(++buff2) = (EEPROM_Params.vendorID & 0x0F00) >> 8;
          *(++buff2) = (EEPROM_Params.vendorID & 0x00F0) >> 4;
          *(++buff2) = (EEPROM_Params.vendorID & 0x000F) ;
          *(++buff2) = EEPROM_Params.productID >> 12;
          *(++buff2) = (EEPROM_Params.productID & 0x0F00) >> 8;
          *(++buff2) = (EEPROM_Params.productID & 0x00F0) >> 4;
          *(++buff2) = (EEPROM_Params.productID & 0x000F) ;
          break;

    // Function 0E - Intellithru midi routing rules
    // 02 Timeout
    case 0x0E020000:
          *(++buff2) = 0X02;
          *(++buff2) = EEPROM_Params.intelliThruDelayPeriod;
          break;

     // Function 0E - Intellithru midi routing rules
     // 03 Routing rules
     case 0x0E030000:
          *(++buff2) = 0X03;
          *(++buff2) = port;
          *(++buff2) = EEPROM_Params.midiRoutingRulesIntelliThru[port].filterMsk;
          c = 0;
          for ( i=0; i != 16 ; i++) {
     						if ( EEPROM_Params.midiRoutingRulesIntelliThru[port].jackOutTargetsMsk & ( 1 << i) ) {
                      *(++buff2) = i;
                      c++;
                }
     		  }
          if (c == 0 ) return 0;
          break;


     // Function 0F - USB/Serial Midi routing rules
     // 03 Transformers
     case 0x0F030000: // Cable  - Slot 1
     case 0x0F030001: // Cable  - Slot 2   
     case 0x0F030002: // Cable  - Slot 3       
     case 0x0F030100: // Serial - Slot 1
     case 0x0F030101: // Serial - Slot 2
     case 0x0F030102: // Serial - Slot 3
     { 
         uint8_t sourcePort = port; //Macro alignment         
         uint8_t slot  = (fnId & 0x0000000F);         
         src  = (fnId & 0x0000FF00) >> 8;

         if (src > 1) return 0;
         *(++buff2) = 0X03;
         *(++buff2) = src;
         *(++buff2) = port;
         *(++buff2) = slot;
         
         uint8_t isEvent;

         if (src) {
            isEvent = L3M_SERIAL_TR_IS_EVENT_TR;

            for (int b=0;b<4;b++){
              *(++buff2) =  StsToI(isEvent, L3M_SERIAL_TR_SLOT.tByte[b]);                    
            }
            *(++buff2) = StsToI(isEvent, L3M_SERIAL_TR_SLOT.tPacket.tGate.gate.lower);
            *(++buff2) = StsToI(isEvent, L3M_SERIAL_TR_SLOT.tPacket.tGate.gate.upper);
         }
         else {
            isEvent = L3M_CABLE_TR_IS_EVENT_TR;
            
            for (int b=0;b<4;b++){
              *(++buff2) =  StsToI(isEvent, L3M_CABLE_TR_SLOT.tByte[b]);
            }
            *(++buff2) = StsToI(isEvent, L3M_CABLE_TR_SLOT.tPacket.tGate.gate.lower);
            *(++buff2) = StsToI(isEvent, L3M_CABLE_TR_SLOT.tPacket.tGate.gate.upper);
         }

         break;
     }     


     // Function 0F - USB/Serial Midi routing rules
     // 02 Midi filter
     case 0x0F020000: // Cable
     case 0x0F020100: // Serial
         src  = (fnId & 0x0000FF00) >> 8;
         if (src > 1  ) return 0;
         *(++buff2) = 0X02;
         *(++buff2) = src;
         *(++buff2) = port;
         if (src) {
            *(++buff2) = EEPROM_Params.midiRoutingRulesSerial[port].filterMsk;
         }
         else {
            *(++buff2) = EEPROM_Params.midiRoutingRulesCable[port].filterMsk;
         }

         break;

     // Function 0F - USB/Serial Midi midi routing rules
     // 01 Routing rules
     case 0x0F010000: // Cable to Cable
     case 0x0F010001: // Cable to Serial
     case 0x0F010100: // Serial to Cable
     case 0x0F010101: // Serial to Serial
     src  = (fnId & 0x0000FF00) >> 8;
     dest = (fnId & 0x000000FF) ;
     if (src > 1 || dest > 1 ) return 0;
     *(++buff2) = 0X01;
     *(++buff2) = src;
     *(++buff2) = port;
     *(++buff2) = dest;

     if (src ) {
       msk = dest ? EEPROM_Params.midiRoutingRulesSerial[port].jackOutTargetsMsk : EEPROM_Params.midiRoutingRulesSerial[port].cableInTargetsMsk;
     } else {
       msk = dest ? EEPROM_Params.midiRoutingRulesCable[port].jackOutTargetsMsk : EEPROM_Params.midiRoutingRulesCable[port].cableInTargetsMsk;
     } ;

     c = 0;
     for ( i = 0 ; i != 16  ; i++) {
         if ( msk & ( 1 << i) ) {
           *(++buff2) = i;
           c++;
         }
     }
     if (c == 0 ) return 0;
     break;
  }
  *(++buff2) = 0xF7;
  return buff2-buff+1;
}

void SysexInternalDumpToStream(uint8_t dest) 
{

  uint16_t l;

  // Function 0B - Change USB Product String
  l = SysexInternalDumpConf(0x0B000000, 0, sysExInternalBuffer);
  if ( l && dest == 0 ) {ShowBufferHexDump(sysExInternalBuffer,l,0);Serial.println();}
  else if ( l && dest == 1 ) serialHw[0]->write(sysExInternalBuffer,l);
  else if ( l && dest == 2 ) SysExSendMsgPacket(sysExInternalBuffer,l);

  // Function 0C - Change USB Vendor ID and Product ID
  l = SysexInternalDumpConf(0x0C000000, 0, sysExInternalBuffer);
  if ( l && dest == 0 ) {ShowBufferHexDump(sysExInternalBuffer,l,0);Serial.println();}
  else if ( l && dest == 1 ) serialHw[0]->write(sysExInternalBuffer,l);
  else if ( l && dest == 2 ) SysExSendMsgPacket(sysExInternalBuffer,l);

  // Function 0E - Intellithru midi routing rules - 02 Timeout
  l = SysexInternalDumpConf(0x0E020000, 0, sysExInternalBuffer);
  if ( l && dest == 0 ) {ShowBufferHexDump(sysExInternalBuffer,l,0);Serial.println();}
  else if ( l && dest == 1 ) serialHw[0]->write(sysExInternalBuffer,l);
  else if ( l && dest == 2 ) SysExSendMsgPacket(sysExInternalBuffer,l);

  for ( uint8_t j=0; j != 16 ; j++) {
      // Function 0E - Intellithru midi routing rules - 03 Routing rules
      l = SysexInternalDumpConf(0x0E030000, j, sysExInternalBuffer);
      if ( l && dest == 0 ) {ShowBufferHexDump(sysExInternalBuffer,l,0);Serial.println();}
      else if ( l && dest == 1 ) serialHw[0]->write(sysExInternalBuffer,l);
      else if ( l && dest == 2 ) SysExSendMsgPacket(sysExInternalBuffer,l);
  }

  for ( uint8_t j=0; j != 16 ; j++) {
      // Function 0F - USB/Serial Midi midi transformers - 03 Midi Transformers Cable - Slot 0
      l = SysexInternalDumpConf(0x0F030000, j, sysExInternalBuffer);
      if ( l && dest == 0 ) {ShowBufferHexDump(sysExInternalBuffer,l,0);Serial.println();}
      else if ( l && dest == 1 ) serialHw[0]->write(sysExInternalBuffer,l);
      else if ( l && dest == 2 ) SysExSendMsgPacket(sysExInternalBuffer,l);
  }

    for ( uint8_t j=0; j != 16 ; j++) {
      // Function 0F - USB/Serial Midi midi transformers - 03 Midi Transformers Cable - Slot 1
      l = SysexInternalDumpConf(0x0F030001, j, sysExInternalBuffer);
      if ( l && dest == 0 ) {ShowBufferHexDump(sysExInternalBuffer,l,0);Serial.println();}
      else if ( l && dest == 1 ) serialHw[0]->write(sysExInternalBuffer,l);
      else if ( l && dest == 2 ) SysExSendMsgPacket(sysExInternalBuffer,l);
  }

  for ( uint8_t j=0; j != 16 ; j++) {
      // Function 0F - USB/Serial Midi midi transformers - 03 Midi Transformers Cable - Slot 2
      l = SysexInternalDumpConf(0x0F030002, j, sysExInternalBuffer);
      if ( l && dest == 0 ) {ShowBufferHexDump(sysExInternalBuffer,l,0);Serial.println();}
      else if ( l && dest == 1 ) serialHw[0]->write(sysExInternalBuffer,l);
      else if ( l && dest == 2 ) SysExSendMsgPacket(sysExInternalBuffer,l);
  }

  for ( uint8_t j=0; j != 16 ; j++) {
      // Function 0F - USB/Serial Midi transformers - 03 Midi Transformers Serial - Slot 0
      l = SysexInternalDumpConf(0x0F030100, j, sysExInternalBuffer);
      if ( l && dest == 0 ) {ShowBufferHexDump(sysExInternalBuffer,l,0);Serial.println();}
      else if ( l && dest == 1 ) serialHw[0]->write(sysExInternalBuffer,l);
      else if ( l && dest == 2 ) SysExSendMsgPacket(sysExInternalBuffer,l);
  }

  for ( uint8_t j=0; j != 16 ; j++) {
      // Function 0F - USB/Serial Midi transformers - 03 Midi Transformers Serial - Slot 1
      l = SysexInternalDumpConf(0x0F030101, j, sysExInternalBuffer);
      if ( l && dest == 0 ) {ShowBufferHexDump(sysExInternalBuffer,l,0);Serial.println();}
      else if ( l && dest == 1 ) serialHw[0]->write(sysExInternalBuffer,l);
      else if ( l && dest == 2 ) SysExSendMsgPacket(sysExInternalBuffer,l);
  }

  for ( uint8_t j=0; j != 16 ; j++) {
      // Function 0F - USB/Serial Midi transformers - 03 Midi Transformers Serial - Slot 2
      l = SysexInternalDumpConf(0x0F030102, j, sysExInternalBuffer);
      if ( l && dest == 0 ) {ShowBufferHexDump(sysExInternalBuffer,l,0);Serial.println();}
      else if ( l && dest == 1 ) serialHw[0]->write(sysExInternalBuffer,l);
      else if ( l && dest == 2 ) SysExSendMsgPacket(sysExInternalBuffer,l);
  }

  for ( uint8_t j=0; j != 16 ; j++) {
      // Function 0F - USB/Serial Midi midi routing rules - 02 Midi filter Cable
      l = SysexInternalDumpConf(0x0F020000, j, sysExInternalBuffer);
      if ( l && dest == 0 ) {ShowBufferHexDump(sysExInternalBuffer,l,0);Serial.println();}
      else if ( l && dest == 1 ) serialHw[0]->write(sysExInternalBuffer,l);
      else if ( l && dest == 2 ) SysExSendMsgPacket(sysExInternalBuffer,l);
  }

  for ( uint8_t j=0; j != 16 ; j++) {
      // Function 0F - USB/Serial Midi midi routing rules - 02 Midi filter Serial
      l = SysexInternalDumpConf(0x0F020100, j, sysExInternalBuffer);
      if ( l && dest == 0 ) {ShowBufferHexDump(sysExInternalBuffer,l,0);Serial.println();}
      else if ( l && dest == 1 ) serialHw[0]->write(sysExInternalBuffer,l);
      else if ( l && dest == 2 ) SysExSendMsgPacket(sysExInternalBuffer,l);
  }
  for ( uint8_t j=0; j != 16 ; j++) {
      // Function 0F - USB/Serial Midi midi routing rules - Cable to Cable
      l = SysexInternalDumpConf(0x0F010000, j, sysExInternalBuffer);
      if ( l && dest == 0 ) {ShowBufferHexDump(sysExInternalBuffer,l,0);Serial.println();}
      else if ( l && dest == 1 ) serialHw[0]->write(sysExInternalBuffer,l);
      else if ( l && dest == 2 ) SysExSendMsgPacket(sysExInternalBuffer,l);
  }

  for ( uint8_t j=0; j != 16 ; j++) {
      // Function 0F - USB/Serial Midi midi routing rules - Cable to Serial
      l = SysexInternalDumpConf(0x0F010001, j, sysExInternalBuffer);
      if ( l && dest == 0 ) {ShowBufferHexDump(sysExInternalBuffer,l,0);Serial.println();}
      else if ( l && dest == 1 ) serialHw[0]->write(sysExInternalBuffer,l);
      else if ( l && dest == 2 ) SysExSendMsgPacket(sysExInternalBuffer,l);
  }

  for ( uint8_t j=0; j != 16 ; j++) {
      // Function 0F - USB/Serial Midi midi routing rules - Serial to Cable
      l = SysexInternalDumpConf(0x0F010100, j, sysExInternalBuffer);
      if ( l && dest == 0 ) {ShowBufferHexDump(sysExInternalBuffer,l,0);Serial.println();}
      else if ( l && dest == 1 ) serialHw[0]->write(sysExInternalBuffer,l);
      else if ( l && dest == 2 ) SysExSendMsgPacket(sysExInternalBuffer,l);
   }

   for ( uint8_t j=0; j != 16 ; j++) {
      // Function 0F - USB/Serial Midi midi routing rules - Serial to Serial
      l = SysexInternalDumpConf(0x0F010101, j, sysExInternalBuffer);
      if ( l && dest == 0 ) {ShowBufferHexDump(sysExInternalBuffer,l,0);Serial.println();}
      else if ( l && dest == 1 ) serialHw[0]->write(sysExInternalBuffer,l);
      else if ( l && dest == 2 ) SysExSendMsgPacket(sysExInternalBuffer,l);
    }
}

void SysExInternalProcess(uint8_t source)
{
  uint8_t msgLen = sysExInternalBuffer[0];
  uint8_t cmdId  = sysExInternalBuffer[1];

  switch (cmdId) {

    // ---------------------------------------------------------------------------
    // Function 05 Sysex configuration dump
    // Example : F0 77 77 78 05 F7
    // ---------------------------------------------------------------------------
    case 0x05:
      if (source == FROM_USB && midiUSBCx) {
        // send to USB , cable 0
        SysexInternalDumpToStream(2);
      } else
      if (source == FROM_SERIAL ) {
        // Send to serial port 0 being the only possible for sysex
        SysexInternalDumpToStream(1);
      }
      break;

    // ---------------------------------------------------------------------------
    // Function 06 subId 0x01 - Identity request.
    // Example : F0 77 77 78 06 01 F7
    // ---------------------------------------------------------------------------
    case 0x06:
      if ( sysExInternalBuffer[2] == 0x01 && msgLen == 2 ) {

          if (source == FROM_USB && midiUSBCx) {
            // send to USB , cable 0
            SysExSendMsgPacket(sysExInternalIdentityRqReply,sizeof(sysExInternalIdentityRqReply));
          } else
          if (source == FROM_SERIAL ) {
            // Send to serial port 0 being the only possible for sysex
            serialHw[0]->write(sysExInternalIdentityRqReply,sizeof(sysExInternalIdentityRqReply));
          }
      }
      break;

    // ---------------------------------------------------------------------------
    // Function 08 - Reboot in serial configuration mode
    // Example : F0 77 77 78 08 F7
    // ---------------------------------------------------------------------------
    case 0x08:
      // Set serial boot mode & Write the whole param struct
      EEPROM_Params.nextBootMode = bootModeConfigMenu;
      EEPROM_ParamsSave();
      nvic_sys_reset();
      break;

    // ---------------------------------------------------------------------------
    // Function 0A - Reboot unit
    // Example : F0 77 77 78 0A F7
    // ---------------------------------------------------------------------------
    case 0x0A:
      nvic_sys_reset();
      break;

    // ---------------------------------------------------------------------------
    // Function 0B - Change USB Product String
    // F0 77 77 78 0B <string bytes> F7
    // ---------------------------------------------------------------------------
    // Copy the received string to the USB Product String Descriptor
    // For MIDI protocol compatibility, and avoid a sysex encoding,
    // accentuated ASCII characters, below 128 are non supported.
    // Size if defined by USB_MIDI_PRODUCT_STRING_SIZE in UsbMidiKliK4x4.h
    // ---------------------------------------------------------------------------
    case 0x0B:
      if (msgLen < 2) break;
      if ( (msgLen-1) > USB_MIDI_PRODUCT_STRING_SIZE  ) break;

      // Store the new string in EEPROM
      memset(&EEPROM_Params.productString,0, sizeof(EEPROM_Params.productString));
      memcpy(&EEPROM_Params.productString,&sysExInternalBuffer[2],msgLen-1);
      EEPROM_ParamsSave();
      break;

    // ---------------------------------------------------------------------------
    // Function 0C - Change USB Vendor ID and Product ID
    // F0 77 77 78 0C <n1 n2 n3 n4 = VID nibbles> <n1 n2 n3 n4 = PID nibbles> F7
    // ---------------------------------------------------------------------------
    // To respect a simple encoding of 7 bits bytes, each hex digit must be
    // transmitted separately in a serialized way.
    // The following example will set  VID to 0X8F12 and PID to 0X9067 :
    // F0 77 77 78 0C 08 0F 01 02 09 00 06 07 F7
    //                8  F  1  2  9  0  6  7
    case 0x0C:
      if ( msgLen != 9 ) break;
      EEPROM_Params.vendorID = (sysExInternalBuffer[2] << 12) + (sysExInternalBuffer[3] << 8) +
                                     (sysExInternalBuffer[4] << 4) + sysExInternalBuffer[5] ;
      EEPROM_Params.productID= (sysExInternalBuffer[6] << 12) + (sysExInternalBuffer[7] << 8) +
                                     (sysExInternalBuffer[8] << 4) + sysExInternalBuffer[9] ;
      EEPROM_ParamsSave();
      break;

    // ---------------------------------------------------------------------------
    // Function 0E - Intellithru midi routing rules
    // ---------------------------------------------------------------------------
    // IntelligentThru can be activated when USB is sleeping or unavailable beyond a certain timeout.
    // F0 77 77 78 0E  < Routing rules command <command args>   >
    //
    // Commands are :
    //  00 Reset Intellithru to default
    //  01 Disable Intellithru
    //  02 Set  Intellithru timeout
    //      arg1 : 0xnn (number of 15s periods 1-127)
    //  03 Set thru mode jack routing
    //      arg1 - Midi In Jack = 0xnn (0-F)
    //      arg2 - Midi filter mask (binary OR)
    //                => zero if you want to inactivate intelliThru for this jack
    //                  channel Voice = 0001 (1), (binary OR)
    //                  system Common = 0010 (2), (binary OR)
    //                  realTime      = 0100 (4), (binary OR)
    //                  sysEx         = 1000 (8)
    //      arg3 - Serial midi Jack out targets
    //            <t1> <t2>...<tn>  0-F 16 targets maximum.
    // EOX = F7
    // Examples :
    // F0 77 77 78 0E 00 F7                    <= Reset to default
    // F0 77 77 78 0E 01 F7                    <= Disable
    // F0 77 77 78 0E 02 02 F7                 <= Set timeout to 30s
    // F0 77 77 78 0E 03 01 0F 00 01 02 03 F7  <= Route Midi In Jack 2 to Jacks out 1,2,3,4 All msg
    // F0 77 77 78 0E 03 03 04 03 04 F7        <= Route Midi In Jack 4 to Jacks out 4,5, real time only

    case 0x0E:
      if ( msgLen < 2 ) break;

      // reset to default midi thru routing
      if (sysExInternalBuffer[2] == 0x00  && msgLen == 2) {
         ResetMidiRoutingRules(ROUTING_RESET_INTELLITHRU);
      } else

      // Disable thru mode
      if (sysExInternalBuffer[2] == 0x01  && msgLen == 3) {
          ResetMidiRoutingRules(ROUTING_INTELLITHRU_OFF);
      } else

      // Set Delay
      // The min delay is 1 period of 15 secondes. The max is 127 periods of 15 secondes.
      if (sysExInternalBuffer[2] == 0x02  && msgLen == 3) {
        if ( sysExInternalBuffer[3] < 1 || sysExInternalBuffer[3] > 0X7F ) break;
        EEPROM_Params.intelliThruDelayPeriod = sysExInternalBuffer[3];
      } else

      // Set routing : Midin 2 mapped to all events. All 4 ports.
      // 0E 03 01 0F 00 01 02 03
      // 0E 03 01 0F 00
      if (sysExInternalBuffer[2] == 3 ) {
          if (msgLen < 4 ) break;
          if ( msgLen > SERIAL_INTERFACE_COUNT + 4 ) break;

          uint8_t src = sysExInternalBuffer[3];
          uint8_t filterMsk = sysExInternalBuffer[4];

          if ( src >= SERIAL_INTERFACE_COUNT) break;

          // Filter is 4 bits
          if ( filterMsk > 0x0F  ) break;

          // Disable Thru mode for this jack if no filter
          if ( filterMsk == 0 && msgLen ==4 ) {
            EEPROM_Params.intelliThruJackInMsk &= ~(1 << src);
          }
          else {
              if ( filterMsk == 0 ) break;
              // Add midi in jack
              EEPROM_Params.intelliThruJackInMsk |= (1 << src);
              // Set filter
              EEPROM_Params.midiRoutingRulesIntelliThru[src].filterMsk = filterMsk;
              // Set Jacks
              if ( msgLen > 4)  {
                uint16_t msk = 0;
                for ( uint8_t i = 5 ; i != (msgLen+1)  ; i++) {
                    if ( sysExInternalBuffer[i] < SERIAL_INTERFACE_COUNT)
                      msk |=  1 << sysExInternalBuffer[i] ;
                }
                EEPROM_Params.midiRoutingRulesIntelliThru[src].jackOutTargetsMsk = msk;
              }
          }
      }
      else  break;

      // Write the whole param struct
      EEPROM_ParamsSave();
      // reset globals for a real time update
      intelliThruDelayMillis = EEPROM_Params.intelliThruDelayPeriod * 15000;
      // Synchronize slaves routing rules if bus active and master
      if (B_IS_MASTER) I2C_SlavesRoutingSyncFromMaster();

      break;

    // ---------------------------------------------------------------------------
    // Function 0F - USB/Serial Midi midi routing rules
    // ---------------------------------------------------------------------------
    // F0 77 77 78 0F  < Routing rules command <command args>   >
    //
    // Commands are :
    //  00 Reset to default midi routing
    //  01 Set routing +
    //      arg1 - source type : <0x00 usb cable | 0x01 jack serial>
    //      arg2 - port id : id for cable or jack serial (0-F)
    //      arg3 - destination = <0x00 usb cable in | 0x01 jack serial out>
    //      arg4 - targets : <port 0 1 2...n> 16 max (0-F)
    //  02  Midi filter
    //      arg1 - source type : : <0x00 usb cable | 0x01 jack serial>
    //      arg2 - port id : id for cable or jack serial (0-F)
    //      arg3 - midi filter mask (binary OR)
    //                => zero if you want to inactivate intelliThru for this jack
    //                  channel Voice = 0001 (1), (binary OR)
    //                  system Common = 0010 (2), (binary OR)
    //                  realTime      = 0100 (4), (binary OR)
    //                  sysEx         = 1000 (8)
    // EOX = F7
      //
    // Examples :
    // F0 77 77 78 0F 00 F7                      <= reset to default midi routing
    // F0 77 77 78 0F 02 00 00 04 F7             <= Set filter to realtime events on cable 0
    // F0 77 77 78 0F 01 00 00 01 00 01 F7       <= Set Cable 0 to Jack 1,2
    // F0 77 77 78 0F 01 00 00 00 00 01 F7       <= Set Cable 0 to Cable In 0, In 01
    // F0 77 77 78 0F 01 00 00 01 00 01 F7       <= & jack 1,2 (2 msg)
    // F0 77 77 78 0F 01 01 01 01 00 01 02 03 F7 <= Set Serial jack In No 2 to serial jacks out 1,2,3,4

    case 0x0F:

      // reset to default routing

      if (sysExInternalBuffer[2] == 0x00  && msgLen == 2) {
          ResetMidiRoutingRules(ROUTING_RESET_MIDIUSB);
      } else

      // Set transformer
      if (sysExInternalBuffer[2] == 0x03) {

          // Set
          // F0 77 77 78 0F 03 01 00 00 00 0B 1A 19 00 1A 1A F7 
          // F0 77 77 78 0F 03 01   : Header, Route/Filter/Trans, Trans, Set
          // 00 00 00               : cable, cable 0, transformer slot 0
          // 0B 1A 19 00            : Event Map, Continue, Start
          // 1A 1A                  : Continue, Continue
          // F7                     : EOX

          // Clear
          // F0 77 77 78 0F 03 00 00 00 00 00 00 00 00 00 00 F7 
          // F0 77 77 78 0F 03 00   : Header, Route/Filter/Trans, Trans, Clear
          // 00 00                  : cable, cable 0
          // 00 00 00 00 00 00 00   : NA
          // F7                     : EOX
          
          uint8_t clearOrSet = sysExInternalBuffer[3];
          uint8_t srcType = sysExInternalBuffer[4];
          uint8_t sourcePort = sysExInternalBuffer[5];
          uint8_t slot = sysExInternalBuffer[6];
          uint8_t command = sysExInternalBuffer[7];
          uint8_t xbyte = sysExInternalBuffer[8];
          uint8_t ybyte = sysExInternalBuffer[9];
          uint8_t zbyte = sysExInternalBuffer[10]; 
          uint8_t lowerStsBound = sysExInternalBuffer[11];
          uint8_t upperStsBound = sysExInternalBuffer[12];       

          if (srcType == 0 ) { // Cable
            if ( sourcePort  >= USBCABLE_INTERFACE_MAX) break;     

                if (clearOrSet == 0){
                  L3M_CABLE_TR_SLOT.i = 0;
                } else {

                  L3M_CABLE_TR_SLOT.tPacket.tCmdCode = command;
                  L3M_CABLE_TR_SLOT.tPacket.tParms.x = IToSts(command, xbyte);
                  L3M_CABLE_TR_SLOT.tPacket.tParms.y = IToSts(command, ybyte);
                  L3M_CABLE_TR_SLOT.tPacket.tParms.z = IToSts(command, zbyte);         

                  L3M_CABLE_TR_SLOT.tPacket.tGate.gate.lower = IToSts(command,lowerStsBound);
                  L3M_CABLE_TR_SLOT.tPacket.tGate.gate.upper = IToSts(command,upperStsBound);
                  
                  L3M_CABLE_TR_UNIT.inUseCount = slot + 1; 
                }
                
          } else

          if (srcType == 1) { // Serial
            if ( sourcePort >= SERIAL_INTERFACE_COUNT) break;        

                if (clearOrSet == 0){
                  L3M_CABLE_TR_SLOT.i = 0;
                } else {

                  L3M_SERIAL_TR_SLOT.tPacket.tCmdCode = command;
                  L3M_SERIAL_TR_SLOT.tPacket.tParms.x = IToSts(command, xbyte);
                  L3M_SERIAL_TR_SLOT.tPacket.tParms.y = IToSts(command, ybyte);
                  L3M_SERIAL_TR_SLOT.tPacket.tParms.z = IToSts(command, zbyte);         

                  L3M_SERIAL_TR_SLOT.tPacket.tGate.gate.lower = IToSts(command, lowerStsBound);
                  L3M_SERIAL_TR_SLOT.tPacket.tGate.gate.upper = IToSts(command, upperStsBound);
                  
                  L3M_SERIAL_TR_UNIT.inUseCount = slot + 1; 
              }

          } else break;


      } else      

      // Set filter
      if (sysExInternalBuffer[2] == 0x02  && msgLen == 5) {

          uint8_t srcType = sysExInternalBuffer[3];
          uint8_t src = sysExInternalBuffer[4];
          uint8_t filterMsk = sysExInternalBuffer[5];

          // Filter is 4 bits
          if ( filterMsk > 0x0F  ) break;

          if (srcType == 0 ) { // Cable
            if ( src  >= USBCABLE_INTERFACE_MAX) break;
                EEPROM_Params.midiRoutingRulesCable[src].filterMsk = filterMsk;
          } else

          if (srcType == 1) { // Serial
            if ( src >= SERIAL_INTERFACE_COUNT) break;
              EEPROM_Params.midiRoutingRulesSerial[src].filterMsk = filterMsk;
          } else break;


      } else

      // Set Routing targets

      if (sysExInternalBuffer[2] == 0x01  )
      {

        if (msgLen < 6) break;

        uint8_t srcType = sysExInternalBuffer[3];
        uint8_t dstType = sysExInternalBuffer[5];
        uint8_t src = sysExInternalBuffer[4];

        if (srcType != 0 && srcType != 1 ) break;
        if (dstType != 0 && dstType != 1 ) break;
        if (srcType  == 0 && src >= USBCABLE_INTERFACE_MAX ) break;
        if (srcType  == 1 && src >= SERIAL_INTERFACE_COUNT) break;
        if (dstType  == 0 &&  msgLen > (USBCABLE_INTERFACE_MAX + 5) )  break;
        if (dstType  == 1 &&  msgLen > (SERIAL_INTERFACE_COUNT + 5) )  break;

        // Compute mask from the port list
        uint16_t msk = 0;
        for ( uint8_t i = 6 ; i != (msgLen+1)  ; i++) {
            uint8_t b = sysExInternalBuffer[i];
            if ( (dstType == 0 && b < USBCABLE_INTERFACE_MAX) ||
                 (dstType == 1 && b < SERIAL_INTERFACE_COUNT) ) {

                   msk |=   1 << b ;
            }
        }// for

        // Set masks
        if ( srcType == 0 ) { // Cable
            if (dstType == 0) // To cable
              EEPROM_Params.midiRoutingRulesCable[src].cableInTargetsMsk = msk;
            else // To serial
              EEPROM_Params.midiRoutingRulesCable[src].jackOutTargetsMsk = msk;

        } else

        if ( srcType == 1 ) { // Serial
          if (dstType == 0)
            EEPROM_Params.midiRoutingRulesSerial[src].cableInTargetsMsk = msk;
          else
            EEPROM_Params.midiRoutingRulesSerial[src].jackOutTargetsMsk = msk;
        }

      } else
          return;

      // Write the whole param struct
      EEPROM_ParamsSave();

      // Synchronize slaves routing rules
      if (B_IS_MASTER) I2C_SlavesRoutingSyncFromMaster();

      break;

  }
}

#endif
