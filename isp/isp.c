/* 
 * AVR uC ISP
 *
 * by Holger Buss
 * Linux port: Henning Schroeder
 * cleaned up/removed dependency on libuconio: Thomas Strathmann
 *
 */

#include <stdio.h>
#include <string.h>

#include <time.h>
#define delay(WAIT) usleep(WAIT*1000)
#include <unistd.h>
#include <asm/io.h>
#define outport(PORT,BYTE) outb((unsigned char)BYTE,PORT)
#define inport(PORT) inb(PORT)

#define RST 0x00		// 0x10 AT89
#define CLK 0x40
#define SDA 0x20
#define ACK 0x40
#define DELAY {for(wa=0;wa < wait;wa++);}

unsigned int wa;
unsigned int Erase = 0,
    Lock = 0, ReadCode = 0, DataMemory = 0, Brownout = 0;
unsigned int lp = 0x378, lp_name = 0, lp_in = 0x379;
unsigned int wait = 1000;
unsigned int FlashSize, DataSize;


void TestPrommer(void)
{
    char testend, Port, inport_old;
    testend = 0;
    Port = 0;
    while (!testend) {
	outport(lp,Port);
	printf("\n  ********************************************");
	printf("\n  * In System Programmer for ATMEL AVR       *");
	printf("\n  * Testmode for the programming interface   *");
	printf("\n  ********************************************");
	printf("\n   Printer Port   ATMEL");
	printf("\n   Pin:6  (D4)     RESET");
	printf("\n   Pin:7  (D5)     MOSI");
	printf("\n   Pin:8  (D6)     SCK");
	printf("\n   Pin:10 (ACK)    MISO");
	printf("\n   Pin:25 (GND)    GND");
	printf("\n\n   Using lp at %xh (lp%u)", lp, lp_name);
	printf("\n\n  (1) (Output) RESET = ");
	if (Port & 0x10)
	    printf("1 (5 V) ");
	else
	    printf("0 (0 V) ");
	printf("\n  (2) (Output) SCK   = ");
	if (Port & CLK)
	    printf("1 (5 V) ");
	else
	    printf("0 (0 V)");
	printf("\n  (3) (Output) MOSI  = ");
	if (Port & SDA)
	    printf("1 (5 V) ");
	else
	    printf("0 (0 V)");
	printf("\n      (input)  MISO  = ");
	if (inport(lp_in) & ACK)
	    printf("1");
	else
	    printf("0");
	printf("\n\n   press 1, 2 or 3 to change state on output pins");
	printf("\n   press ESC to exit\n");

	//delay(200);
	switch (getchar()) {
	    case 27:
		testend = 1;
		break;
	    case '1':
		Port ^= 0x10;
		break;
	    case '2':
		Port ^= CLK;
		break;
	    case '3':
		Port ^= SDA;
		break;
	}
    }
}

void Out(unsigned char d1)
{
    unsigned char bits = 0x80, i;
    for (i = 0; i < 8; i++) {
	if (d1 & bits)
	    outport(lp, RST | SDA);
	else
	    outport(lp, RST);
	DELAY;
	if (d1 & bits)
	    outport(lp, RST | SDA | CLK);
	else
	    outport(lp, RST | CLK);
	DELAY;
	if (d1 & bits)
	    outport(lp, RST | SDA);
	else
	    outport(lp, RST);
	DELAY;
	bits /= 2;
    }
}

unsigned char in(void)
{
    unsigned char bits = 0x80, i, ret = 0;
    for (i = 0; i < 8; i++) {
	DELAY;
	if (inport(lp_in) & ACK)
	    ret |= bits;
	outport(lp, RST | CLK);
	DELAY;
	outport(lp, RST);
	bits /= 2;
    }
    return (ret);
}

void Sync(unsigned char d1, unsigned char d2, unsigned char d3)
{
    Out(d1);
    Out(d2);
    Out(d3);
}

