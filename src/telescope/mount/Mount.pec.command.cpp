//--------------------------------------------------------------------------------------------------
// telescope mount control, commands

#include "Mount.h"

#ifdef MOUNT_PRESENT

#include "../../tasks/OnTask.h"
extern Tasks tasks;
#include "site/Site.h"

bool Mount::commandPec(char *reply, char *command, char *parameter, bool *supressFrame, bool *numericReply, CommandError *commandError) {
  *supressFrame = false;
  *commandError = CE_NONE;

  if (command[0] == 'G' && command[1] == 'X' && parameter[2] == 0) {
    #if AXIS1_PEC == ON
      // :GX91#     Get PEC analog value
      //            Returns: n#
      if (parameter[0] == '9' && parameter[1] == '1') {
        sprintf(reply, "%d", pecAnalogValue);
        *numericReply = false;
      } else
    #endif

    // :GXE6#     Get pec worm rotation steps (from NV)
    //            Returns: n#
    if (parameter[0] == 'E' && parameter[1] == '6') {
      sprintF(reply, "%0.6f", stepsPerSiderealSecondAxis1);
      *numericReply = false;
    } else

    // :GXE7#     Get pec worm rotation steps (from NV)
    //            Returns: n#
    if (parameter[0] == 'E' && parameter[1] == '7') {
      Pec tempPec;
      nv.readBytes(NV_MOUNT_PEC_BASE, &tempPec, PecSize);
      sprintf(reply, "%ld", tempPec.wormRotationSteps);
      *numericReply = false;
    } else

    // :GXE8#     Get pec buffer size in seconds
    //            Returns: n#
    if (parameter[0] == 'E' && parameter[1] == '8') {
      sprintf(reply,"%ld",pecBufferSize);
      *numericReply = false;
    } else return false;
  } else

  #if AXIS1_PEC == ON
    // :SXE7,[n]#
    //            Set PEC steps per worm rotation [n]
    //            Return: 0 on failure or 1 on success
    if (command[0] == 'S' && command[1] == 'X' && parameter[0] == 'E' && parameter[1] == '7' && parameter[2] == ',') {
      if (parameter[2] != ',') { *commandError = CE_PARAM_FORM; return true; } 
      long l = atol(&parameter[3]);
      if (l >= 0 && l < 129600000) {
        Pec tempPec = pec;
        tempPec.wormRotationSteps = l;
        nv.updateBytes(NV_MOUNT_PEC_BASE, &tempPec, PecSize);
      } else *commandError = CE_PARAM_RANGE;
    } else
  #endif

  // V - PEC Readout
  if (command[0] == 'V') {
    #if AXIS1_PEC == ON
      //  :VH#      PEC index sense position in sidereal seconds
      //            Returns: n#
      if (command[1] == 'H' && parameter[0] == 0) {
        long s = lroundf(wormSenseSteps/stepsPerSiderealSecondAxis1);
        while (s > wormRotationSeconds) s -= wormRotationSeconds;
        while (s < 0) s += wormRotationSeconds;
        sprintf(reply,"%05ld",s);
        *numericReply = false;
      } else

      // :VR[n]#    Read PEC table entry rate adjustment (in steps +/-) for worm segment n (in seconds)
      //            Returns: sn#
      // :VR#       Read PEC table entry rate adjustment (in steps +/-) for currently playing segment and its rate adjustment (in steps +/-)
      //            Returns: sn,n#
      if (command[1] == 'R') {
        int16_t i, j;
        bool conv_result = true;
        if (parameter[0] == 0) i = pecBufferIndex; else conv_result = convert.atoi2(parameter, &i);
        if (conv_result) {
          if (i >= 0 && i < pecBufferSize) {
            if (parameter[0] == 0) {
              i -= 1;
              if (i < 0) i += wormRotationSeconds;
              if (i >= wormRotationSeconds) i -= wormRotationSeconds;
              j = pecBuffer[i];
              sprintf(reply,"%+04i,%03i", j, i);
            } else {
              j = pecBuffer[i];
              sprintf(reply,"%+04i", j);
            }
          } else *commandError = CE_PARAM_RANGE;
        } else *commandError = CE_PARAM_FORM;
        *numericReply = false;
      } else

      // :Vr[n]#    Read out RA PEC ten byte frame in hex format starting at worm segment n (in seconds)
      //            Returns: x0x1x2x3x4x5x6x7x8x9# (hex one byte integers)
      //            Ten rate adjustment factors for 1s worm segments in steps +/- (steps = x0 - 128, etc.)
      if (command[1] == 'r') {
        int16_t i, j;
        if (convert.atoi2(parameter, &i)) {
          if (i >= 0 && i < pecBufferSize) {
            j = 0;
            uint8_t b;
            char s[3] = "  ";
            for (j = 0; j < 10; j++) {
              if (i + j < pecBufferSize) b = (int)pecBuffer[i + j] + 128; else b = 128;
              sprintf(s, "%02X", b);
              strcat(reply, s);
            }
          } else *commandError = CE_PARAM_RANGE;
        } else *commandError = CE_PARAM_FORM;
        *numericReply = false;
      } else
    #endif

    // :VS#       Get PEC number of steps per sidereal second of worm rotation
    //            Returns: n.n#
    if (command[1] == 'S' && parameter[0] == 0) {
      sprintF(reply, "%0.6f", stepsPerSiderealSecondAxis1);
      *numericReply = false;
    } else

    // :VW#       Get pec worm rotation steps
    //            Returns: n#
    if (command[1] == 'W' && parameter[0] == 0) {
      long steps = 0;
      #if AXIS1_PEC == ON
        steps = pec.wormRotationSteps;
      #endif
      sprintf(reply, "%06ld", steps);
      *numericReply = false;
    } else return false;
  } else

  // W - PEC Write
  if (command[0] == 'W') {
    #if AXIS1_PEC == ON
      // :WR+#      Move PEC Table ahead by one sidereal second
      //            Return: 0 on failure
      //                    1 on success
      if (command[1] == 'R' && parameter[0] == '+' && parameter[1] == 0) {
        int8_t i = pecBuffer[wormRotationSeconds - 1];
        memmove(&pecBuffer[1], &pecBuffer[0], wormRotationSeconds - 1);
        pecBuffer[0] = i;
      } else

      // :WR-#      Move PEC Table back by one sidereal second
      //            Return: 0 on failure
      //                    1 on success
      if (command[1] == 'R' && parameter[0] == '-' && parameter[1] == 0) {
        int8_t i = pecBuffer[0];
        memmove(&pecBuffer[0], &pecBuffer[1], wormRotationSeconds - 1);
        pecBuffer[wormRotationSeconds - 1] = i;
      } else

      // :WR[n,sn]# Write PEC table entry for worm segment [n] (in sidereal seconds)
      // where [sn] is the correction in steps +/- for this 1 second segment
      //            Returns: Nothing
      if (command[1] == 'R') {
        char *parameter2 = strchr(parameter, ',');
        if (parameter2) {
          int16_t i, j;
          parameter2[0] = 0;
          parameter2++;
          if (convert.atoi2(parameter, &i)) {
            if (i >= 0 && i < pecBufferSize) {
              if (convert.atoi2(parameter2, &j)) {
                if (j >= -128 && j <= 127) {
                  pecBuffer[i] = j;
                  pec.recorded = true;
                } else *commandError = CE_PARAM_RANGE;
              } else *commandError = CE_PARAM_FORM;
            } else *commandError = CE_PARAM_RANGE;
          } else *commandError = CE_PARAM_FORM;
        } else *commandError = CE_PARAM_FORM;
        *numericReply = false;
      } else
    #endif
    return false;
  } else

  // $QZ - PEC Control
  if (command[0] == '$' && command[1] == 'Q' && parameter[0] == 'Z' && parameter[2] == 0) {
    *numericReply = false;
    #if AXIS1_PEC == ON
      // :$QZ+#     Enable RA PEC compensation 
      //            Returns: nothing
      if (parameter[1] == '+') {
        if (pec.state == PEC_NONE && pec.recorded) pec.state = PEC_READY_PLAY; else *commandError = CE_0;
        nv.updateBytes(NV_MOUNT_PEC_BASE, &pec, PecSize);
      } else
      // :$QZ-#     Disable RA PEC Compensation
      //            Returns: nothing
      if (parameter[1] == '-') {
        pec.state = PEC_NONE;
        nv.updateBytes(NV_MOUNT_PEC_BASE, &pec, PecSize);
      } else
      // :$QZ/#     Ready Record PEC
      //            Returns: nothing
      if (parameter[1] == '/') {
        if (pec.state == PEC_NONE && trackingState == TS_SIDEREAL) {
          pec.state = PEC_READY_RECORD;
          nv.updateBytes(NV_MOUNT_PEC_BASE, &pec, PecSize);
        } else *commandError = CE_0;
      } else
      // :$QZZ#     Clear the PEC data buffer
      //            Return: Nothing
      if (parameter[1] == 'Z') {
        for (int i = 0; i < pecBufferSize; i++) pecBuffer[i] = 0;
        pec.state = PEC_NONE;
        pec.recorded = false;
        nv.updateBytes(NV_MOUNT_PEC_BASE, &pec, PecSize);
      } else
      // :$QZ!#     Write PEC data to NV
      //            Returns: nothing
      if (parameter[1] == '!') {
        pec.recorded = true;
        nv.updateBytes(NV_MOUNT_PEC_BASE, &pec, PecSize);
        for (int i = 0; i < pecBufferSize; i++) nv.update(NV_PEC_BUFFER_BASE + i, pecBuffer[i]);
      } else
    #endif
    // :$QZ?#     Get PEC status
    //            Returns: s#, one of "IpPrR" (I)gnore, get ready to (p)lay, (P)laying, get ready to (r)ecord, (R)ecording
    //                         or an optional (.) to indicate an index detect
    if (parameter[1] == '?') {
      const char *pecStateStr = "IpPrR";
      uint8_t state = 0;
      #if AXIS1_PEC == ON
        state = pec.state;
      #endif
      reply[0] = pecStateStr[state]; reply[1] = 0; reply[2] = 0;
      #if AXIS1_PEC == ON
        if (wormIndexSenseThisSecond) reply[1] = '.';
      #endif
    } else { *numericReply = true; return false; }
  } else return false;

  return true;
}

#endif