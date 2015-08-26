PS/2 mouse to C64/128 1351 proportional mouse adapter with AVR
==============================================================

#AFTERWORD
These are my experiments with coding AVR microcontroller to translate PS/2 mouse
signals into proportional mode of CBM 1351 mouse.
I have mostly failed, only digital (joystick emulation) worked flawlessly.

Is this still relevant now with almost all peripherals using USB?

Actually you should visit a project that actually works:
http://svo.2.staticpublic.s3-website-us-east-1.amazonaws.com/%5Bm%5Douse/

Or original PIC version with lots of background 1351 information:
http://www.zimmers.net/anonftp/pub/cbm/documents/projects/interfaces/mouse/Mouse.html

Let's go back to 2003-10-19...

#INTRO:

*WARNING: This is work in progress. Double-check everything. The schematic
may not represent the circuit that software was written for. The only thing
that works 100% now is joystick emulator.*

You may notice that the code is somewhat bloated comparing to what it actually
can do. This is because I wanted to write a flexible system right from the
start.

isp/ is the In-System Programmer, read its code to get the information about
connecting AVR to LPT port.

avr-gcc and GNU make are needed for compiling software


#BASIC USAGE:

If you examine main() routines you'll see that there is a lengthy delay, then
mouse is resetted and its status is read. If the right button is pressed then
joystick emulator is started.
What is wrong here is that one cheap mouse that I own forgets that a button is
pressed if it was pressed all the time during mouse reset. So joystick emulator
is never started. Anyway. this RMB check is not needed until 1351 emulator is
finished and merged to this code. So remove it if you like.



#HARDWARE:

At this time I present only something that was actually tested.

Schematic presents a fully-featured device but it may change in the
future.

To use In-System Programming feature you have to install a switch or
jumper on connection between PB5 and PB6. If you don't care about ISP
then you might want to ground PB4 and not use PB1 at all.

To use Analog Comparator there is a trimm resistor. In my test circuit I
used it to have a ~1V (+/- ~0.3V) voltage on PB3. Measured fixed resistance
would be sth like:

```

         +5V---/\/\/\----+----/\/\/\----GND
                18K     PB3     4K
```
				
Changing system clock from 8MHz to 4MHz would require significant changes
in delay routines.

In ps2-1351/ directory you will find schematic file for Eagle, a PostScript
printout is available in ps2-1351.ps file.
Beware that the schematic is not yet synchronized with the sources. Every
version of software has own small differences. The schematic provides
general information.


#SOFTWARE:

The code is about 1KB now after compiling so a joystick-only emulator will
surely fit on a 90S2313 device. The only thing to do would be to take
PS/2 code from avr-1351/ (as it doesn't use SPI for PS/2) and the rest from
avr-1351-spi/. If required, 16-bit delay routines can be replaced by 8-bit
delay routine taken from avr-potxy/. The only thing you should do is to
redefine joystick lines from PORTA to other pins.

Each of the directories contains a single step towards full
PS/2<->1351 emulator.

Each version contains useful functions that can be moved to
another one.

*WARNING!* Wiring changed a bit between versions.

*WARNING!* Some versions in joystick mode set the bits instead of
         pulling them down. (That's because I put LEDs there at
	 the start, for testing)

So in order:

##avr-1351/
    This one contains has PS/2 DATA wired to PD4 and PS/2 clock
    connected to PD3. SPI is not needed.
    Joystick port is inverted (bit is set when active).
    PS/2 routines: receiving is done in INT1 while sending doesn't
    use interrupts at all.
    Time routines: uses 16-bit Timer. This is overkill, but may be
    useful.

##avr-1351-spi/
    This is the state of the art code for a complete PS/2<->joystick
    emulator. This one is ALMOST compliant with the schematic.
    THE ONLY DIFFERENCE IS THAT PB4 is connected to PB3, not PB1 like
    on the schematic. (this is because analog comparator is not used)
    PS/2 routines: sending and receiving use mix of INT1 and SPI
    interrupts. In this application this is overkill.
    Time routines: again 16-bit Timer. Not really required and could
    be replaced by 8-bit delay routine, just like in code below.

##avr-potxy/
    This is playground for testing accurate routines to correctly send
    bytes over POTX/Y lines. (PD2/PD1 is POTX/Y)
    Time routines: one delay for 8-bit timer (could be used in other
    versions), 16-bit timer with one or two compare registers to control
    whole cycle.
    This code works well when X/Y values are well separated.
    The only missing thing is the origin of interrupt on the start of the
    SID POT-reading cycle. INT0 doesn't work and is forced as int0() function.

##avr-potxy-onetimer/
    Very similar to avr-potxy now. Uses only one compare interrupt,
    Doesn't bring anything new.

##avr-int0/
    Stripped-down test code of the above to examine POTX/Y routines when
    falling edge on INT0 is the source of interrupt. When connected directly
    to POTX it doesn't work.

##avr-ac/
    This is a testcode for using Analog Comparator to detect the start of
    the read cycle from SID.
