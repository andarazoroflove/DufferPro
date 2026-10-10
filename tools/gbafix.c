/*
	DevkitPro GBA ROM fix utility (v1.05) with SuperCard flashcart enhancements
	(Dark Fader / BlackThunder / WinterMute / Diegoisawesome / RetroSimGolf)
	Validates and patches header of GBA ROMs for real hardware and flashcarts.
*/

#pragma pack(1)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define VER		"1.05-sc"
#define ARGV	argv[arg]
#define VALUE	(ARGV+2)
#define NUMBER	strtoul(VALUE, NULL, 0)

typedef struct
{
	uint32_t	start_code;			// B instruction (0xEA00002E -> 0x080000C0)
	uint8_t		logo[0xA0-0x04];	// Authentic Nintendo logo data (156 bytes)
	uint8_t		title[0xC];			// Game title (12 bytes)
	uint32_t	game_code;			// Game code (4 bytes)
	uint16_t	maker_code;			// Maker code (2 bytes)
	uint8_t		fixed;				// 0x96
	uint8_t		unit_code;			// 0x00
	uint8_t		device_type;		// 0x00
	uint8_t		unused[7];			//
	uint8_t		game_version;		// 0x00
	uint8_t		complement;			// 800000A0..800000BC complement checksum
	uint16_t	checksum;			// 0x0000
} Header;

Header header;

const Header good_header =
{
	// start_code: Branch to 0x080000C0 (0xEA00002E)
	0xEA00002E,
	// Authentic Nintendo GBA BIOS Logo Data (156 bytes, XOR checksum = 0xF8CFF8FC)
	{ 0x24,0xFF,0xAE,0x51,0x69,0x9A,0xA2,0x21,0x3D,0x84,0x82,0x0A,0x84,0xE4,0x09,0xAD,
	0x11,0x24,0x8B,0x98,0xC0,0x81,0x7F,0x21,0xA3,0x52,0xBE,0x19,0x93,0x09,0xCE,0x20,
	0x10,0x46,0x4A,0x4A,0xF8,0x27,0x31,0xEC,0x58,0xC7,0xE8,0x33,0x82,0xE3,0xCE,0xBF,
	0x85,0xF4,0xDF,0x94,0xCE,0x4B,0x09,0xC1,0x94,0x56,0x8A,0xC0,0x13,0x72,0xA7,0xFC,
	0x9F,0x84,0x4D,0x73,0xA3,0xCA,0x9A,0x61,0x58,0x97,0xA3,0x27,0xFC,0x03,0x98,0x76,
	0x23,0x1D,0xC7,0x61,0x03,0x04,0xAE,0x56,0xBF,0x38,0x84,0x00,0x40,0xA7,0x0E,0xFD,
	0xFF,0x52,0xFE,0x03,0x6F,0x95,0x30,0xF1,0x97,0xFB,0xC0,0x85,0x60,0xD6,0x80,0x25,
	0xA9,0x63,0xBE,0x03,0x01,0x4E,0x38,0xE2,0xF9,0xA2,0x34,0xFF,0xBB,0x3E,0x03,0x44,
	0x78,0x00,0x90,0xCB,0x88,0x11,0x3A,0x94,0x65,0xC0,0x7C,0x63,0x87,0xF0,0x3C,0xAF,
	0xD6,0x25,0xE4,0x8B,0x38,0x0A,0xAC,0x72,0x21,0xD4,0xF8,0x07 } ,
	// title
	{ 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 },
	// game code
	0x00000000,
	// maker code: '0', '1' (0x3130 little endian)
	0x3130,
	// fixed value
	0x96,
	// unit_code
	0x00,
	// device type
	0x00,
	// unused
	{ 0x00,0x00,0x00,0x00,0x00,0x00,0x00 },
	// game version
	0x00,
	// complement
	0x00,
	// checksum
	0x0000
};

char HeaderComplement(void)
{
	int n;
	char c = 0;
	char *p = (char *)&header + 0xA0;
	for (n = 0; n < 0xBD - 0xA0; n++)
	{
		c += *p++;
	}
	return -(0x19 + c);
}

