/*
 Simple, STB-style, parser for URL:s as specified by RFC1738 ( http://www.ietf.org/rfc/rfc1738.txt )

 compile with URL_PARSER_IMPLEMENTATION defined for implementation.
 compile with URL_PARSER_IMPLEMENTATION_STATIC defined for static implementation.

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

#if defined(URL_PARSER_IMPLEMENTATION_STATIC)
#  if !defined(URL_PARSER_IMPLEMENTATION)
#    define URL_PARSER_IMPLEMENTATION
#    define URL_PARSER_LINKAGE static
#  endif
#else
#    define URL_PARSER_LINKAGE
#endif

/**
 * Struct describing a parsed url.
 *
 * @example <scheme>://<user>:<pass>@<host>:<port>/<path>
 */
struct parsed_url {
	const char* scheme; ///< scheme part of url or 0x0 if not present.
	const char* user;   ///< user part of url or 0x0 if not present.
	const char* pass;   ///< password part of url or 0x0 if not present.
	const char* host;   ///< host part of url or 0x0 if not present.
	unsigned int port;   ///< port part of url or 0 if not present.
	const char* path;   ///< path part of url or 0x0 if not present.
};

/**
 * Calculate the amount of memory needed to parse the specified url.
 * @param url the url to parse.
 */
URL_PARSER_LINKAGE size_t parse_url_calc_mem_usage(const char* url);

/**
 * Parse an url specified by RFC1738 into its parts.
 *
 * @param url url to parse.
 * @param mem memory-buffer to use to parse the url or NULL to use malloc.
 * @param mem_size size of mem in bytes.
 *
 * @return parsed url. If mem is NULL this value will need to be free:ed with free().
 */
URL_PARSER_LINKAGE parsed_url* parse_url(const char* url, void* mem, size_t mem_size);




#if defined(URL_PARSER_IMPLEMENTATION)
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

struct parse_url_ctx
{
	void* mem;
	size_t memsize;
	size_t memleft;
};

const char* parse_url_strnchr( const char* str, size_t len, int ch )
{
	for( size_t i = 0; i < len; ++i )
		if( str[i] == ch )
			return &str[i];
	return 0x0;
}

static void parse_url_strncpy_lower( char* dst, const char* src, size_t chars )
{
	for( size_t i = 0; i < chars; ++i )
		dst[i] = (char)tolower( src[i] );
	dst[chars] = '\0';
}

static void* parse_url_alloc_mem( parse_url_ctx* ctx, size_t request_size )
{
	if( request_size > ctx->memleft )
		return 0;
	void* res = (char*)ctx->mem + ctx->memsize - ctx->memleft;
	ctx->memleft -= request_size;
	return res;
}

static unsigned int parse_url_default_port_for_scheme( const char* scheme )
{
	if( scheme == 0x0 )
		return 0;

	if( strcmp( scheme, "http"   ) == 0 ) return 80;
	if( strcmp( scheme, "https"  ) == 0 ) return 443;
	if( strcmp( scheme, "ftp"    ) == 0 ) return 21;
	if( strcmp( scheme, "ssh"    ) == 0 ) return 22;
	if( strcmp( scheme, "telnet" ) == 0 ) return 23;
	return 0;
}

static const char* parse_url_parse_scheme( const char* url, parse_url_ctx* ctx, parsed_url* out )
{
	out->scheme = 0x0;

	const char* schemesep = strchr( url, ':' );
	if( schemesep == 0x0 )
		return url;
	else
	{
		// ... is this the user part of a user/pass pair or the separator host:port? ...
		if( schemesep[1] != '/')
			return url;

		if( schemesep[2] != '/' )
			return 0x0;

		out->scheme = (const char*)parse_url_alloc_mem( ctx, (size_t)( schemesep - url + 1 ) );
		if( out->scheme == 0x0 )
			return 0x0;
		parse_url_strncpy_lower( (char*)out->scheme, url, (size_t)( schemesep - url ) );
		return &schemesep[3];
	}
}

