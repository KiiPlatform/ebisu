#include <string.h>
#include "wm_include.h"


#include "light.h"
#include "light_if.h"

#define GPIO_LED1					13


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
    tls_gpio_cfg(GPIO_LED1, TLS_GPIO_DIR_OUTPUT, TLS_GPIO_ATTR_FLOATING);
    tls_gpio_write(GPIO_LED1, light.state);
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
int lightIf_status(light_struct *light)
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
int lightIf_updatePassword(char *pwd)
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
int lightIf_getPassword(char *pwd)
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
int lightIf_getFirmwareVersion(char *version)
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
int lightIf_firmwareUpgrade(char *url, char *version)
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
int lightIf_getIPAddress(char *ipAddress)
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
int lightIf_getIModelName(char *name)
{
    strcpy(name, "FirmwareUpgrade_07-00-XXXX");
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
int lightIf_getMacAddr(char *mac_addr)
{
    unsigned char addr[8];
    int i;	

    memset(addr,0,sizeof(addr));
    tls_get_mac_addr(addr);
    for(i=0; i<6; i++)
    {
	sprintf(mac_addr+strlen(mac_addr), "%02X", addr[i]);
    }
    return 0;
		
}

