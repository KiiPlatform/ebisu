#include <string.h>
#include "wm_include.h"
#include "wm_http_fwup.h"

#if DEMO_HTTP
#define    HTTP_CLIENT_BUFFER_SIZE   1024
extern u8 RemoteIp[4];
UINT32   http_snd_req(HTTPParameters ClientParams, HTTP_VERB verb, CHAR* pSndData)
{
		INT32                   nRetCode;
    UINT32                  nSize,nTotal = 0;
    CHAR*                   Buffer = NULL;
    HTTP_SESSION_HANDLE     pHTTP;
    UINT32                  nSndDataLen ;
    do
    {
        Buffer = (CHAR*)malloc(HTTP_CLIENT_BUFFER_SIZE);
        if(Buffer == NULL)
            return HTTP_CLIENT_ERROR_NO_MEMORY;
        memset(Buffer, 0, HTTP_CLIENT_BUFFER_SIZE);
        printf("\nHTTP Client v1.0\n\n");
        nSndDataLen = (pSndData==NULL ? 0 : strlen(pSndData));
        // Open the HTTP request handle
        pHTTP = HTTPClientOpenRequest(0);
        if(!pHTTP)
        {
            nRetCode =  HTTP_CLIENT_ERROR_INVALID_HANDLE;
            break;
        }
        // Set the Verb
        nRetCode = HTTPClientSetVerb(pHTTP,verb);
        if(nRetCode != HTTP_CLIENT_SUCCESS)
        {
            break;
        }
#if TLS_CONFIG_HTTP_CLIENT_AUTH
        // Set authentication
        if(ClientParams.AuthType != AuthSchemaNone)
        {
            if((nRetCode = HTTPClientSetAuth(pHTTP,ClientParams.AuthType,NULL)) != HTTP_CLIENT_SUCCESS)
            {
                break;
            }

            // Set authentication
            if((nRetCode = HTTPClientSetCredentials(pHTTP,ClientParams.UserName,ClientParams.Password)) != HTTP_CLIENT_SUCCESS)
            {
                break;
            }
        }
#endif //TLS_CONFIG_HTTP_CLIENT_AUTH
#if TLS_CONFIG_HTTP_CLIENT_PROXY
        // Use Proxy server
        if(ClientParams.UseProxy == TRUE)
        {
            if((nRetCode = HTTPClientSetProxy(pHTTP,ClientParams.ProxyHost,ClientParams.ProxyPort,NULL,NULL)) != HTTP_CLIENT_SUCCESS)
            {

                break;
            }
        }
#endif //TLS_CONFIG_HTTP_CLIENT_PROXY
	 if((nRetCode = HTTPClientSendRequest(pHTTP,ClientParams.Uri,pSndData,nSndDataLen,verb==VerbPost,0,0)) != HTTP_CLIENT_SUCCESS)
        {
            break;
        }
        // Retrieve the the headers and analyze them
        if((nRetCode = HTTPClientRecvResponse(pHTTP,3)) != HTTP_CLIENT_SUCCESS)
        {
            break;
        }
	 printf("Start to receive data from remote server...\n");
        // Get the data until we get an error or end of stream code
        while(nRetCode == HTTP_CLIENT_SUCCESS || nRetCode != HTTP_CLIENT_EOS)
        {
            // Set the size of our buffer
            nSize = HTTP_CLIENT_BUFFER_SIZE;   
            // Get the data
            nRetCode = HTTPClientReadData(pHTTP,Buffer,nSize,0,&nSize);
		if(nRetCode != HTTP_CLIENT_SUCCESS && nRetCode != HTTP_CLIENT_EOS)
			break;
		printf("%s", Buffer);
            nTotal += nSize;
        }
    } while(0); // Run only once
    free(Buffer);
    if(pHTTP)
        HTTPClientCloseRequest(&pHTTP);
    if(ClientParams.Verbose == TRUE)
    {
        printf("\n\nHTTP Client terminated %d (got %d kb)\n\n",nRetCode,(nTotal/ 1024));
    }
    return nRetCode;
}

UINT32 http_get(HTTPParameters ClientParams)
{
    return http_snd_req(ClientParams, VerbGet, NULL);
}

UINT32 http_post(HTTPParameters ClientParams, CHAR* pSndData)
{
    return http_snd_req(ClientParams, VerbPost, pSndData);
}

int http_get_demo(char *buf)
{
    HTTPParameters httpParams;
    memset(&httpParams, 0, sizeof(HTTPParameters));
    httpParams.Uri = (CHAR*)malloc(128);
    if(httpParams.Uri == NULL)
    {
        printf("malloc error.\n");
        return WM_FAILED;
    }
    memset(httpParams.Uri, 0, 128);
    sprintf(httpParams.Uri, "http://%d.%d.%d.%d:8080/TestWeb/", RemoteIp[0],RemoteIp[1],RemoteIp[2],RemoteIp[3]);
    //httpParams.ProxyHost = "61.175.96.34";
    //httpParams.ProxyPort = 9999;
    //httpParams.UseProxy = TRUE;
    httpParams.Verbose = TRUE;
    printf("Location: %s\n",httpParams.Uri);
    http_get(httpParams);
    free(httpParams.Uri);

    return WM_SUCCESS;
}
int http_post_demo(char* postData)
{
	HTTPParameters httpParams;
	extern const char HTTP_POST[];
	memset(&httpParams, 0, sizeof(HTTPParameters));
	httpParams.Uri = (CHAR*)malloc(128);
	if(httpParams.Uri == NULL)
	{
	    printf("malloc error.\n");
	    return WM_FAILED;
	}
	memset(httpParams.Uri, 0, 128);
	sprintf(httpParams.Uri, "http://%d.%d.%d.%d:8080/TestWeb/login.do", RemoteIp[0],RemoteIp[1],RemoteIp[2],RemoteIp[3]);
	printf("Location: %s\n",httpParams.Uri);
	httpParams.Verbose = TRUE;
	http_post(httpParams, postData + strlen(HTTP_POST));
	free(httpParams.Uri);
	return WM_SUCCESS;
}

#if TLS_CONFIG_SOCKET_RAW
int http_fwup_demo(char *buf)
{
	HTTPParameters httpParams;
	memset(&httpParams, 0, sizeof(HTTPParameters));
	httpParams.Uri = (CHAR*)malloc(128);
	if(httpParams.Uri == NULL)
	{
	    printf("malloc error.\n");
	    return WM_FAILED;
	}
	memset(httpParams.Uri, 0, 128);
	sprintf(httpParams.Uri, "http://%d.%d.%d.%d:8080/TestWeb/cuckoo.do", RemoteIp[0],RemoteIp[1],RemoteIp[2],RemoteIp[3]);
	printf("Location: %s\n",httpParams.Uri);
	httpParams.Verbose = TRUE;
	http_fwup(httpParams);
	free(httpParams.Uri);

	return WM_SUCCESS;
}
#endif

#endif //DEMO_HTTP