void SyncAVR(unsigned char d1, unsigned char d2, unsigned char d3,
	     unsigned char d4)
{
    Out(d1);
    Out(d2);
    Out(d3);
    Out(d4);
}

unsigned char SyncRead(unsigned char d1, unsigned char d2)
{
    unsigned char r;
    Out(d1);
    Out(d2);
    r = in();
    return (r);
}

unsigned char SyncReadAVR(unsigned char d1, unsigned char d2,
			  unsigned char d3)
{
    unsigned char r;
    Out(d1);
    Out(d2);
    Out(d3);
    r = in();
    return (r);
}

void InitChip()
{
    printf("\n Reset chip...");
    outport(lp, 0x10);
    delay(50);
    outport(lp, RST);
    delay(50);
    outport(lp, 0x10);		// 00

    delay(50);
    outport(lp, RST);
    delay(500);
    SyncAVR(0xAC, 0x53, 0x00, 0x00);	// programming enable

    delay(100);
    printf(" ok");
}

int Signatur(void)
{
    unsigned int manuf, chip;
    manuf = SyncReadAVR(0x30, 0, 0);
    chip = (SyncReadAVR(0x30, 0, 1) << 8);
    chip |= SyncReadAVR(0x30, 0, 2);
    printf("\n Signature: ");
    FlashSize = 8 * 1024;	// default

    DataSize = 512;		// default

    if (manuf == 0x1E) {
	printf("ATMEL ");
	if (chip == 0x9001) {
	    printf("AT90S1200 (not tested)");
	    DataSize = 64;
	} else if (chip == 0x9101) {
	    printf("AT90S2313");
	    DataSize = 128;
	} else if (chip == 0x9202) {
	    printf("AT90S4434");
	    DataSize = 128;
	} else if (chip == 0x9105) {
	    printf("AT90S2333");
	    DataSize = 128;
	} else if (chip == 0x9203) {
	    printf("AT90S4433");
	    DataSize = 256;
	} else if (chip == 0x9303) {
	    printf("AT90S8535");
	    DataSize = 256;
	} else if (chip == 0x9301) {
	    printf("AT90S8515");
	    DataSize = 512;
	} else
	    printf("ATxxxxx Signature: %x (not tested)", chip);

	if ((chip & 0xff00) == 0x9300) {
	    printf("  8kB Flash");
	    FlashSize = 8192;
	};
	if ((chip & 0xff00) == 0x9200) {
	    printf("  4kB Flash");
	    FlashSize = 4096;
	};
	if ((chip & 0xff00) == 0x9100) {
	    printf("  2kB Flash");
	    FlashSize = 2048;
	};
	if ((chip & 0xff00) == 0x9000) {
	    printf("  1kB Flash");
	    FlashSize = 1024;
	};
	printf
	    ("\n     operates with: %u Bytes Flash and %u Bytes data-memory",
	     FlashSize, DataSize);
    } else {
	if (chip == 0x0102)
	    printf("Read protected");
	else if (manuf == 0xff && chip == 0xffff)
	    printf(" No chip or no signature !!!!!!");
	else if (manuf == 0x00 && chip == 0x0000)
	    printf(" No chip or no signature !!!!!!");
	else
	    printf("Unknown %x : %x", manuf, chip);
    }
    return (chip);
}

long FileSize(FILE * stream)
{
    long curpos, length;
    curpos = ftell(stream);
    fseek(stream, 0L, SEEK_END);
    length = ftell(stream);
    fseek(stream, curpos, SEEK_SET);
    return length;
}

