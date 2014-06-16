/*
    Simple http-client written to be an drop-in code, focus is to be small
    and easy to use but maybe not efficient.

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

#include <http_client/http_client.h>
#include <http_client/url.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <netdb.h>

struct http_client
{
	int sockfd;
	parsed_url* url;
	const char* useragent;
};

struct http_request_ctx
{
	char   buffer[2048];
	size_t bytes_in_buffer;
};

static void* http_client_alloc( size_t size, http_client_allocator* alloc )
{
	if( alloc == 0x0 )
		return malloc( size );
	return alloc->alloc( size, alloc );
}

static parsed_url* http_client_parse_url( const char* url, char* mem, size_t memsize )
{
	parsed_url* parsed = parse_url( url, mem, memsize );
	if( parsed != 0x0 )
	{
		if( parsed->scheme != 0x0 )
		{
			if( strcmp( parsed->scheme, "http" ) != 0 )
				return 0x0;
		}

		if( parsed->port == 0 )
			parsed->port = 80;
	}

	return parsed;
}

size_t http_client_calc_mem_usage( const char* url )
{
	return sizeof( http_client ) + parse_url_calc_mem_usage( url );
}

http_client_result http_client_connect( http_client_t* c, const char* url, const char* useragent, void* usermem, size_t memsize )
{
	size_t neededsize = http_client_calc_mem_usage( url );
	void* mem = usermem;
	if( mem == 0x0 )
	{
		mem = malloc( neededsize );
		memsize = neededsize;
	}
	if( memsize < neededsize )
		return HTTP_CLIENT_MEMORY_ALLOC_ERROR;

	http_client* client = (http_client*)mem;
	client->sockfd = -1;
	client->useragent = useragent ? useragent : "http-client";
	client->url = http_client_parse_url( url, (char*)mem + sizeof( http_client ), neededsize - sizeof( http_client ) );
	if( client->url == 0x0 )
	{
		if( usermem == 0x0 )
			free( mem );
		*c = 0x0;
		return HTTP_CLIENT_INVALID_URL;
	}

	addrinfo hints;
	memset( &hints, 0x0, sizeof(hints) );
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	char port[8];
	snprintf( port, 8, "%u", client->url->port );
	struct addrinfo* result;
	int error = getaddrinfo( client->url->host, port, &hints, &result );
	if( error != 0 )
	{
		if( usermem == 0x0 )
			free( mem );
		*c = 0x0;
		return HTTP_CLIENT_SOCKET_ERROR;
	}

	for( addrinfo* res_iter = result; res_iter != NULL; res_iter = res_iter->ai_next )
	{
		client->sockfd = socket( res_iter->ai_family, res_iter->ai_socktype, res_iter->ai_protocol );
		if( client->sockfd < 0 )
			continue;

		if( connect( client->sockfd, res_iter->ai_addr, res_iter->ai_addrlen ) < 0 )
		{
			close( client->sockfd );
			client->sockfd = -1;
			continue;
		}
	}
	freeaddrinfo( result );

	if( client->sockfd < 0 )
	{
		if( usermem == 0x0 )
			free( mem );
		*c = 0x0;
		return HTTP_CLIENT_SOCKET_ERROR;
	}

	*c = client;

    return HTTP_CLIENT_OK;
}

void http_client_disconnect( http_client_t client )
{
    // ... disconnect here ... ;)
	close( client->sockfd );
}

static http_client_result http_client_readline( int sock_fd, http_request_ctx* ctx, size_t* consumed )
{
	while( true )
	{
		if( char* eol = (char*)memmem( ctx->buffer, ctx->bytes_in_buffer, "\r\n", 2 ) )
		{
			*eol = '\0';
			*consumed = (size_t)(eol + 2 - ctx->buffer);
			return HTTP_CLIENT_OK;
		}

		ssize_t bytes_read = recv( sock_fd, ctx->buffer, sizeof( ctx->buffer ), 0 );
		if( bytes_read == 0 )
			return HTTP_CLIENT_CONNECTION_LOST;
		if( bytes_read < 0 && errno != EAGAIN )
			return HTTP_CLIENT_INTERNAL_ERROR;
		else
			ctx->bytes_in_buffer += bytes_read;
	};

	return HTTP_CLIENT_INTERNAL_ERROR;
}

static void http_client_finalize_line( http_request_ctx* ctx, size_t consumed )
{
	ctx->bytes_in_buffer -= consumed;
	memmove( ctx->buffer, &ctx->buffer[consumed], ctx->bytes_in_buffer );
}

static http_client_result http_client_read_status_line( int sockfd, http_request_ctx* ctx, unsigned int* error_code )
{
	unsigned int protocol_major = 0;
	unsigned int protocol_minor = 0;

	size_t consumed = 0;
	http_client_result res = http_client_readline( sockfd, ctx, &consumed );
	if( res != HTTP_CLIENT_OK )
		return res;

	if( sscanf( ctx->buffer, "HTTP/%u.%u %u", &protocol_major, &protocol_minor, error_code ) != 3 )
		return HTTP_CLIENT_SOCKET_ERROR;

	http_client_finalize_line( ctx, consumed );
	return HTTP_CLIENT_OK;
}

static http_client_result http_client_make_request( http_client_t client, http_request_ctx* ctx, const char* verb, const char* resource, size_t* content_length, const void* payload, size_t payload_size )
{
	char request[2048];
	if( payload == 0x0 )
	{
		size_t request_len = snprintf( request, sizeof(request), "%s %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: %s\r\n\r\n", verb, resource, client->url->host, client->useragent );
		if( send( client->sockfd, request, request_len, 0 ) < 0 )
			return HTTP_CLIENT_SOCKET_ERROR;
	}
	else
	{
		size_t request_len = snprintf( request, sizeof(request), "%s %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: %s\r\nContent-Length: %lu\r\n\r\n", verb, resource, client->url->host, client->useragent, payload_size );
		if( send( client->sockfd, request, request_len, 0 ) < 0 )
			return HTTP_CLIENT_SOCKET_ERROR;
		if( send( client->sockfd, payload, payload_size, 0 ) < 0 )
			return HTTP_CLIENT_SOCKET_ERROR;
	}

	http_client_result res;
	unsigned int error_code = 0;
	res = http_client_read_status_line( client->sockfd, ctx, &error_code );
	if( res != HTTP_CLIENT_OK )
		return res;

	if( error_code >= 300 )
		return (http_client_result)error_code;

	for( bool done = false; !done; )
    {
    	size_t consumed = 0;
    	http_client_result res = http_client_readline( client->sockfd, ctx, &consumed );
		if( res != HTTP_CLIENT_OK )
			return res;

		if( ctx->buffer[0] == '\0' )
		{
			done = true;
		}
		else if( strncasecmp( ctx->buffer, "content-length:", 15 ) == 0 )
		{
			if( content_length )
				*content_length = atoi( &ctx->buffer[16] );
		}
		else if( strncasecmp( ctx->buffer, "transfer-encoding:", 18 ) == 0 )
		{
			if( strncasecmp( ctx->buffer + 19, "chunked", 7 ) == 0 )
			{
				// not supported yet =/
				return HTTP_CLIENT_INTERNAL_ERROR;
			}
		}

		http_client_finalize_line( ctx, consumed );
    }

	return HTTP_CLIENT_OK;
}

http_client_result http_client_get( http_client_t client, const char* resource, void** msgbody, size_t* msgbody_size, http_client_allocator* alloc )
{
    *msgbody = 0x0;
    *msgbody_size = 0;

    http_request_ctx ctx;
    ctx.bytes_in_buffer = 0;

    http_client_result res = http_client_make_request( client, &ctx, "GET", resource, msgbody_size, 0, 0 );
    if( res != HTTP_CLIENT_OK )
    	return res;

    char* result_buffer = (char*)http_client_alloc( *msgbody_size, alloc );
    if( result_buffer == 0x0 )
    	return HTTP_CLIENT_MEMORY_ALLOC_ERROR;

    *msgbody = result_buffer;
    memcpy( result_buffer, ctx.buffer, ctx.bytes_in_buffer );

    // TODO: handle error!
    if( *msgbody_size > ctx.bytes_in_buffer )
    	recv( client->sockfd, result_buffer + ctx.bytes_in_buffer, ctx.bytes_in_buffer - *msgbody_size, 0 );

    return HTTP_CLIENT_OK;
}

http_client_result http_client_head( http_client_t client, const char* resource, size_t* msgbody_size )
{
    *msgbody_size = 0;
    http_request_ctx ctx;
    ctx.bytes_in_buffer = 0;
    return http_client_make_request( client, &ctx, "HEAD", resource, msgbody_size, 0, 0 );
}

http_client_result http_client_post( http_client_t client, const char* resource, const void* msgbody, size_t msgbody_size )
{
    http_request_ctx ctx;
    ctx.bytes_in_buffer = 0;
    return http_client_make_request( client, &ctx, "POST", resource, 0x0, msgbody, msgbody_size );
}

http_client_result http_client_put( http_client_t client, const char* resource, const void* msgbody, size_t msgbody_size )
{
    http_request_ctx ctx;
    ctx.bytes_in_buffer = 0;
    return http_client_make_request( client, &ctx, "PUT", resource, 0x0, msgbody, msgbody_size );
}

http_client_result http_client_delete( http_client_t client, const char* resource )
{
    http_request_ctx ctx;
    ctx.bytes_in_buffer = 0;
    return http_client_make_request( client, &ctx, "DELETE", resource, 0x0, 0x0, 0 );
}

const char* http_client_result_to_string( http_client_result result )
{
#define HTTP_RES_TO_STR( res ) case res: return #res
#define HTTP_RES_TO_STR2( res1, res2 ) case res1: return #res1 " or " #res2
	switch( result )
	{
		HTTP_RES_TO_STR( HTTP_CLIENT_OK );
		HTTP_RES_TO_STR( HTTP_CLIENT_INVALID_URL );
		HTTP_RES_TO_STR( HTTP_CLIENT_UNSUPPORTED_SCHEME );
		HTTP_RES_TO_STR( HTTP_CLIENT_SOCKET_ERROR ); // flesh out this.
		HTTP_RES_TO_STR( HTTP_CLIENT_CONNECTION_LOST );
		HTTP_RES_TO_STR( HTTP_CLIENT_INTERNAL_ERROR );

		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_300_MULTIPLE_CHOICES );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_301_MOVED_PERMANENTLY );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_302_FOUND );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_303_SEE_OTHER );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_304_NOT_MODIFIED );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_305_USE_PROXY );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_306_SWITCH_PROXY );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_307_TEMPORARY_REDIRECT );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_308_PERMANENT_REDIRECT );

		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_400_BAD_REQUEST );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_401_UNAUTHORIZED );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_402_PAYMENT_REQUIRED );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_403_FORBIDDEN );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_404_NOT_FOUND );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_405_METHOD_NOT_ALLOWED );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_406_NOT_ACCEPTABLE );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_407_PROXY_AUTHENTICATION_REQUIRED );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_408_REQUEST_TIMEOUT );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_409_CONFLICT );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_410_GONE );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_411_LENGTH_REQUIRED );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_412_PRECONDITION_FAILED );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_413_REQUEST_ENTITY_TOO_LARGE );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_414_REQUEST_URI_TOO_LONG );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_415_UNSUPPORTED_MEDIA_TYPE );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_416_REQUESTED_RANGE_NOT_SATISFIABLE );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_417_EXPECTATION_FAILED );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_418_IM_A_TEAPOT );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_419_AUTHENTICATION_TIMEOUT );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_422_UNPROCESSABLE_ENTITY );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_423_LOCKED );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_424_FAILED_DEPENDENCY );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_425_UNORDERED_COLLECTION );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_426_UPGRADE_REQUIRED );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_428_PRECONDITION_REQUIRED );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_429_TOO_MANY_REQUESTS );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_431_REQUEST_HEADER_FIELDS_TOO_LARGE );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_440_LOGIN_TIMEOUT );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_449_RETRY_WITH );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_450_BLOCKED_BY_WINDOWS_PARENTAL_CONTROLS );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_444_NO_RESPONSE );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_494_REQUEST_HEADER_TOO_LARGE );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_495_CERT_ERROR );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_496_NO_CERT );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_497_HTTP_TO_HTTPS );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_499_CLIENT_CLOSED_REQUEST );

		HTTP_RES_TO_STR2( HTTP_CLIENT_RESULT_420_METHOD_FAILURE, HTTP_CLIENT_RESULT_420_ENHANCE_YOUR_CALM );
		HTTP_RES_TO_STR2( HTTP_CLIENT_RESULT_451_UNAVAILABLE_FOR_LEGAL_REASONS, HTTP_CLIENT_RESULT_451_REDIRECT );

		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_500_INTERNAL_SERVER_ERROR );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_501_NOT_IMPLEMENTED );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_502_BAD_GATEWAY );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_503_SERVICE_UNAVAILABLE );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_504_GATEWAY_TIMEOUT );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_505_HTTP_VERSION_NOT_SUPPORTED );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_506_VARIANT_ALSO_NEGOTIATES );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_507_INSUFFICIENT_STORAGE );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_508_LOOP_DETECTED );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_509_BANDWIDTH_LIMIT_EXCEEDED );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_510_NOT_EXTENDED );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_511_NETWORK_AUTHENTICATION_REQUIRED );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_520_ORIGIN_ERROR );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_521_WEB_SERVER_IS_DOWN );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_522_CONNECTION_TIMED_OUT );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_523_PROXY_DECLINED_REQUEST );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_524_A_TIMEOUT_OCCURRED );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_598_NETWORK_READ_TIMEOUT_ERROR );
		HTTP_RES_TO_STR( HTTP_CLIENT_RESULT_599_NETWORK_CONNECT_TIMEOUT_ERROR );

	default:
		return "Unknown http client result!";
	}
#undef HTTP_RES_TO_STR
#undef HTTP_RES_TO_STR2
}
