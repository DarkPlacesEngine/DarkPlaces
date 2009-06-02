
#include "quakedef.h"

// LordHavoc: some portable directory listing code I wrote for lmp2pcx, now used in darkplaces to load id1/*.pak and such...

int matchpattern(const char *in, const char *pattern, int caseinsensitive)
{
	return matchpattern_with_separator(in, pattern, caseinsensitive, "/\\:", false);
}

int matchpattern_with_separator(const char *in, const char *pattern, int caseinsensitive, const char *separators, qboolean wildcard_least_one)
{
	int c1, c2;
	while (*pattern)
	{
		switch (*pattern)
		{
		case 0:
			return 1; // end of pattern
		case '?': // match any single character
			if (*in == 0 || strchr(separators, *in))
				return 0; // no match
			in++;
			pattern++;
			break;
		case '*': // match anything until following string
			if(wildcard_least_one)
				if (*in == 0 || strchr(separators, *in))
					return 0; // no match
			if (!*in)
				return 1; // match
			pattern++;
			while (*in)
			{
				if (strchr(separators, *in))
					break;
				// see if pattern matches at this offset
				if (matchpattern_with_separator(in, pattern, caseinsensitive, separators, wildcard_least_one))
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
	list->strings = NULL;
}

void stringlistappend(stringlist_t *list, const char *text)
{
	size_t textlen;
	char **oldstrings;

	if (list->numstrings >= list->maxstrings)
	{
		oldstrings = list->strings;
		list->maxstrings += 4096;
		list->strings = (char **) Z_Malloc(list->maxstrings * sizeof(*list->strings));
		if (list->numstrings)
			memcpy(list->strings, oldstrings, list->numstrings * sizeof(*list->strings));
		if (oldstrings)
			Z_Free(oldstrings);
	}
	textlen = strlen(text) + 1;
	list->strings[list->numstrings] = (char *) Z_Malloc(textlen);
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
			if (strcasecmp(list->strings[i], list->strings[j]) > 0)
			{
				temp = list->strings[i];
				list->strings[i] = list->strings[j];
				list->strings[j] = temp;
			}
		}
	}
}

// operating system specific code
static void adddirentry(stringlist_t *list, const char *path, const char *name)
{
	if (strcmp(name, ".") && strcmp(name, ".."))
	{
		char temp[MAX_OSPATH];
		dpsnprintf( temp, sizeof( temp ), "%s%s", path, name );
		stringlistappend(list, temp);
	}
}
#ifdef WIN32
#include <windows.h>
void listdirectory(stringlist_t *list, const char *basepath, const char *path)
{
	int i;
	char pattern[4096], *c;
	WIN32_FIND_DATA n_file;
	HANDLE hFile;
	strlcpy (pattern, basepath, sizeof(pattern));
	strlcat (pattern, path, sizeof (pattern));
	strlcat (pattern, "*", sizeof (pattern));
	// ask for the directory listing handle
	hFile = FindFirstFile(pattern, &n_file);
	if(hFile == INVALID_HANDLE_VALUE)
		return;
	do {
		adddirentry(list, path, n_file.cFileName);
	} while (FindNextFile(hFile, &n_file) != 0);
	FindClose(hFile);

	// convert names to lowercase because windows does not care, but pattern matching code often does
	for (i = 0;i < list->numstrings;i++)
		for (c = list->strings[i];*c;c++)
			if (*c >= 'A' && *c <= 'Z')
				*c += 'a' - 'A';
}
#else
#include <dirent.h>
void listdirectory(stringlist_t *list, const char *basepath, const char *path)
{
	char fullpath[MAX_OSPATH];
	DIR *dir;
	struct dirent *ent;
	dpsnprintf(fullpath, sizeof(fullpath), "%s%s", basepath, *path ? path : "./");
	dir = opendir(fullpath);
	if (!dir)
		return;
	while ((ent = readdir(dir)))
		adddirentry(list, path, ent->d_name);
	closedir(dir);
}
#endif

