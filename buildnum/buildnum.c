
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// LordHavoc: wait for a key press so the window doesn't disappear immediately
#if _DEBUG && WIN32
#define ERROR fprintf(stderr, "press any key\n");getchar();return -1;
#else
#define ERROR return -1;
#endif

// version template:
#define BUILDNUMBER 1

int main(int argc, char **argv)
{
	FILE *file;
	unsigned int insize, outsize, sizedifference, inbuildsize, outbuildsize, writtensize;
	unsigned char *data, *in, *out, *buildstring, *endofbuildstring, outbuildstring[32];
	int inbuildnumber, outbuildnumber, remainder;
	if (argc != 2)
	{
		fprintf(stderr, "usage: buildnum <filename.c or .h>\npurpose: increments build number in version string for darkplaces engine\n");
		ERROR
	}

	file = fopen(argv[1], "rb");
	if (!file)
	{
		fprintf(stderr, "buildnum: unable to open file \"%s\" for reading\n", argv[1]);
		ERROR
	}

	fseek(file, 0, SEEK_END);
	insize = ftell(file);
	data = calloc(1, insize+20);
	fseek(file, 0, SEEK_SET);
	if (fread(data, 1, insize, file) < insize)
	{
		fprintf(stderr, "buildnum: unable to read file \"%s\"\n", argv[1]);
		ERROR
	}
	fclose(file);
	buildstring = strstr(data, "#define BUILDNUMBER ");
	if (!buildstring)
	{
		fprintf(stderr, "buildnum: unable to find \"#define BUILDNUMBER \"\n");
		ERROR
	}
	buildstring += strlen("#define BUILDNUMBER ");
	endofbuildstring = buildstring;
	while (*endofbuildstring && *endofbuildstring != '\r' && *endofbuildstring != '\n')
		endofbuildstring++;
	inbuildnumber = atoi(buildstring);
	outbuildnumber = inbuildnumber + 1;
	printf("incrementing build number %d to %d\n", inbuildnumber, outbuildnumber);
	sprintf(outbuildstring, "%d", outbuildnumber);
	inbuildsize = endofbuildstring - buildstring;
	outbuildsize = strlen(outbuildstring);
	sizedifference = outbuildsize-inbuildsize;
	remainder = (data + insize) - buildstring;
	outsize = insize + sizedifference;
	memmove(buildstring + sizedifference, buildstring, remainder);
	in = outbuildstring;
	out = buildstring;
	while (*in)
		*out++ = *in++;

	file = fopen(argv[1], "wb");
	if (!file)
	{
		fprintf(stderr, "buildnum: unable to open file \"%s\" for writing\n", argv[1]);
		ERROR
	}

	writtensize = fwrite(data, 1, outsize, file);
	fclose(file);
	if (writtensize < outsize)
	{
		fprintf(stderr, "buildnum: error writing file \"%s\", emergency code trying to save to buildnum.dmp\n", argv[1]);
		file = fopen("buildnum.dmp", "wb");
		if (!file)
		{
			fprintf(stderr, "buildnum: unable to open file for writing\n");
			ERROR
		}

		writtensize = fwrite(data, 1, outsize, file);
		fclose(file);
		if (writtensize < outsize)
		{
			fprintf(stderr, "buildnum: error writing emergency dump file!\n");
			ERROR
		}
	}

	return 0;
}

