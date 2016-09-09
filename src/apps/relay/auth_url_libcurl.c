#include <stdio.h>
#include <string.h>

#if !defined(TURN_NO_AUTH_URL)

#include "auth_url_libcurl.h"
#include "ns_turn_utils.h"
#include "mainrelay.h"
#include "dbdrivers/dbdriver.h"

#include <curl/curl.h>

char *auth_url_curl_version(void)
{
	return curl_version();
}

static const char *turn_params_method(void)
{
	return turn_params.auth_url_use_post ? "POST" : "GET";
}

void auth_url_setup(void)
{
	CURLcode res;

	if (!turn_params.auth_url[0]) {
		return;
	}

	TURN_LOG_FUNC(TURN_LOG_LEVEL_INFO, "Auth URL: %s\n", turn_params.auth_url);
	TURN_LOG_FUNC(TURN_LOG_LEVEL_INFO, "Auth URL Method: %s\n", turn_params_method());

	res = curl_global_init(CURL_GLOBAL_ALL);
	if (res != 0) {
		TURN_LOG_FUNC(TURN_LOG_LEVEL_ERROR, "libcurl initialization failed with result %d. The AUTH URL functionality will be disabled.\n", res);
		turn_params.auth_url[0] = 0;
	}
}

static const size_t RESPONSE_STRING_SIZE = TURN_LONG_STRING_SIZE;

static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	char *response = userdata;
	size_t ptr_bytes = size * nmemb;
	int n = ptr_bytes;

	snprintf(response, RESPONSE_STRING_SIZE, "%s%.*s", response, n, ptr);

	/*
		We ignore any errors due to string overflow. The
		authentication will however fail because the computed hash
		will not match.
	*/

	return ptr_bytes;
}

static char *response_json_result(char *response)
{
	char *start, *end;

	start = strstr(response, "result");
	if (!start) {
		return NULL;
	}

	start = strchr(start, ':');
	if (!start) {
		return NULL;
	}

	start = strchr(start, '"');
	if (!start) {
		return NULL;
	}

	start += 1;

	end = strchr(start, '"');
	if (!end) {
		return NULL;
	}
	*end = '\0';

	return start;
}

