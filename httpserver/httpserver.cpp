// httpserver.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <http.h>

//
// Macros.
//
#define INITIALIZE_HTTP_RESPONSE( resp, status, reason )    \
    do                                                      \
    {                                                       \
        RtlZeroMemory( (resp), sizeof(*(resp)) );           \
        (resp)->StatusCode = (status);                      \
        (resp)->pReason = (reason);                         \
        (resp)->ReasonLength = (USHORT) strlen(reason);     \
    } while (FALSE)

#define ADD_KNOWN_HEADER(Response, HeaderId, RawValue)               \
    do                                                               \
    {                                                                \
        (Response).Headers.KnownHeaders[(HeaderId)].pRawValue =      \
                                                          (RawValue);\
        (Response).Headers.KnownHeaders[(HeaderId)].RawValueLength = \
            (USHORT) strlen(RawValue);                               \
    } while(FALSE)

#define ALLOC_MEM(cb) HeapAlloc(GetProcessHeap(), 0, (cb))

#define FREE_MEM(ptr) HeapFree(GetProcessHeap(), 0, (ptr))

ULONG DoReceiveRequests(HANDLE hReqQueue);
DWORD SendHttpResponse(
	IN HANDLE        hReqQueue,
	IN PHTTP_REQUEST pRequest,
	IN USHORT        StatusCode,
	IN PSTR          pReason,
	IN PSTR          pEntityString
	);

int main(int argc, char** argv)
{
	ULONG           retCode;
	HANDLE          hReqQueue = NULL;
	int             UrlAdded = 0;
	HTTPAPI_VERSION HttpApiVersion = HTTPAPI_VERSION_1;


	//if (argc < 2)
	//{
	//	::printf("%s: <Url1> [Url2] ... \n", argv[0]);
	//	return -1;
	//}

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
	// Create the HTTP request queue
	//
	retCode = ::HttpCreateHttpHandle(&hReqQueue, 0);

	if (retCode != NO_ERROR)
	{
		::printf("HttpCreateHttpHandle faield with %lu \n", retCode);
		return retCode;
	}

	// 
	// Add the URL to listen for
	//
	retCode = ::HttpAddUrl(hReqQueue, L"http://localhost:80/", NULL);
	if (retCode != NO_ERROR)
	{
		::printf("HttpAddUrl failed with %lu \n", retCode);
		return retCode;
	}

	// receive requests
	DoReceiveRequests(hReqQueue);


	//
	// Let HTTP.SYS know we're no longer listening for a URL
	//
	retCode = ::HttpRemoveUrl(hReqQueue, L"http://+:80");
	if (retCode != NO_ERROR)
	{
		::printf("HttpRemoveUrl failed with %lu \n", retCode);
		return retCode;
	}

	// 
	// Dispose the HTTP request queue
	//
	::CloseHandle(hReqQueue);

	// 
	// Call HttpTerminate.
	//
	::HttpTerminate(HTTP_INITIALIZE_SERVER, NULL);

	return 0;
}

