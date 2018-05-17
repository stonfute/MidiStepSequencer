#include <Encoder.h>
#include "FifteenStep.h"
#include "Adafruit_NeoPixel.h"
#include "Wire.h"
#include "Adafruit_MPR121.h"
#include "MIDIUSB.h"


#define NEO_PIN  10
#define LEDS     16
#define TEMPO    60
#define BUTTONS  8
#define IRQ_PIN  4
#define PLAY_PIN  16
#define REC_PIN  14
#define CMD_PIN  15
#define TEMPO_PIN  18
#define LEN_PIN  19

#define PLAY_LED  5
#define REC_LED  6
#define CMD_LED  7
#define TEMPO_LED 8
#define LEN_LED  9



// sequencer, neopixel, & mpr121 init
FifteenStep seq = FifteenStep(1024);
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(LEDS, NEO_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_MPR121 cap = Adafruit_MPR121();
Encoder myEnc(20,21);

// keep track of touched buttons
uint16_t lasttouched = 0;
uint16_t currtouched = 0;

// start sequencer in record mode
bool record_mode = true;
bool play_mode = true;

// set command states to off by default
bool command_mode = false;
bool tempo_mode = false;
bool shuffle_mode = false;
bool pitch_mode = false;
bool velocity_mode = false;
bool channel_mode = false;
bool step_mode = false;

// keep pointers for selected buttons to operate
// on when in note and velocity mode
int  mode_position = 0;
bool position_selected = false;

// prime dynamic values
int channel = 0;
int pitch[] = {36,37,38,39,40,41,42,43};
int vel[] = {127, 127, 127, 127, 127, 127, 127, 127};
int steps = 16;

long oldEncoder = -999;


void setup() {

  // set mpr121 IRQ pin to input
  pinMode(IRQ_PIN, INPUT);

  // Set btn to input
  pinMode(PLAY_PIN, INPUT);
  pinMode(REC_PIN, INPUT);
  pinMode(CMD_PIN, INPUT);
  pinMode(TEMPO_PIN, INPUT);
  pinMode(LEN_PIN, INPUT);

  
  // wait until no fail if the mpr121 init fails
  if (! cap.begin(0x5A))
    while (1);

  // start neopixels
  pixels.begin();
  //pixels.setBrightness(80);

  // start sequencer and set callbacks
  seq.begin(TEMPO, steps);
  seq.setMidiHandler(midi);
  seq.setStepHandler(step);

}

void loop() {

  // if mpr121 irq goes low, there have been
  // changes to the button states, so read the values
  if(digitalRead(IRQ_PIN) == LOW){
        readButtons();
  }
  // this is needed to keep the sequencer
  // running. there are other methods for
  // start, stop, and pausing the steps
  seq.run();

  
  // Get CMD Btn
  getCmdBtn();
  // Display led
  ledBtn();

}

///////////////////////////////////////////////////////////////////////////////
//                                                                           //
//                       COMMAND BTN HANDLERS                                //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////
void getCmdBtn(){
  // TODO : Add debounce
  if(digitalRead(PLAY_PIN) == LOW){
    seq.pause();
    play_mode = play_mode ? false : true;
  }
  if(digitalRead(REC_PIN) == LOW){
    record_mode = record_mode ? false : true;
  }
  if(digitalRead(CMD_PIN) == LOW){
    clear_modes();
    // Reset encoder value to current position
    oldEncoder = myEnc.read();
    command_mode = command_mode ? false : true;
  }
  if(digitalRead(TEMPO_PIN)==LOW){
    // Reset encoder value to current position
    oldEncoder = myEnc.read();
    tempo_mode = true;    
  }
  if(digitalRead(LEN_PIN)==LOW){
    // Reset encoder value to current position
    oldEncoder = myEnc.read();
    position_selected = true;
    step_mode = true;    
  }
  
  if(command_mode){
    handle_command();
  }
  
}

///////////////////////////////////////////////////////////////////////////////
//                                                                           //
//                       BUTTON LED HANDLERS                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////
void ledBtn(){
  digitalWrite(PLAY_LED, play_mode);
  digitalWrite(REC_LED, record_mode);
  digitalWrite(CMD_LED, command_mode);
  digitalWrite(TEMPO_LED, tempo_mode);
  digitalWrite(LEN_LED, step_mode);
}

///////////////////////////////////////////////////////////////////////////////
//                                                                           //
//                       BUTTON PRESS HANDLERS                               //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

// deal with note on and off presses
void handle_note() {

  for (uint8_t i=0; i < BUTTONS; i++) {

    // note on check
    if ((currtouched & _BV(i)) && !(lasttouched & _BV(i))) {

      // play pressed note
      midi(channel, 0x9, pitch[i], vel[i]);

      // if recording, save note on
      if(record_mode)
        seq.setNote(channel, pitch[i], vel[i]);

    }

    // note off check
    if ( !(currtouched & _BV(i)) && (lasttouched & _BV(i)) ) {

      // play note off
      midi(channel, 0x8, pitch[i], 0x0);

      // if recording, save note off
      if(record_mode)
        seq.setNote(channel, pitch[i], 0x0);

    }

  }

}

// allow user to select which command mode to enter
void handle_command() {

  // if we aren't in one of the general
  // command modes, we need to select a command
  if(! mode_selected()) {
    select_mode();
    return;
  }

  // tempo and shuffle are handled below
  if(!tempo_mode && !shuffle_mode) {
    handle_change();
    return;
  }

  long newEncoder = myEnc.read();
  long diff = newEncoder - oldEncoder ;
  // increase tempo or shuffle
  if(currtouched == 0x20 || diff > 0 ) {

    if(tempo_mode)
      seq.increaseTempo();
    else if(shuffle_mode)
      seq.increaseShuffle();

  } else if(currtouched == 0x10  || diff < 0 ) {

    if(tempo_mode)
      seq.decreaseTempo();
    else if(shuffle_mode)
      seq.decreaseShuffle();

  }

  // Set new value to old encoder value
  oldEncoder = newEncoder;

}

// select which mode to enter
void select_mode() {

  // switch pads 1-6
  switch(currtouched) {

    case 0x1:
      if(lasttouched != 0x3)
        tempo_mode = true;
      break;
    case 0x2:
      if(lasttouched != 0x3)
        shuffle_mode = true;
      break;
    case 0x4:
      position_selected = true;
      step_mode = true;
      break;
    case 0x8:
      position_selected = true;
      channel_mode = true;
      break;
    case 0x10:
      pitch_mode = true;
      break;
    case 0x20:
      velocity_mode = true;
      break;

  }

}

// pass the appropriate values to the change value helper
void handle_change() {

  // no button index selected, wait until it has
  // been selected before changing values
  if(! position_selected) {
    get_position();
    return;
  }

  byte rgb[] = {0,0,0};
  int display;

  // set up values to increment or decrement
  if(pitch_mode) {

    int last = pitch[mode_position];
    change_value(pitch[mode_position], 127);
    display = map(pitch[mode_position], 0, 127, 0, LEDS);
    rgb[1] = 64;

    if(last != pitch[mode_position])
      midi(channel, 0x9, pitch[mode_position], vel[mode_position]);

  } else if(velocity_mode) {

    int last = vel[mode_position];
    change_value(vel[mode_position], 127);
    display = map(vel[mode_position], 0, 127, 0, LEDS);
    rgb[2] = 64;

    if(last != vel[mode_position])
      midi(channel, 0x9, pitch[mode_position], vel[mode_position]);

  } else if(channel_mode) {
    change_value(channel, 15);
    display = channel + 1;
    rgb[1] = 32;
    rgb[2] = 32;
  } else if(step_mode) {
    change_value(steps, FS_MAX_STEPS);
    seq.setSteps(steps);
    display = steps;
    rgb[0] = 32;
    rgb[2] = 32;
  }

  if(display % LEDS != 0)
    display = display % LEDS;

  flash(0,0,0);
  show_range(0, display, rgb[0], rgb[1], rgb[2]);

}

// common method for increasing or decreasing values
int change_value(int &current, int max_val) {
  // Get encoder
  long newEncoder = myEnc.read();
  long diff = newEncoder - oldEncoder ;
  // increasing or decreasing value?
  if(currtouched == 0x10 || diff<0)
    current = current > 0 ? current - 1 : 0;
  else if(currtouched == 0x20 || diff>0)
    current = current < max_val ? current + 1 : max_val;

  oldEncoder = newEncoder;
  return current;

}

///////////////////////////////////////////////////////////////////////////////
//                                                                           //
//                         SEQUENCER CALLBACKS                               //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

// called when the step position changes. both the current
// position and last are passed to the callback
void step(int current, int last) {

  // if we are in a command mode, flash command
  if(command_mode) {
    mode_flash(current);
    return;
  }

  note_flash(current);

}

// the callback that will be called by the sequencer when it needs
// to send midi commands.
void midi(byte channel, byte command, byte pitch, byte velocity) {
  // if note ON
  if(command == 0x9) {
    noteOn(channel, pitch, velocity);
  }else{
    noteOff(channel, pitch, velocity);
  }
}

void noteOn(byte channel, byte pitch, byte velocity) {
        midiEventPacket_t noteOn = {0x09, 0x90 | channel, pitch, velocity};
        MidiUSB.sendMIDI(noteOn);
        MidiUSB.flush();
}
void noteOff(byte channel, byte pitch, byte velocity) {
        midiEventPacket_t noteOff = {0x08, 0x80 | channel, pitch, velocity};
        MidiUSB.sendMIDI(noteOff);
        MidiUSB.flush();
}

///////////////////////////////////////////////////////////////////////////////
//                                                                           //
//                          NEOPIXEL HELPERS                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

// handles flashing for the different mode states
void mode_flash(int current) {

  // bail if we don't need to flash
  if(position_selected)
    return;

  byte rgb[] = {0,0,0};

  // set colors for modes
  if(pitch_mode) {
    rgb[1] = 64;
  } else if(velocity_mode) {
    rgb[2] = 64;
  } else if(tempo_mode) {
    rgb[0] = 32;
    rgb[1] = 32;
    rgb[2] = 32;
  } else if(shuffle_mode) {
    rgb[0] = 32;
    rgb[1] = 32;
  } else {
    rgb[0] = 64;
  }

  // LEDs on when it's an even step
  if((current % 2) == 0)
    flash(rgb[0],rgb[1],rgb[2]);
  else
    flash(0,0,0);

}

void note_flash(int current) {

  byte rgb[] = {0,0,0};

  // all LEDs off
  flash(0, 0, 0);

  // make sure we stay within the led count
  current = current % LEDS;

  // highlight quarter notes
  if(current % 4 == 0) {
    // red quarter notes in record mode
    // bright blue in play mode
    if(record_mode)
      rgb[0] = 255;
    else
      rgb[2] = 255;
  } else {
    // dim blue sixteenths
    rgb[2] = 64;
  }

  // highlight note
  show_range(current, current + 1, rgb[0], rgb[1], rgb[2]);

}

// sets all pixels to passed RGB values
void flash(byte r, byte g, byte b) {
  show_range(0, LEDS, r, g, b);
}

// sets range of pixels to RGB values
void show_range(int start, int last, byte r, byte g, byte b) {

  for (; start < last; start++)
    pixels.setPixelColor(start, pixels.Color(r,g,b));

  pixels.show();

}

///////////////////////////////////////////////////////////////////////////////
//                                                                           //
//                      GENERAL UTILITY FUNCTIONS                            //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

// read the currently touched buttons and detect any
// pressed patterns that match command states. this
// set of states is set up for 6 pads, but you could
// easily modify the hex values to match the ammount
// pads you are using
void readButtons() {

  // read current values
  currtouched = cap.touched();

  switch(currtouched) {

    case 0x21: // stop all notes. pads 1,6
      seq.panic();
      break;

    case 0x3: // command mode. pads 1,2
      clear_modes();
      command_mode = command_mode ? false : true;
      break;

    case 0x18: // toggle record mode. pads 4,5
      record_mode = record_mode ? false : true;
      break;

    case 0x30: // pause toggle. pads 5,6
      seq.pause();
      break;

    default:
      // if it's not a command pattern, call route
      // it to the proper handler
      if(command_mode)
        handle_command();
      else
        handle_note();

  }

  // save current values to compare against in the next loop
  lasttouched = currtouched;

}

// get the index of the pressed button. this is used by
// the velocity and note modes to tell which button to change
void get_position() {

  for(int i=0; i < BUTTONS; i++) {
    if ( (currtouched & _BV(i)) && !(lasttouched & _BV(i)) ) {
      mode_position = i;
      position_selected = true;
    }
  }

}

// resets mode display to off
void clear_modes() {

  // turn off all modes
  channel_mode = false;
  velocity_mode = false;
  pitch_mode = false;
  step_mode = false;
  tempo_mode = false;
  shuffle_mode = false;

  // reset mode position
  mode_position = 0;
  position_selected = false;

  // clear pixels
  flash(0,0,0);

}

// check if we have selected a command mode
bool mode_selected() {

  if(! command_mode)
    return false;

  if(channel_mode || velocity_mode || pitch_mode || step_mode || tempo_mode || shuffle_mode)
    return true;

  return false;

}
