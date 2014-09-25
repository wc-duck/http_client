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

#ifndef HTTP_CLIENT_H_INCLUDED
#define HTTP_CLIENT_H_INCLUDED

#include <stdlib.h>
#include <http_client/http_client_error.h>

/**
 * Handle to an open http-client.
 */
typedef struct http_client* http_client_t;

/**
 * Allocator used together with functions that need to allocate dynamic memory.
 *
 * alloc function should work as realloc().
 *
 * @example
 *
 * void* my_alloc( void* ptr, size_t sz, my_allocator* self )
 * {
 *     return some_alloc_func( ptr, sz, self->my_userdata );
 * }
 *
 * struct my_allocator
 * {
 *     http_client_allocator alloc;
 *     int my_userdata;
 * };
 *
 * my_alloc a = { { my_alloc }, 1337 };
 */
struct http_client_allocator
{
	void* (*alloc)( void* ptr, size_t sz, http_client_allocator* self );
};

/**
 * Calculate amount of memory needed to call http_client_connect if memory is allocated by the user.
 *
 * @param url to open.
 */
size_t http_client_calc_mem_usage( const char* url );

/**
 * Open a http-connection to the specified url.
 *
 * @param client ptr to http_client_t to fill.
 * @param url url to connect to.
 * @param useragent user agent to identify as towards the server, can be NULL to use "http-client".
 * @param usermem pointer to memory buffer to use for new http_client_t, can be NULL.
 *                If NULL malloc will be used and the returned client will need a call to free() after
 *                http_client_disconnect.
 *                If non-NULL the size of the buffer must be at least http_client_calc_mem_usage( url )
 *                and be valid until after a call to http_client_disconnect().
 * @param memsize size of usermem.
 *
 * @return HTTP_CLIENT_RESULT_OK on success.
 */
http_client_result http_client_connect( http_client_t* client, const char* url, const char* useragent, void* usermem, size_t memsize );

/**
 * Disconnect a connected client from host.
 *
 * @client client to disconnect.
 */
void http_client_disconnect( http_client_t client );

/**
 * Perform http GET request towards connected host.
 *
 * @param client connected client.
 * @param resource resource on server to GET ( host.com/this/is/the/resource.htm -> /this/is/the/resource.htm )
 * @param msgbody ptr where to return GET message body, if alloc is NULL this will be allocated with malloc, otherwise alloc will be used.
 * @param msgbody_size ptr where to return GET message body size.
 * @param alloc allocator to use to alloc msgbody or NULL to use malloc.
 *
 * @note memory allocated for msgbody will need to be free:ed manually even if an error occured.
 *
 * @return HTTP_CLIENT_RESULT_OK on success.
 */
http_client_result http_client_get( http_client_t client, const char* resource, void** msgbody, size_t* msgbody_size, http_client_allocator* alloc );

/**
 * Perform http HEAD request towards connected host.
 *
 * @param client connected client.
 * @param resource resource on server to HEAD ( host.com/this/is/the/resource.htm -> /this/is/the/resource.htm )
 * @param msgbody_size ptr where to return resource size.
 *
 * @return HTTP_CLIENT_RESULT_OK on success.
 */
http_client_result http_client_head( http_client_t client, const char* resource, size_t* msgbody_size );

/**
 * Perform http POST request towards connected host.
 *
 * @param client connected client.
 * @param resource resource on server to POST ( host.com/this/is/the/resource.htm -> /this/is/the/resource.htm )
 * @param msgbody body of POST-request.
 * @param msgbody_size in bytes of msgbody.
 *
 * @return HTTP_CLIENT_RESULT_OK on success.
 */
http_client_result http_client_post( http_client_t client, const char* resource, const void* msgbody, size_t msgbody_size );

/**
 * Perform http PUT request towards connected host.
 *
 * @param client connected client.
 * @param resource resource on server to PUT ( host.com/this/is/the/resource.htm -> /this/is/the/resource.htm )
 * @param msgbody body of POST-request.
 * @param msgbody_size in bytes of msgbody.
 *
 * @return HTTP_CLIENT_RESULT_OK on success.
 */
http_client_result http_client_put( http_client_t client, const char* resource, const void* msgbody, size_t msgbody_size );

/**
 * Perform http DELETE request towards connected host.
 *
 * @param client connected client.
 * @param resource resource on server to POST ( host.com/this/is/the/resource.htm -> /this/is/the/resource.htm )

 * @return HTTP_CLIENT_RESULT_OK on success.
 */
http_client_result http_client_delete( http_client_t client, const char* resource );

/**
 * Convert http_client_result to string.
 */
const char* http_client_result_to_string( http_client_result result );

#endif // HTTP_CLIENT_H_INCLUDED
