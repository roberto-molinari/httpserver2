// httpserver.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <http.h>

int main(int argc, char** argv)
{
	ULONG           retCode;
	HANDLE          hReqQueue = NULL;
	int             UrlAdded = 0;
	HTTPAPI_VERSION HttpApiVersion = HTTPAPI_VERSION_1;


	if (argc < 2)
	{
		::printf("%s: <Url1> [Url2] ... \n", argv[0]);
		return -1;
	}

	//
	// Initialize HTTP Server APIs
	//
	retCode = ::HttpInitialize(
		HttpApiVersion,
		HTTP_INITIALIZE_SERVER,    // Flags
		NULL                       // Reserved
		);

	if (retCode != NO_ERROR)
	{
		wprintf(L"HttpInitialize failed with %lu \n", retCode);
		return retCode;
	}

	// 
	// Call HttpTerminate.
	//
	::HttpTerminate(HTTP_INITIALIZE_SERVER, NULL);

	return 0;
}