/*
	This function can be called from multiple auth threads
	simultaneously and needs to be thread safe.
*/
int auth_url_get_user_key(u08bits *usname, u08bits *realm, hmackey_t key)
{
	int ev = (turn_params.verbose == TURN_VERBOSE_EXTRA);
	const char *base_url = turn_params.auth_url;
	int use_post = turn_params.auth_url_use_post;
	const char *method = turn_params_method();
	char this_url[TURN_LONG_STRING_SIZE];
	char post_body[TURN_LONG_STRING_SIZE];
	char response[RESPONSE_STRING_SIZE] = "";
	char *response_ha1;
	CURL *curl = NULL;
	CURLcode res;
	char *user_escaped = NULL;
	char *realm_escaped = NULL;
	struct curl_slist *headers = NULL;
	int ret = -1;

	if (!turn_params.auth_url[0]) {
		goto skip;
	}

	curl = curl_easy_init();
	if (!curl) {
		TURN_LOG_FUNC(TURN_LOG_LEVEL_ERROR, "Could not initialize curl handle for authentication of user %s in realm %s\n", usname, realm);
		goto err;
	} else if (ev) {
		TURN_LOG_FUNC(TURN_LOG_LEVEL_INFO, "Initialized curl handle %p for authentication of user %s in realm %s\n", curl, usname, realm);
	}

	if (ev) {
		res = curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
		if (res != CURLE_OK) {
			TURN_LOG_FUNC(TURN_LOG_LEVEL_WARNING, "Could not set the verbose flag for the curl handle %p: %s\n", curl, curl_easy_strerror(res));
		}
	}

	if (!use_post) {
		user_escaped = curl_easy_escape(curl, (char *)usname, 0);
		if (!user_escaped) {
			TURN_LOG_FUNC(TURN_LOG_LEVEL_ERROR, "Could not escape username %s during URL construction for curl handle %p\n", usname, curl);
			goto err;
		}

		realm_escaped = curl_easy_escape(curl, (char *)realm, 0);
		if (!realm_escaped) {
			TURN_LOG_FUNC(TURN_LOG_LEVEL_ERROR, "Could not escape realm %s during URL construction for curl handle %p\n", realm, curl);
			goto err;
		}

		snprintf(this_url, sizeof(this_url), "%s?user=%s&realm=%s",
				 base_url, user_escaped, realm_escaped);
	} else {
		snprintf(this_url, sizeof(this_url), "%s", base_url);

		snprintf(post_body, sizeof(post_body),
				 "{\"auth_user\": \"%s\", \"auth_realm\": \"%s\"}",
				 usname, realm);

		res = curl_easy_setopt(curl, CURLOPT_POST, 1L);
		if (res != CURLE_OK) {
			TURN_LOG_FUNC(TURN_LOG_LEVEL_ERROR, "Could not enable HTTP POST for the curl handle %p: %s\n", curl, curl_easy_strerror(res));
			goto err;
		}

		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_body);
		if (res != CURLE_OK) {
			TURN_LOG_FUNC(TURN_LOG_LEVEL_ERROR, "Could not set the POST body for the curl handle %p: %s\n", curl, curl_easy_strerror(res));
			goto err;
		}
	}

	res = curl_easy_setopt(curl, CURLOPT_URL, this_url);
	if (res != CURLE_OK) {
		TURN_LOG_FUNC(TURN_LOG_LEVEL_ERROR, "Could not set the URL %s for the curl handle %p: %s\n", this_url, curl, curl_easy_strerror(res));
		goto err;
	}

	headers = curl_slist_append(headers, "Content-Type: application/json");
	if (!headers) {
		TURN_LOG_FUNC(TURN_LOG_LEVEL_ERROR, "Could not add \"Content-Type\" header to curl slist\n");
		goto err;
	}

	res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	if (res != CURLE_OK) {
		TURN_LOG_FUNC(TURN_LOG_LEVEL_ERROR, "Could not set HTTP headers for the curl handle %p: %s\n", curl, curl_easy_strerror(res));
		goto err;
	}

	res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
	if (res != CURLE_OK) {
		TURN_LOG_FUNC(TURN_LOG_LEVEL_ERROR, "Could not set write callback for the curl handle %p: %s\n", curl, curl_easy_strerror(res));
		goto err;
	}

	res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
	if (res != CURLE_OK) {
		TURN_LOG_FUNC(TURN_LOG_LEVEL_ERROR, "Could not set write callback user data for the curl handle %p: %s\n", curl, curl_easy_strerror(res));
		goto err;
	}

	if (ev) {
		TURN_LOG_FUNC(TURN_LOG_LEVEL_INFO, "Attempting authentication by performing a HTTP %s request to URL %s using CURL handle %p\n", method, this_url, curl);
	} else {
		TURN_LOG_FUNC(TURN_LOG_LEVEL_INFO, "HTTP %s %s\n", method, this_url);
	}

	res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		TURN_LOG_FUNC(TURN_LOG_LEVEL_INFO, "Auth URL request to %s failed: %s\n", this_url, curl_easy_strerror(res));
		goto err;
	}

	if (ev) {
		TURN_LOG_FUNC(TURN_LOG_LEVEL_INFO, "HTTP response body for curl handle %p:\n%s\n", curl, response);
	}

	response_ha1 = response_json_result(response);
	if (!response_ha1) {
		TURN_LOG_FUNC(TURN_LOG_LEVEL_INFO, "HTTP response body for curl handle %p does not contain a JSON object with a \"result\" key:\n%s\n", curl, response);
		goto err;
	}

	if (ev) {
		TURN_LOG_FUNC(TURN_LOG_LEVEL_INFO, "Extracted result %s from HTTP response body for curl handle %p\n", response_ha1, curl);
	}

	size_t sz = get_hmackey_size(SHATYPE_DEFAULT);
	ret = convert_string_key_to_binary(response_ha1, key, sz);
	if (ret != 0) {
		TURN_LOG_FUNC(TURN_LOG_LEVEL_ERROR, "Could not convert response ha1 string %s to hmackey_t: %d\n", response_ha1, ret);
		goto err;
	}

 err:
	if (headers) {
		curl_slist_free_all(headers);
	}
	if (user_escaped) {
		curl_free(user_escaped);
	}
	if (realm_escaped) {
		curl_free(realm_escaped);
	}
	if (curl) {
		curl_easy_cleanup(curl);
	}

 skip:
	return ret;
}

#endif
/*TURN_NO_AUTH_URL*/
