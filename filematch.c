
#include "quakedef.h"

// LordHavoc: some portable directory listing code I wrote for lmp2pcx, now used in darkplaces to load id1/*.pak and such...

int matchpattern(char *in, char *pattern, int caseinsensitive)
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

// a little chained strings system
stringlist_t *stringlistappend(stringlist_t *current, char *text)
{
	stringlist_t *newitem;
	newitem = Z_Malloc(strlen(text) + 1 + sizeof(stringlist_t));
	newitem->next = NULL;
	newitem->text = (char *)(newitem + 1);
	strcpy(newitem->text, text);
	if (current)
		current->next = newitem;
	return newitem;
}

void stringlistfree(stringlist_t *current)
{
	stringlist_t *next;
	while (current)
	{
		next = current->next;
		Z_Free(current);
		current = next;
	}
}

stringlist_t *stringlistsort(stringlist_t *start)
{
	int notdone;
	stringlist_t *current, *previous, *temp2, *temp3, *temp4;
	// exit early if there's nothing to sort
	if (start == NULL || start->next == NULL)
		return start;
	notdone = 1;
	while (notdone)
	{
		current = start;
		notdone = 0;
		previous = NULL;
		while (current && current->next)
		{
			if (strcmp(current->text, current->next->text) > 0)
			{
				// current is greater than next
				notdone = 1;
				temp2 = current->next;
				temp3 = current;
				temp4 = current->next->next;
				if (previous)
					previous->next = temp2;
				else
					start = temp2;
				temp2->next = temp3;
				temp3->next = temp4;
				break;
			}
			previous = current;
			current = current->next;
		}
	}
	return start;
}

// operating system specific code
#ifdef WIN32
#include <io.h>
stringlist_t *listdirectory(const char *path)
{
	char pattern[4096], *c;
	struct _finddata_t n_file;
	long hFile;
	stringlist_t *start, *current;
	strlcpy (pattern, path, sizeof (pattern));
	strlcat (pattern, "*", sizeof (pattern));
	// ask for the directory listing handle
	hFile = _findfirst(pattern, &n_file);
	if(hFile == -1)
		return NULL;
	// start a new chain with the the first name
	start = current = stringlistappend(NULL, n_file.name);
	// iterate through the directory
	while (_findnext(hFile, &n_file) == 0)
		current = stringlistappend(current, n_file.name);
	_findclose(hFile);

	// convert names to lowercase because windows does not care, but pattern matching code often does
	for (current = start;current;current = current->next)
		for (c = current->text;*c;c++)
			if (*c >= 'A' && *c <= 'Z')
				*c += 'a' - 'A';

	// sort the list alphanumerically
	return stringlistsort(start);
}
#else
#include <dirent.h>
stringlist_t *listdirectory(const char *path)
{
	DIR *dir;
	struct dirent *ent;
	stringlist_t *start, *current;
	dir = opendir(path);
	if (!dir)
		return NULL;
	start = current = NULL;
	while ((ent = readdir(dir)))
	{
		if (strcmp(ent->d_name, ".") && strcmp(ent->d_name, ".."))
		{
			current = stringlistappend(current, ent->d_name);
			if (!start)
				start = current;
		}
	}
	closedir(dir);
	// sort the list alphanumerically
	return stringlistsort(start);
}
#endif

void freedirectory(stringlist_t *list)
{
	stringlistfree(list);
}