static const char* parse_url_parse_user_pass( const char* url, parse_url_ctx* ctx, parsed_url* out )
{
	out->user = 0x0;
	out->pass = 0x0;

	const char* atpos = strchr( url, '@' );
	if( atpos != 0x0 )
	{
		// ... check for a : before the @ ...
		const char* passsep = parse_url_strnchr( url, (size_t)( atpos - url ), ':' );
		if( passsep == 0 )
		{
			out->pass = "";

			out->user = (const char*)parse_url_alloc_mem( ctx, (size_t)( atpos - url + 1 ) );
			if( out->user == 0 )
				return 0;
			parse_url_strncpy_lower( (char*)out->user, url, (size_t)( atpos - url ) );
		}
		else
		{
			size_t userlen = (size_t)(passsep - url);
			size_t passlen = (size_t)(atpos - passsep - 1);
			char* user = (char*)parse_url_alloc_mem( ctx, userlen + 1 );
			char* pass = (char*)parse_url_alloc_mem( ctx, passlen + 1 );
			if( user == 0x0 ) return 0x0;
			if( pass == 0x0 ) return 0x0;

			memcpy( user, url, userlen );
			user[userlen] = '\0';
			memcpy( pass, passsep + 1, passlen );
			pass[passlen] = '\0';

			out->user = user;
			out->pass = pass;
		}

		return atpos + 1;
	}

	return url;
}

static const char* parse_url_parse_host_port( const char* url, parse_url_ctx* ctx, parsed_url* out )
{
	out->host = "localhost";

	const char* portsep = strchr( url, ':' );
	const char* pathsep = 0x0;
	size_t hostlen = 0;

	if( portsep == 0x0 )
	{
		out->port = parse_url_default_port_for_scheme( out->scheme );
		pathsep = strchr( url, '/' );
		hostlen = pathsep == 0x0 ? strlen( url ) : (size_t)( pathsep - url );
	}
	else
	{
		out->port = (unsigned int)atoi( portsep + 1 );
		hostlen = (size_t)( portsep - url );
		pathsep = strchr( portsep, '/' );
	}

	if( hostlen > 0 )
	{
		out->host = (const char*)parse_url_alloc_mem( ctx, hostlen + 1 );
		if( out->host == 0x0 )
			return 0x0;
		parse_url_strncpy_lower( (char*)out->host, url, hostlen );
	}

	// ... parse path ...
	if( pathsep == 0x0 )
	{
		out->path = "/";
	}
	else
	{
		size_t reslen = strlen( pathsep );
		out->path = (const char*)parse_url_alloc_mem( ctx, reslen + 1 );
		if( out->path == 0x0 )
			return 0x0;
		parse_url_strncpy_lower( (char*)out->path, pathsep, reslen );
	}

	return url;
}

#define URL_PARSE_FAIL_IF( x ) if( x ) { if( usermem == 0x0 ) free( mem ); return 0x0; }

URL_PARSER_LINKAGE size_t parse_url_calc_mem_usage( const char* url )
{
	return sizeof( parsed_url ) + strlen( url ) + 5; // 5 == max number of '\0' terminate
}

URL_PARSER_LINKAGE parsed_url* parse_url( const char* url, void* usermem, size_t mem_size )
{
	void* mem = usermem;
	if( mem == 0x0 )
	{
		mem_size = parse_url_calc_mem_usage( url );
		mem = malloc( mem_size );
	}

	parse_url_ctx ctx = {mem, mem_size, mem_size};

	parsed_url* out = (parsed_url*)parse_url_alloc_mem( &ctx, sizeof( parsed_url ) );
	URL_PARSE_FAIL_IF( out == 0x0 );

	url = parse_url_parse_scheme( url, &ctx, out ); URL_PARSE_FAIL_IF( url == 0x0 );
	url = parse_url_parse_user_pass( url, &ctx, out ); URL_PARSE_FAIL_IF( url == 0x0 );
	url = parse_url_parse_host_port( url, &ctx, out ); URL_PARSE_FAIL_IF( url == 0x0 );

	return out;
}
#endif // defined(URL_PARSER_IMPLEMENTATION)

#endif // URL_H_INCLUDED
