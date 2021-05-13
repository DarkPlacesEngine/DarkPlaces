/*
Copyright (C) 2000-2021 Ashley Rose Hale (LadyHavoc)
Copyright (C) 2021 David Knapp (Cloudwalk)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "darkplaces.h"
#include "token.h"

// Advance forward in the stream as many times as 'count', cleanly.
const char *Token_Next(const char *data, int count)
{
	const char *out = data;
	for (int i = 0; i < count; i++)
	{
		if (!*out)
			break;
		out++;
	}

	return out;
}

// Skips newlines, and handles different line endings.
qbool Token_Newline(const char **data)
{
	if(**data == '\n')
		goto newline;
	if(**data == '\r')
	{
		if(**data + 1 == '\n')
			(*data)++;
		goto newline;
	}
	return false;
newline:
	(*data)++;
	return true;
}
