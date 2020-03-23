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
///////////////////////////////////////////////////////////////////////////////
//  MIDI TRANSFORMATION PIPES FUNCTIONS
//-----------------------------------------------------------------------------
// Due to the unusual make process of Arduino platform, this file is directly
// included in the main ino one.
//
// The midi transformation engine relies on a pipeline process. A "pipe" is
// an elementary midi tranformation function accepting a midi packet and some
// parameters. So a pipeline, is a chain of transformation functions, delivering
// a packet (or many) from a source port to a destination port.
//
// For each pipe  :
//   . A MidiTransFn_(pipe name)_CheckParms function to validate input prms
//   . A MidiTransFn_(pipe name) function being the transformation pipe itself
//   . Declaration of an fnId in the MidiTransPipeFnID enum before FN_TRANSPIPE_VECTOR_SIZE
//   . An entry in the MidiTransFnVector functio pointer array with the pipe fn address
//   . An entry in the MidiTransFnVector_CheckParms array with the check parms fn address

// parameters are not checked at all at run time for speed optimization reason.
///////////////////////////////////////////////////////////////////////////////

#pragma once

//boolean MidiTransFn_MYPIPE_CheckParms(midiTransPipe_t *);
//boolean MidiTransFn_MYPIPE(uint8_t, midiPacket_t *, midiTransPipe_t *);

boolean MidiTransFn_NoteChanger_CheckParms(midiTransPipe_t *);
boolean MidiTransFn_NoteChanger(uint8_t, midiPacket_t *, midiTransPipe_t *);

boolean MidiTransFn_ChannelMapper_CheckParms(midiTransPipe_t *);
boolean MidiTransFn_ChannelMapper(uint8_t, midiPacket_t *, midiTransPipe_t *);

boolean MidiTransFn_VeloChanger_CheckParms(midiTransPipe_t *);
boolean MidiTransFn_VeloChanger(uint8_t, midiPacket_t *, midiTransPipe_t *);

boolean MidiTransFn_CCChanger_CheckParms(midiTransPipe_t *);
boolean MidiTransFn_CCChanger(uint8_t, midiPacket_t *, midiTransPipe_t *);

boolean MidiTransFn_ClockDivider_CheckParms(midiTransPipe_t *);
boolean MidiTransFn_ClockDivider(uint8_t, midiPacket_t *, midiTransPipe_t *);

///////////////////////////////////////////////////////////////////////////////
//  Midi transformation functions vector
///////////////////////////////////////////////////////////////////////////////

#define FN_TRANSPIPE_NOPIPE -1

enum MidiTransPipeFnID {
  FN_TRANSPIPE_NOTE_CHANGER,
  FN_TRANSPIPE_CHANNEL_MAPPER,
  FN_TRANSPIPE_VELO_CHANGER,
  FN_TRANSPIPE_CC_CHANGER,
  FN_TRANSPIPE_CLOCK_DIVIDER,
  FN_TRANSPIPE_VECTOR_SIZE,
} ;

typedef boolean (*MidiTransFnPt) (uint8_t source, midiPacket_t *pk, midiTransPipe_t *pipe) ;
typedef boolean (*MidiTransFnPt_CheckParms) (midiTransPipe_t *pipe) ;

MidiTransFnPt MidiTransFnVector[] = {
  &MidiTransFn_NoteChanger,
  &MidiTransFn_ChannelMapper,
  &MidiTransFn_VeloChanger,
  &MidiTransFn_CCChanger,
  &MidiTransFn_ClockDivider,
};

MidiTransFnPt_CheckParms MidiTransFnVector_CheckParms[] = {
  &MidiTransFn_NoteChanger_CheckParms,
  &MidiTransFn_ChannelMapper_CheckParms,
  &MidiTransFn_VeloChanger_CheckParms,
  &MidiTransFn_CCChanger_CheckParms,
  &MidiTransFn_ClockDivider_CheckParms,
};