int main(int argc, char *argv[])
{
	int arg;
	char *argfile = NULL;
	FILE *infile;
	int size, bit;
	int do_pad = 0;

	if (argc <= 1)
	{
		printf("GBA ROM fixer v" VER " (devkitPro + SuperCard compatibility)\n");
		printf("Syntax: gbafix <rom.gba> [-p] [-t[title]] [-c<game_code>] [-m<maker_code>] [-r<version>]\n");
		return -1;
	}

	for (arg = 1; arg < argc; arg++)
	{
		if (ARGV[0] != '-') { argfile = ARGV; break; }
	}

	if (!argfile)
	{
		printf("Filename needed!\n");
		return -1;
	}

	infile = fopen(argfile, "r+b");
	if (!infile) { printf("Error opening input file: %s\n", argfile); return -1; }
	fseek(infile, 0, SEEK_SET);
	if (fread(&header, sizeof(header), 1, infile) != 1)
	{
		printf("Error reading ROM header!\n");
		fclose(infile);
		return -1;
	}

	/* Copy authentic Nintendo logo & hardware constants */
	memcpy(header.logo, good_header.logo, sizeof(header.logo));
	memcpy(&header.fixed, &good_header.fixed, sizeof(header.fixed));
	memcpy(&header.device_type, &good_header.device_type, sizeof(header.device_type));

	for (arg = 1; arg < argc; arg++)
	{
		if (ARGV[0] == '-')
		{
			switch (ARGV[1])
			{
				case 'p':
					do_pad = 1;
					break;

				case 't':
				{
					char title[256];
					memset(title, 0, sizeof(title));
					if (VALUE[0])
					{
						strncpy(title, VALUE, sizeof(header.title));
					}
					else
					{
						char s[256], *begin = s, *t;
						strcpy(s, argfile);
						t = strrchr(s, '\\'); if (t) begin = t + 1;
						t = strrchr(s, '/'); if (t) begin = t + 1;
						t = strrchr(s, '.'); if (t) *t = 0;
						strncpy(title, begin, sizeof(header.title));
					}
					memcpy(header.title, title, sizeof(header.title));
					break;
				}

				case 'c':
				{
					header.game_code = (uint8_t)VALUE[0] | ((uint8_t)VALUE[1] << 8) |
					                   ((uint8_t)VALUE[2] << 16) | ((uint8_t)VALUE[3] << 24);
					break;
				}

				case 'm':
				{
					header.maker_code = (uint8_t)VALUE[0] | ((uint8_t)VALUE[1] << 8);
					break;
				}

				case 'r':
				{
					header.game_version = (uint8_t)NUMBER;
					break;
				}

				default:
					break;
			}
		}
	}

	/* Perform padding if requested: Pad to power of 2, minimum 512KB (524,288 bytes) for SuperCard SDRAM */
	if (do_pad)
	{
		fseek(infile, 0, SEEK_END);
		size = (int)ftell(infile);
		for (bit = 31; bit >= 0; bit--)
		{
			if (size & (1 << bit)) break;
		}
		int target_sz = (size == (1 << bit)) ? size : (1 << (bit + 1));
		if (target_sz < 524288) target_sz = 524288;
		int todo = target_sz - size;
		while (todo-- > 0) fputc(0xFF, infile);
		fseek(infile, 0, SEEK_SET);
	}

	/* Update complement checksum */
	header.complement = 0;
	header.checksum = 0;
	header.complement = HeaderComplement();

	fseek(infile, 0, SEEK_SET);
	fwrite(&header, sizeof(header), 1, infile);
	fclose(infile);

	printf("Successfully patched GBA header for %s (Title: %.12s, Complement: 0x%02X)\n",
	       argfile, (char*)header.title, (uint8_t)header.complement);

	/* Generate matching 64KB .sav file for SuperCard SRAM */
	char sav_path[512];
	strncpy(sav_path, argfile, sizeof(sav_path) - 5);
	sav_path[sizeof(sav_path) - 5] = '\0';
	char *dot = strrchr(sav_path, '.');
	if (dot) strcpy(dot, ".sav");
	else strcat(sav_path, ".sav");

	FILE *fsav = fopen(sav_path, "wb");
	if (fsav)
	{
		uint8_t ff_buf[512];
		memset(ff_buf, 0xFF, sizeof(ff_buf));
		for (int i = 0; i < 65536 / (int)sizeof(ff_buf); ++i)
		{
			fwrite(ff_buf, 1, sizeof(ff_buf), fsav);
		}
		fclose(fsav);
		printf("Generated 64KB SuperCard save file: %s\n", sav_path);
	}

	return 0;
}
