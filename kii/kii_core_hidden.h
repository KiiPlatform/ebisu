#ifndef _KII_CORE_HIDDEN_
#define _KII_CORE_HIDDEN_

#ifdef __cplusplus
extern "C" {
#endif

/** Initializes Kii SDK
 *
 *  This method is for SDK developer. Application developers does not
 *  need to use this method.
 *
 *  \param [inout] kii core sdk instance.
 *  \param [in] site the input of site name,
 *  should be one of "CN", "CN3", "JP", "US", "SG", "EU"
 *  \param [in] app_id the input of Application ID
 *  \param [in] app_key the input of Application Key
 *  \param [in] info the input of SDK info.
 *  \return  KIIE_OK:success, KIIE_FAIL: failure
 */
kii_error_code_t _kii_core_init_with_info(
        kii_core_t* kii,
        const char* site,
        const char* app_id,
        const char* app_key,
        const char* info);

#ifdef __cplusplus
}
#endif

#endif