///////////////////////////////////////////////////////////////////////////////
// PIPES
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Note changer. 00
//----------------------------------------------------------------------------
// par1  : 0 Transpose + (par2 = semitones)
//       : 1 Transpose - (par2 = semitones)
//       : 2 Split       Split point :note >=par2, par3 = midichannel 0 to F
//       : 3 Velo split  Velo value >=par2, par3 = ch 0 to F, par4 = fixed velo or 0 if no change
///////////////////////////////////////////////////////////////////////////////

boolean MidiTransFn_NoteChanger_CheckParms(midiTransPipe_t *pipe) {
  if ( pipe->par1 > 3 ) return false;
  if ( pipe->par3 > 0x0F ) return false; // Split &  velo Split
  return true;
}

boolean MidiTransFn_NoteChanger(uint8_t source, midiPacket_t *pk, midiTransPipe_t *pipe) {

  uint8_t midiStatus = pk->packet[1] & 0xF0;

  // Only notes ON/OFF
  if ( midiStatus != midiXparser::noteOffStatus && midiStatus != midiXparser::noteOnStatus)
        return true;

  // Add semitones to the note value
  if (pipe->par1 == 0 )
    pk->packet[2] = ( pk->packet[2] + pipe->par2 > 127 ? pk->packet[2] : pk->packet[2] + pipe->par2 );
  else
  // Sub semitones to the note value
  if (pipe->par1 == 1 )
    pk->packet[2] = ( pipe->par2 >= pk->packet[2] ? pk->packet[2] : pk->packet[2] - pipe->par2 );
  else
  // Split point
  if (pipe->par1 == 2 ) {
    if ( pk->packet[2] >= pipe->par2  )
        pk->packet[1] = midiStatus + pipe->par3;
  }
  // Velo Split
  if (pipe->par1 == 3 ) {

    if ( midiStatus == midiXparser::noteOffStatus ) {
      // Send a note off to the channel to ensure note stop  in all case
      // as no history of note on exists.
      // packet copy
      midiPacket_t pk2 = { .i = pk->i };
      pk2.packet[1] = midiStatus + pipe->par3;
      RoutePacketToTarget(source, &pk2);
    }
    else
    if ( pk->packet[3] >= pipe->par2  ) {
        pk->packet[1] = midiStatus + pipe->par3;
        if (pipe->par4) pk->packet[3] = pipe->par4;
    }
  }

  return true;
}

///////////////////////////////////////////////////////////////////////////////
// Midi channel mapper 01
//----------------------------------------------------------------------------
// par1 : 0 Map channel(par2= source channel- 0x7F=any, par3= dest channel).
//      : 1 Map to port (par2=source channel, par3=midi port)
///////////////////////////////////////////////////////////////////////////////
boolean MidiTransFn_ChannelMapper_CheckParms(midiTransPipe_t *pipe) {
  if ( pipe->par1 > 1 ) return false;
  if ( pipe->par1 == 0 && pipe->par2 > 0x0F && pipe->par2 != 0x7F ) return false;
  if ( pipe->par1 == 1 && pipe->par2 > 0x0F ) return false;
  if ( pipe->par3 > 0x0F ) return false;
  return true;
}

boolean MidiTransFn_ChannelMapper(uint8_t source, midiPacket_t *pk, midiTransPipe_t *pipe) {

  if (midiXparser::getMidiStatusMsgTypeMsk(pk->packet[1]) !=  midiXparser::channelVoiceMsgTypeMsk)
      return true;

  // Map channel to channel
  if (pipe->par1 == 0 ) {
    // Any or matching channel
    if ( (pipe->par2 == 0x7f) || ( pipe->par2 == (pk->packet[1] & 0x0F) ) )
      pk->packet[1] = (pk->packet[1] & 0xF0) + pipe->par3;
  }

  else
  // Map channel to port
  if (pipe->par1 == 1 ) {
    if ( pipe->par2 == (pk->packet[1] & 0x0F) )
        pk->packet[0] = (pk->packet[0] & 0x0F) + ( pipe->par3 << 4);
  }

  return true;
}