ULONG DoReceiveRequests(HANDLE hReqQueue)
{
	ULONG              result;
	HTTP_REQUEST_ID    requestId;
	DWORD              bytesRead;
	PHTTP_REQUEST      pRequest;
	PCHAR              pRequestBuffer;
	ULONG              RequestBufferLength;

	//
	// Allocate a 2 KB buffer. This size should work for most 
	// requests. The buffer size can be increased if required. Space
	// is also required for an HTTP_REQUEST structure.
	//
	RequestBufferLength = sizeof(HTTP_REQUEST) + 2048;
	pRequestBuffer = (PCHAR)ALLOC_MEM(RequestBufferLength);

	if (pRequestBuffer == NULL)
	{
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	pRequest = (PHTTP_REQUEST)pRequestBuffer;

	//
	// Wait for a new request. This is indicated by a NULL 
	// request ID.
	//

	HTTP_SET_NULL_ID(&requestId);

	for (;;)
	{
		RtlZeroMemory(pRequest, RequestBufferLength);

		result = HttpReceiveHttpRequest(
			hReqQueue,          // Req Queue
			requestId,          // Req ID
			0,                  // Flags
			pRequest,           // HTTP request buffer
			RequestBufferLength,// req buffer length
			&bytesRead,         // bytes received
			NULL                // LPOVERLAPPED
			);

		if (NO_ERROR == result)
		{
			//
			// Worked! 
			// 
			switch (pRequest->Verb)
			{
			case HttpVerbGET:
				wprintf(L"Got a GET request for %ws \n",
					pRequest->CookedUrl.pFullUrl);

				result = SendHttpResponse(
					hReqQueue,
					pRequest,
					200,
					"OK",
					"Hey! You hit the server \r\n"
					);
				break;

			case HttpVerbPOST:

				wprintf(L"Got a POST request for %ws \n",
					pRequest->CookedUrl.pFullUrl);

				result = 0;
				//result = SendHttpPostResponse(hReqQueue, pRequest);
				break;

			default:
				wprintf(L"Got a unknown request for %ws \n",
					pRequest->CookedUrl.pFullUrl);

				result = SendHttpResponse(
					hReqQueue,
					pRequest,
					503,
					"Not Implemented",
					NULL
					);
				break;
			}

			if (result != NO_ERROR)
			{
				break;
			}

			//
			// Reset the Request ID to handle the next request.
			//
			HTTP_SET_NULL_ID(&requestId);
		}
		else if (result == ERROR_MORE_DATA)
		{
			//
			// The input buffer was too small to hold the request
			// headers. Increase the buffer size and call the 
			// API again. 
			//
			// When calling the API again, handle the request
			// that failed by passing a RequestID.
			//
			// This RequestID is read from the old buffer.
			//
			requestId = pRequest->RequestId;

			//
			// Free the old buffer and allocate a new buffer.
			//
			RequestBufferLength = bytesRead;
			FREE_MEM(pRequestBuffer);
			pRequestBuffer = (PCHAR)ALLOC_MEM(RequestBufferLength);

			if (pRequestBuffer == NULL)
			{
				result = ERROR_NOT_ENOUGH_MEMORY;
				break;
			}

			pRequest = (PHTTP_REQUEST)pRequestBuffer;

		}
		else if (ERROR_CONNECTION_INVALID == result &&
			!HTTP_IS_NULL_ID(&requestId))
		{
			// The TCP connection was corrupted by the peer when
			// attempting to handle a request with more buffer. 
			// Continue to the next request.

			HTTP_SET_NULL_ID(&requestId);
		}
		else
		{
			break;
		}

	}

	if (pRequestBuffer)
	{
		FREE_MEM(pRequestBuffer);
	}

	return result;
}



DWORD SendHttpResponse(
	IN HANDLE        hReqQueue,
	IN PHTTP_REQUEST pRequest,
	IN USHORT        StatusCode,
	IN PSTR          pReason,
	IN PSTR          pEntityString
	)
{
	HTTP_RESPONSE   response;
	HTTP_DATA_CHUNK dataChunk;
	DWORD           result;
	DWORD           bytesSent;

	//
	// Initialize the HTTP response structure.
	//
	INITIALIZE_HTTP_RESPONSE(&response, StatusCode, pReason);

	//
	// Add a known header.
	//
	ADD_KNOWN_HEADER(response, HttpHeaderContentType, "text/html");

	if (pEntityString)
	{
		// 
		// Add an entity chunk.
		//
		dataChunk.DataChunkType = HttpDataChunkFromMemory;
		dataChunk.FromMemory.pBuffer = pEntityString;
		dataChunk.FromMemory.BufferLength =
			(ULONG)strlen(pEntityString);

		response.EntityChunkCount = 1;
		response.pEntityChunks = &dataChunk;
	}

	// 
	// Because the entity body is sent in one call, it is not
	// required to specify the Content-Length.
	//

	result = HttpSendHttpResponse(
		hReqQueue,           // ReqQueueHandle
		pRequest->RequestId, // Request ID
		0,                   // Flags
		&response,           // HTTP response
		NULL,                // pReserved1
		&bytesSent,          // bytes sent  (OPTIONAL)
		NULL,                // pReserved2  (must be NULL)
		0,                   // Reserved3   (must be 0)
		NULL,                // LPOVERLAPPED(OPTIONAL)
		NULL                 // pReserved4  (must be NULL)
		);

	if (result != NO_ERROR)
	{
		wprintf(L"HttpSendHttpResponse failed with %lu \n", result);
	}

	return result;
}