int main(int argc, char *argv[])
{
    unsigned char *prt;
    unsigned int a;
    FILE *in, *out;

    if ((argc > 1) && ((prt = strpbrk(argv[1], "012")) != NULL)) {
	switch (*prt) {
	case '0':
	    lp = 0x378;
	    lp_in = 0x379;
	    lp_name = 0;
	    printf("\n Using lp0 at 378\n");
	    break;
	case '1':
	    lp = 0x278;
	    lp_in = 0x279;
	    lp_name = 1;
	    printf("\n Using lp1 at 278\n");
	    break;
	case '2':
	    lp = 0x3BC;
	    lp_in = 0x3BD;
	    lp_name = 2;
	    printf("\n Using lp2 at 3BC\n");
	    break;
	}
	if (ioperm(lp, 3, 1)) {
	    perror("opening printer port");
	    exit(1);
	}
    } else {
	printf
	    ("usage: %s -lpn [-data] [-erase] [-lock] [-slow] [-read] [-test] [-bo] <file>\n\n",
	     argv[0]);
	exit(255);
    }

    for (a = 2; a < argc; a++) {
	if (strcmp(argv[a], "-erase") == 0) {
	    Erase = 1;
	}
    }

    for (a = 2; a < argc; a++) {
	if (strcmp(argv[a], "-slow") == 0) {
	    wait = 5000;
	    printf("\n Using slow data transfer");
	}
    }
    for (a = 2; a < argc; a++) {
	if (strcmp(argv[a], "-lock") == 0) {
	    Lock = 1;
	}
    }
    for (a = 2; a < argc; a++) {
	if (strcmp(argv[a], "-read") == 0) {
	    ReadCode = 1;
	}
    }
    for (a = 2; a < argc; a++) {
	if (strcmp(argv[a], "-data") == 0) {
	    DataMemory = 1;
	}
    }
    for (a = 2; a < argc; a++) {
	if (strcmp(argv[a], "-bo") == 0) {
	    Brownout = 1;
	}
    }
    for (a = 2; a < argc; a++) {
	if (strcmp(argv[a], "-test") == 0) {
	    printf("vorher\n");
	    TestPrommer();
	    exit(255);
	}
    }

    /*
     *  Write code or data memory
     */
    if (!ReadCode) {
	printf("Open '%s' to download... ", argv[argc - 1]);
	if ((in = fopen(argv[argc - 1], "rb"))
	    == NULL) {
	    printf("\n ERROR: Cannot open input file.\n");
	    fflush(stdin);
	    getchar();
	    return 1;
	} else {
	    unsigned char data, tmp, read, command;
	    unsigned int z = 0, f = 0, adr;
	    unsigned long size = 0;
	    InitChip();
	    Signatur();
	    if (Erase) {
		printf("\n Erase code and data memory...");
		SyncAVR(0xAC, 0x80, 0x00, 0x00);
		delay(500);
		printf(" done");
	    } else
		printf("\n No erasing");
	    if (DataMemory)
		printf("\n Writing data memory...\n");
	    else
		printf("\n Writing code memory...\n");
	    size = (unsigned long) FileSize(in);
	    if (!DataMemory && size >= FlashSize)
		printf("\n WARNinG: more than %i Bytes\n", FlashSize);
	    if (DataMemory && size >= DataSize)
		printf("\n WARNinG: more than %i Bytes\n", FlashSize);
	    while (!feof(in)) {
		data = fgetc(in);
		if (feof(in)) {
		    printf("\n Chip sucessfully programmed.");
		    break;
		};
		tmp = (z >> 5) & 0xf8;
		if (DataMemory)
		    command = 0xC0;	// write data Memory

		else
		    command = 0x40;	// write code Memory

		if (!DataMemory) {
		    adr = z / 2;
		    if (z & 1)	// High-Byte

			SyncAVR(0x48, adr / 0x100, adr % 0x100, data);
		    else
			SyncAVR(0x40, adr / 0x100, adr % 0x100, data);
		    delay(2);
		    if (z & 1)
			read = SyncReadAVR(0x28, adr / 256, adr % 256);
		    else
			read = SyncReadAVR(0x20, adr / 256, adr % 256);
		} else {
		    adr = z;
		    SyncAVR(0xC0, 0, adr % 0x100, data);
		    delay(100);
		    read = SyncReadAVR(0xA0, 0, adr % 256);
		}
		printf("\r  Adress:%04x Write:%02x Read:%02x | ", z, data,
		       read);
		if (DataMemory)
		    printf
			("Volume:%02lu%% of EEPROM | %02lu%% of File (%luByte)",
			 ((long) z * 100) / (DataSize - 1),
			 ((long) (z + 1) * 100) / size, size);
		else
		    printf
			("Volume:%02lu%% of Flash | %02lu%% of File (%luByte)",
			 ((long) z * 100) / (FlashSize - 1),
			 ((long) (z + 1) * 100) / size, size);

		if (read != data) {
		    wait += wait / 10;

		    delay(50);
		    if (z & 1)
			read = SyncReadAVR(0x28, adr / 256, adr % 256);
		    else
			read = SyncReadAVR(0x20, adr / 256, adr % 256);
		    if (read != data) {
			printf("\n\n   !!!!!!!!!!! ERROR !!!!!!!!!!!");
			printf("\n   Memory not written successfully!");
			if ((read < data) && !Erase)
			    printf
				("\n   Chip was not erased.\n   Use /ERASE to clear code and data memory.");
			else
			    printf
				("\n   Check connection, port and power supply.\n   Try slow (-slow) data transfer.");
			break;
		    }
		} else if (wait > 10)
		    wait--;
		z++;
	    }
	    if (Lock) {
		unsigned char i = 0;
		printf("\n Program lock bits...");
		SyncAVR(0xAC, 0xF0, 0, 0);
		delay(50);
		for (i = 0; i < 250; i++)
		    if (SyncReadAVR(0x20, 0, i) != i) {
			printf
			    ("\n ***************************************");
			printf("\n ERROR: Code not locked!");
			printf
			    ("\n ***************************************");
			fflush(stdin);
			getchar();
			break;
		    }
		if (i == 250)
		    printf(" ok. Code locked.");
	    } else
		printf
		    ("\n Warning: Code not protected. Use /LOCK if you want to.");

	    if (Brownout) {
		printf
		    ("\n Program Brownout fuse (4V & Reset 256ms +16k * ck");
		SyncAVR(0xAC, 0xA2, 0x00, 0x00);
	    } else
		printf("\n No Brownout\n");


	    fclose(in);
	}
    }

    /*
     * Read code or data memory
     */
    if (ReadCode) {
	unsigned int z;
	unsigned char tmp, read, erased = 1, locked = 1;
	InitChip();
	Signatur();
	if (DataMemory)
	    printf("\n Read data from chip...");
	else
	    printf("\n Read code from chip...");
	printf("\n   Write code to: '%s'... \n", argv[argc - 1]);
	out = fopen(argv[argc - 1], "wb");
	for (z = 0; z < (FlashSize / 2); z++) {
	    if (DataMemory) {
		if (z == DataSize / 2)
		    break;
		read = SyncReadAVR(0xA0, z / 256, z % 256);
		fputc(read, out);
	    } else {
		read = SyncReadAVR(0x20, z / 256, z % 256);
		fputc(read, out);
		read = SyncReadAVR(0x28, z / 256, z % 256);
		fputc(read, out);
	    }
	    if (read != z % 256)
		locked = 0;
	    if (read != 0xff)
		erased = 0;
	    if (DataMemory)
		printf("\r   Adress:%04x Read:%02x   %02u%% of EEPROM ", z,
		       read, ((long) (z + 1) * 100) / (DataSize / 2));
	    else
		printf("\r   Adress:%04x Read:%02x   %02u%% of Flash ", z,
		       read, ((long) (z + 1) * 100) / (FlashSize / 2));
	}
	if (locked)
	    printf("\n WARNinG: Code was probably locked.");
	if (erased)
	    printf("\n WARNinG: Code is erased or no chip connected.");
	fclose(out);
    }
    delay(100);
    outport(lp, 0);
    delay(100);
    outport(lp, RST);
    delay(100);
    outport(lp, 0x10);		// 00
    if (ioperm(lp_in, 2, 0)) {
	perror("error opening printer port");
	exit(1);
    }
    return 0;
}
