#ifndef __AUTH_URL_LIBCURL_H__
#define __AUTH_URL_LIBCURL_H__

#include "userdb.h"

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(TURN_NO_AUTH_URL)

/*
	The result resides in a statically allocated buffer and must not
	be freed by the caller.
*/
char *auth_url_curl_version(void);

void auth_url_setup(void);
int auth_url_get_user_key(u08bits *usname, u08bits *realm, hmackey_t key);

#endif
/*TURN_NO_AUTH_URL*/

#ifdef __cplusplus
}
#endif

#endif
/*__AUTH_URL_LIBCURL_H__*/
