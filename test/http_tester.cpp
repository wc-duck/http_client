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

#include <stdio.h>
#include <string.h>

#if defined( _MSC_VER )
#  include <WinSock2.h>
#endif

#define HTTP_ERR_CHECK( err ) if( err != HTTP_CLIENT_OK ) \
	{ \
		free( c ); \
		fprintf( stderr, "%s\n", http_client_result_to_string( res ) ); \
		return 1; \
	}

int main( int argc, char** argv )
{
	if( argc < 4 )
	{
		printf("usage: http_tester [GET,SET,PUT] url path\n");
		return 1;
	}

#if defined( _MSC_VER )
	// Initialize Winsock
	WSADATA wsaData;
	int iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
	if (iResult != 0)
	{
		printf("WSAStartup failed: %d\n", iResult);
		return 1;
	}
#endif // defined( _MSC_VER )

    http_client_t c;
    http_client_result res;

    res = http_client_connect( &c, argv[2], 0, 0, 0 );
    HTTP_ERR_CHECK( res );

    if( strcmp( argv[1], "GET" ) == 0 )
    {
		char*  msgbody;
		size_t msgbody_size;
		res = http_client_get( c, argv[3], (void**)&msgbody, &msgbody_size, 0x0 );
		HTTP_ERR_CHECK( res );

		fprintf( stdout, "%.*s\n", (int)msgbody_size, msgbody );
		free( msgbody );
    }
    else if( strcmp( argv[1], "HEAD" ) == 0 )
    {
		size_t msgbody_size;
		res = http_client_head( c, argv[3], &msgbody_size);
		HTTP_ERR_CHECK( res );

		printf( "content size: %lu\n", msgbody_size );
    }

    else if( strcmp( argv[1], "PUT" ) == 0 )
	{
		res = http_client_put( c, argv[3], "apansson", 9 );
		HTTP_ERR_CHECK( res );
	}

    http_client_disconnect( c );
    free( c );

    return 0;
}
