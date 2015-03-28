#include <string.h>
#include <stdio.h>
#include <stdlib.h>


#include "light.h"
#include "light_if.h"


/*****************************************************************************
*
*  lightIf_control
*
*  \param  light - light struct
*
*  \return  0:success; -1: failure
*
*  \brief  Controls light
*
*****************************************************************************/
int lightIf_control(light_struct light)
{
	return 0;
}

/*****************************************************************************
*
*  lightIf_status
*
*  \param  *light - light struct poiter
*
*  \return  0:success; -1: failure
*
*  \brief  Gets light status
*
*****************************************************************************/
int lightIf_status(light_struct* light)
{
	light->state = 1;
	light->brightness = 90;
	light->color[0] = 0xf1;
	light->color[1] = 0x88;
	light->color[2] = 0x77;
	light->ct = 80;
	light->effectiveField = LIGHT_EFFECTIVE_FIELD_STATE | LIGHT_EFFECTIVE_FIELD_COLOR | LIGHT_EFFECTIVE_FIELD_BRIGHTESS;
	return 0;
}

/*****************************************************************************
*
*  lightIf_updateStatus
*
*  \param  light - light struct
*
*  \return  0:success; -1: failure
*
*  \brief  Updates light status to cloud
*
*****************************************************************************/
int lightIf_updateStatus(light_struct light)
{
	return light_updateStatus(light);
}

/*****************************************************************************
*
*  lightIf_updatePassword
*
*  \param  pwd - the string of password, it should be allowed to be NULL
*
*  \return  0:success; -1: failure
*
*  \brief  Updates light password
*
*****************************************************************************/
int lightIf_updatePassword(char* pwd)
{
	return 0;
}

/*****************************************************************************
*
*  lightIf_getPassword
*
*  \param  pwd - the string of password, returns NULL if no password
*
*  \return  0:success; -1: failure
*
*  \brief  Gets light password
*
*****************************************************************************/
int lightIf_getPassword(char* pwd)
{
	strcpy(pwd, "123456");
	return 0;
}

/*****************************************************************************
*
*  lightIf_factoryReset
*
*  \param  none
*
*  \return  0:success; -1: failure
*
*  \brief  Factory reset
*
*****************************************************************************/
int lightIf_factoryReset(void)
{
	return 0;
}

/*****************************************************************************
*
*  lightIf_getFirmwareVersion
*
*  \param  version - the string of version name
*
*  \return  0:success; -1: failure
*
*  \brief  gets the current firmware version
*
*****************************************************************************/
int lightIf_getFirmwareVersion(char* version)
{
	strcpy(version, "1.1.00");
	return 0;
}

/*****************************************************************************
*
*  lightIf_firmwareUpgrade
*
*  \param  url - the string of url for downloading firmware
*              version - the string of version name to be upgraded
*
*  \return  0:success; -1: failure
*
*  \brief  Upgrades firmware
*
*****************************************************************************/
int lightIf_firmwareUpgrade(char* url, char* version)
{

	return 0;
}

/*****************************************************************************
*
*  lightIf_getIPAddress
*
*  \param  ipAddress - the string of IP adress
*
*  \return  0:success; -1: failure
*
*  \brief  Gets external IP address
*
*****************************************************************************/
int lightIf_getIPAddress(char* ipAddress)
{
	strcpy(ipAddress, "192.168.1.98");
	return 0;
}

/*****************************************************************************
*
*  lightIf_getIModelName
*
*  \param  name - the string of model name
*
*  \return  0:success; -1: failure
*
*  \brief  Gets model name
*
*****************************************************************************/
int lightIf_getIModelName(char* name)
{
	strcpy(name, "07-00-XXXX");
	return 0;
}

/*****************************************************************************
*
*  lightIf_getMacAddr
*
*  \param  mac_addr - the string of mac address
*
*  \return  0:success; -1: failure
*
*  \brief  Gets mac address
*
*****************************************************************************/
int lightIf_getMacAddr(char* mac_addr)
{

	strcpy(mac_addr, "78B3B90FFEF1");
	return 0;
}
