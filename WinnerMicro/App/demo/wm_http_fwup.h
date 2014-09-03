/***************************************************************************** 
* 
* File Name : wm_http_fwup.h 
* 
* Description: Http firmware update header file.
* 
* Copyright (c) 2014 Winner Microelectronics Co., Ltd. 
* All rights reserved. 
* 
* Author : wanghf 
* 
* Date : 2014-6-5 
*****************************************************************************/ 

#ifndef WM_HTTP_FWUP_H
#define WM_HTTP_FWUP_H

#include "wm_type_def.h"
#include "wm_http_client.h"

typedef struct _HTTPParameters
{
    CHAR*                  Uri;        
    CHAR*                  ProxyHost;  
    UINT32                  UseProxy ;  
    UINT32                  ProxyPort;
    UINT32                  Verbose;
    CHAR*                  UserName;
    CHAR*                  Password;
    HTTP_AUTH_SCHEMA      AuthType;

} HTTPParameters;

/*************************************************************************** 
* Function: http_fwup 
* Description: Download the firmware from internet by http and upgrade it. 
* 
* Input: ClientParams: The parameters of connecting http server. 
* 
* Output: None 
* 
* Return: 0-success, other- failed 
* 
* Date : 2014-6-5 
****************************************************************************/ 
UINT32   http_fwup(HTTPParameters ClientParams);

#endif //WM_HTTP_FWUP_H
