/*
    Simple parser for URL:s as specified by RFC1738 ( http://www.ietf.org/rfc/rfc1738.txt )

    version 1.0, June, 2014

	Copyright (C) 2014- Fredrik Kihlander

	This software is provided 'as-is', without any express or implied
	warranty.  In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	1. The origin of this software must not be misrepresented; you must not
	   claim that you wrote the original software. If you use this software
	   in a product, an acknowledgment in the product documentation would be
	   appreciated but is not required.
	2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.
	3. This notice may not be removed or altered from any source distribution.

	Fredrik Kihlander
*/

#ifndef URL_H_INCLUDED
#define URL_H_INCLUDED

#include <stdlib.h>

/**
 * Struct describing a parsed url.
 *
 * @example <scheme>://<user>:<pass>@<host>:<port>/<path>
 */
struct parsed_url
{
	const char*  scheme; ///< scheme part of url or 0x0 if not present.
	const char*  user;   ///< user part of url or 0x0 if not present.
	const char*  pass;   ///< password part of url or 0x0 if not present.
	const char*  host;   ///< host part of url or 0x0 if not present.
	unsigned int port;   ///< port part of url or 0 if not present.
	const char*  path;   ///< path part of url or 0x0 if not present.
};

/**
 * Calculate the amount of memory needed to parse the specified url.
 * @param url the url to parse.
 */
size_t parse_url_calc_mem_usage( const char* url );

/**
 * Parse an url specified by RFC1738 into its parts.
 *
 * @param url url to parse.
 * @param mem memory-buffer to use to parse the url or NULL to use malloc.
 * @param mem_size size of mem in bytes.
 *
 * @return parsed url. If mem is NULL this value will need to be free:ed with free().
 */
parsed_url* parse_url( const char* url, void* mem, size_t mem_size );

#endif // URL_H_INCLUDED
