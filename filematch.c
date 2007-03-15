
#include "quakedef.h"

// LordHavoc: some portable directory listing code I wrote for lmp2pcx, now used in darkplaces to load id1/*.pak and such...

int matchpattern(const char *in, const char *pattern, int caseinsensitive)
{
	int c1, c2;
	while (*pattern)
	{
		switch (*pattern)
		{
		case 0:
			return 1; // end of pattern
		case '?': // match any single character
			if (*in == 0 || *in == '/' || *in == '\\' || *in == ':')
				return 0; // no match
			in++;
			pattern++;
			break;
		case '*': // match anything until following string
			if (!*in)
				return 1; // match
			pattern++;
			while (*in)
			{
				if (*in == '/' || *in == '\\' || *in == ':')
					break;
				// see if pattern matches at this offset
				if (matchpattern(in, pattern, caseinsensitive))
					return 1;
				// nope, advance to next offset
				in++;
			}
			break;
		default:
			if (*in != *pattern)
			{
				if (!caseinsensitive)
					return 0; // no match
				c1 = *in;
				if (c1 >= 'A' && c1 <= 'Z')
					c1 += 'a' - 'A';
				c2 = *pattern;
				if (c2 >= 'A' && c2 <= 'Z')
					c2 += 'a' - 'A';
				if (c1 != c2)
					return 0; // no match
			}
			in++;
			pattern++;
			break;
		}
	}
	if (*in)
		return 0; // reached end of pattern but not end of input
	return 1; // success
}

// a little strings system
void stringlistinit(stringlist_t *list)
{
	memset(list, 0, sizeof(*list));
}

void stringlistfreecontents(stringlist_t *list)
{
	int i;
	for (i = 0;i < list->numstrings;i++)
	{
		if (list->strings[i])
			Z_Free(list->strings[i]);
		list->strings[i] = NULL;
	}
	list->numstrings = 0;
	list->maxstrings = 0;
	if (list->strings)
		Z_Free(list->strings);
}

void stringlistappend(stringlist_t *list, char *text)
{
	size_t textlen;
	char **oldstrings;

	if (list->numstrings >= list->maxstrings)
	{
		oldstrings = list->strings;
		list->maxstrings += 4096;
		list->strings = Z_Malloc(list->maxstrings * sizeof(*list->strings));
		if (list->numstrings)
			memcpy(list->strings, oldstrings, list->numstrings * sizeof(*list->strings));
		if (oldstrings)
			Z_Free(oldstrings);
	}
	textlen = strlen(text) + 1;
	list->strings[list->numstrings] = Z_Malloc(textlen);
	memcpy(list->strings[list->numstrings], text, textlen);
	list->numstrings++;
}

void stringlistsort(stringlist_t *list)
{
	int i, j;
	char *temp;
	// this is a selection sort (finds the best entry for each slot)
	for (i = 0;i < list->numstrings - 1;i++)
	{
		for (j = i + 1;j < list->numstrings;j++)
		{
			if (strcmp(list->strings[i], list->strings[j]) > 0)
			{
				temp = list->strings[i];
				list->strings[i] = list->strings[j];
				list->strings[j] = temp;
			}
		}
	}
}

// operating system specific code
#ifdef WIN32
#include <io.h>
void listdirectory(stringlist_t *list, const char *path)
{
	int i;
	char pattern[4096], *c;
	struct _finddata_t n_file;
	long hFile;
	strlcpy (pattern, path, sizeof (pattern));
	strlcat (pattern, "*", sizeof (pattern));
	// ask for the directory listing handle
	hFile = _findfirst(pattern, &n_file);
	if(hFile == -1)
		return NULL;
	// start a new chain with the the first name
	stringlistappend(list, n_file.name);
	// iterate through the directory
	while (_findnext(hFile, &n_file) == 0)
		stringlistappend(list, n_file.name);
	_findclose(hFile);

	// convert names to lowercase because windows does not care, but pattern matching code often does
	for (i = 0;i < list->numstrings;i++)
		for (c = list->strings[i];*c;c++)
			if (*c >= 'A' && *c <= 'Z')
				*c += 'a' - 'A';
}
#else
#include <dirent.h>
void listdirectory(stringlist_t *list, const char *path)
{
	DIR *dir;
	struct dirent *ent;
	dir = opendir(path);
	if (!dir)
		return;
	while ((ent = readdir(dir)))
		if (strcmp(ent->d_name, ".") && strcmp(ent->d_name, ".."))
			stringlistappend(list, ent->d_name);
	closedir(dir);
}
#endif

