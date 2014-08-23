
CC = /opt/avrgcc/bin/avr-gcc
LD = /opt/avrgcc/bin/avr-ld
OBJCOPY = /opt/avrgcc/bin/avr-objcopy
SIZE = /opt/avrgcc/bin/avr-size

CFLAGS = -Wall -Os -mmcu=at90s8515 -g
LDFLAGS = -mmcu=at90s8515 -Wall -g

OBJECTS = main.o

PART = at90s8515

all : flash.bin eeprom.hex

flash.hex : main.out
	$(OBJCOPY) -j .text -O ihex $< $@

flash.bin : main.out
	$(OBJCOPY) -j .text -O binary $< $@

eeprom.hex : main.out
	$(OBJCOPY) -j .eeprom -O ihex $< $@

main.out : $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS)
	$(SIZE) $@

main.o : main.c
	$(CC) $(CFLAGS) -c $<

.PHONY : burn
burn   : flash.bin
	./isp -0 -erase flash.bin

.PHONY : clean
clean :
	rm -rf *.hex *.out *.o *.bin