///////////////////////////////////////////////////////////////////////////////
// Midi Velocity changer
//----------------------------------------------------------------------------
// par1 : 00 half velocity
//        01 full velocity
//        02 include notes velo range [par2,par3]
//        03 exclude notes velo range [par2,par3]
///////////////////////////////////////////////////////////////////////////////
boolean MidiTransFn_VeloChanger_CheckParms(midiTransPipe_t *pipe) {
  if ( pipe->par1 > 3 ) return false;
  if ( (pipe->par1 == 2 || pipe->par1 == 3 ) && ( pipe->par2 > pipe->par3 ) ) return false;
  return true;
}

boolean MidiTransFn_VeloChanger(uint8_t source, midiPacket_t *pk, midiTransPipe_t *pipe) {

  uint8_t midiStatus = pk->packet[1] & 0xF0;

  // Only notes ON
  if ( midiStatus != midiXparser::noteOnStatus)
        return true;

  // Half velocity
  if ( pipe->par1 == 0x00 ) {
    pk->packet[3] /= 2;
  }
  else
  // Full velocity
  if ( pipe->par1 == 0x01 ) {
    pk->packet[3] = 127;
  }
  else
  // Include range
  if ( pipe->par1 == 0x02 ) {
    if ( pk->packet[3] < pipe->par2 || pk->packet[3] > pipe->par3 )
        return false; // drop packet
  }
  else
  // Exclude range
  if ( pipe->par1 == 0x03 ) {
    if ( pk->packet[3] >= pipe->par2 || pk->packet[3] <= pipe->par3 )
        return false; // drop packet
  }

  return true;
}

///////////////////////////////////////////////////////////////////////////////
// CC changer
//----------------------------------------------------------------------------
// par1 : 00 map(par2=source cc,par3=dest cc)
//        01 filter include range[par2,par3])
//        02 filter exclude range[par2,par3])
//        03 invert(par2=source cc) : 0=127, 127 = 0
///////////////////////////////////////////////////////////////////////////////
boolean MidiTransFn_CCChanger_CheckParms(midiTransPipe_t *pipe) {
  if ( pipe->par1 > 3 ) return false;
  if ( (pipe->par1 == 1 || pipe->par1 == 2 ) && ( pipe->par2 > pipe->par3 ) ) return false;
  return true;
}

boolean MidiTransFn_CCChanger(uint8_t source, midiPacket_t *pk, midiTransPipe_t *pipe) {

  uint8_t midiStatus = pk->packet[1] & 0xF0;

  // Only notes ON
  if ( midiStatus != midiXparser::controlChangeStatus)
        return true;

  // Map
  if ( pipe->par1 == 0x00 ) {
    if ( pk->packet[2] == pipe->par2 ) pk->packet[2] = pipe->par3;
  }
  else
  // Include range
  if ( pipe->par1 == 0x01 ) {
    if ( pk->packet[2] < pipe->par2 || pk->packet[3] > pipe->par3 )
        return false; // drop packet
  }
  else
  // Exclude range
  if ( pipe->par1 == 0x02 ) {
    if ( pk->packet[2] >= pipe->par2 || pk->packet[3] <= pipe->par3 )
        return false; // drop packet
  }
  else
  // Invert
  if ( pipe->par1 == 0x03 ) {
    if ( pk->packet[2] == pipe->par2 )
        pk->packet[3] = 127 - pk->packet[3] ;
  }

  return true;
}

///////////////////////////////////////////////////////////////////////////////
// Midi Clock Divider
//----------------------------------------------------------------------------
// par1 : ratio : 2 to 16
// Example for a div 2 : 1---(2)---3---(4)---5---(6)---7---(8)
// Example for a div 3 : 1---2---(3)---4---5---(6)---7---8---(9)
// (x) means clock sent
///////////////////////////////////////////////////////////////////////////////
boolean MidiTransFn_ClockDivider_CheckParms(midiTransPipe_t *pipe) {
  if ( pipe->par1 < 2 || pipe->par1 >16 ) return false;
  return true;
}

boolean MidiTransFn_ClockDivider(uint8_t source, midiPacket_t *pk, midiTransPipe_t *pipe) {

  if ( pk->packet[1] != midiXparser::timingClockStatus)
      return true;

  static uint8_t clockCounter = 0;
  if ( ++clockCounter == pipe->par1 ) {
      clockCounter = 0;
      return true;
  }

  return false;
}
