/****************************************************************************
 *
 * Copyright 2017 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/

#include <stdlib.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <netpacket/packet.h>

#include "securevirtualresourcetypes.h"

//Added as workround to test cert based (D)TLS connection
#include "srmresourcestrings.h"

#include "logging/things_logger.h"
#include "utils/things_malloc.h"
#include "utils/things_string.h"
#include "things_security_manager.h"
#include "things_common.h"
#include "things_sss_manager.h"
#include "things_iotivity_lock.h"
#include "credresource.h"
#include "security/ss_sha2.h"
#include "oxmverifycommon.h"
#include "oic_string.h"
#include "utlist.h"
#include "aclresource.h"
#include "srmutility.h"
#include "things_data_manager.h"

#include <wifi_manager/wifi_manager.h>
#define TAG "[OIC_SEC_MGR]"

#ifdef CONFIG_SVR_DB_SECURESTORAGE
#include "security/sss_security/sss_storage_server.h"
#endif

typedef enum {
	OIC_SEC_OK = 0,
	OIC_SEC_ERROR = 1,
	OIC_SEC_INVALID_PARAM = 2
} OICSecurityResult;

#if defined(__MIPS__)
#define STRING_SVR_DB_PATH "/data1/oic_svr_db_server.dat"
#elif defined(__TIZEN__)
#define STRING_SVR_DB_PATH "/opt/data/ocfData/oic_svr_db_server.dat"
#else
#define STRING_SVR_DB_PATH "./oic_svr_db_server.dat"
#endif

#define MD_MAX_LEN 64
#define MAC_BUF_SIZE 64
#define MAX_PATH_LEN 100

static char SVR_DB_PATH[MAX_PATH_LEN] = { 0 };

static things_auth_type_e g_auth_type = AUTH_JUST_WORKS;
static bool g_is_svr_db_exist = false;
static bool g_is_mfg_cert_required = false;
#ifdef __ST_THINGS_RTOS__
#define USER_CONFIRM 1
#define SetVerifyOption(x) {}
#define sync() {}
#endif
const unsigned char OIC_SVR_DB_COMMON[] = {
	0xBF, 0x63, 0x61, 0x63, 0x6C, 0x59, 0x03, 0x2B, 0xA4, 0x66, 0x61, 0x63, 0x6C, 0x69, 0x73, 0x74,
	0xA1, 0x64, 0x61, 0x63, 0x65, 0x73, 0x84, 0xA3, 0x6B, 0x73, 0x75, 0x62, 0x6A, 0x65, 0x63, 0x74,
	0x75, 0x75, 0x69, 0x64, 0x61, 0x2A, 0x69, 0x72, 0x65, 0x73, 0x6F, 0x75, 0x72, 0x63, 0x65, 0x73,
	0x83, 0xA4, 0x64, 0x68, 0x72, 0x65, 0x66, 0x68, 0x2F, 0x6F, 0x69, 0x63, 0x2F, 0x72, 0x65, 0x73,
	0x62, 0x72, 0x74, 0x81, 0x6A, 0x6F, 0x69, 0x63, 0x2E, 0x77, 0x6B, 0x2E, 0x72, 0x65, 0x73, 0x62,
	0x69, 0x66, 0x81, 0x69, 0x6F, 0x69, 0x63, 0x2E, 0x69, 0x66, 0x2E, 0x6C, 0x6C, 0x63, 0x72, 0x65,
	0x6C, 0x60, 0xA4, 0x64, 0x68, 0x72, 0x65, 0x66, 0x66, 0x2F, 0x6F, 0x69, 0x63, 0x2F, 0x64, 0x62,
	0x72, 0x74, 0x81, 0x68, 0x6F, 0x69, 0x63, 0x2E, 0x77, 0x6B, 0x2E, 0x64, 0x62, 0x69, 0x66, 0x82,
	0x6F, 0x6F, 0x69, 0x63, 0x2E, 0x69, 0x66, 0x2E, 0x62, 0x61, 0x73, 0x65, 0x6C, 0x69, 0x6E, 0x65,
	0x68, 0x6F, 0x69, 0x63, 0x2E, 0x69, 0x66, 0x2E, 0x72, 0x63, 0x72, 0x65, 0x6C, 0x60, 0xA4, 0x64,
	0x68, 0x72, 0x65, 0x66, 0x66, 0x2F, 0x6F, 0x69, 0x63, 0x2F, 0x70, 0x62, 0x72, 0x74, 0x81, 0x68,
	0x6F, 0x69, 0x63, 0x2E, 0x77, 0x6B, 0x2E, 0x70, 0x62, 0x69, 0x66, 0x82, 0x6F, 0x6F, 0x69, 0x63,
	0x2E, 0x69, 0x66, 0x2E, 0x62, 0x61, 0x73, 0x65, 0x6C, 0x69, 0x6E, 0x65, 0x68, 0x6F, 0x69, 0x63,
	0x2E, 0x69, 0x66, 0x2E, 0x72, 0x63, 0x72, 0x65, 0x6C, 0x60, 0x6A, 0x70, 0x65, 0x72, 0x6D, 0x69,
	0x73, 0x73, 0x69, 0x6F, 0x6E, 0x02, 0xA3, 0x6B, 0x73, 0x75, 0x62, 0x6A, 0x65, 0x63, 0x74, 0x75,
	0x75, 0x69, 0x64, 0x61, 0x2A, 0x69, 0x72, 0x65, 0x73, 0x6F, 0x75, 0x72, 0x63, 0x65, 0x73, 0x84,
	0xA4, 0x64, 0x68, 0x72, 0x65, 0x66, 0x6D, 0x2F, 0x6F, 0x69, 0x63, 0x2F, 0x73, 0x65, 0x63, 0x2F,
	0x64, 0x6F, 0x78, 0x6D, 0x62, 0x72, 0x74, 0x81, 0x6A, 0x6F, 0x69, 0x63, 0x2E, 0x72, 0x2E, 0x64,
	0x6F, 0x78, 0x6D, 0x62, 0x69, 0x66, 0x81, 0x6F, 0x6F, 0x69, 0x63, 0x2E, 0x69, 0x66, 0x2E, 0x62,
	0x61, 0x73, 0x65, 0x6C, 0x69, 0x6E, 0x65, 0x63, 0x72, 0x65, 0x6C, 0x60, 0xA4, 0x64, 0x68, 0x72,
	0x65, 0x66, 0x6E, 0x2F, 0x6F, 0x69, 0x63, 0x2F, 0x73, 0x65, 0x63, 0x2F, 0x70, 0x73, 0x74, 0x61,
	0x74, 0x62, 0x72, 0x74, 0x81, 0x6B, 0x6F, 0x69, 0x63, 0x2E, 0x72, 0x2E, 0x70, 0x73, 0x74, 0x61,
	0x74, 0x62, 0x69, 0x66, 0x81, 0x6F, 0x6F, 0x69, 0x63, 0x2E, 0x69, 0x66, 0x2E, 0x62, 0x61, 0x73,
	0x65, 0x6C, 0x69, 0x6E, 0x65, 0x63, 0x72, 0x65, 0x6C, 0x60, 0xA4, 0x64, 0x68, 0x72, 0x65, 0x66,
	0x6C, 0x2F, 0x6F, 0x69, 0x63, 0x2F, 0x73, 0x65, 0x63, 0x2F, 0x61, 0x63, 0x6C, 0x62, 0x72, 0x74,
	0x81, 0x69, 0x6F, 0x69, 0x63, 0x2E, 0x72, 0x2E, 0x61, 0x63, 0x6C, 0x62, 0x69, 0x66, 0x81, 0x6F,
	0x6F, 0x69, 0x63, 0x2E, 0x69, 0x66, 0x2E, 0x62, 0x61, 0x73, 0x65, 0x6C, 0x69, 0x6E, 0x65, 0x63,
	0x72, 0x65, 0x6C, 0x60, 0xA4, 0x64, 0x68, 0x72, 0x65, 0x66, 0x6D, 0x2F, 0x6F, 0x69, 0x63, 0x2F,
	0x73, 0x65, 0x63, 0x2F, 0x63, 0x72, 0x65, 0x64, 0x62, 0x72, 0x74, 0x81, 0x6A, 0x6F, 0x69, 0x63,
	0x2E, 0x72, 0x2E, 0x63, 0x72, 0x65, 0x64, 0x62, 0x69, 0x66, 0x81, 0x6F, 0x6F, 0x69, 0x63, 0x2E,
	0x69, 0x66, 0x2E, 0x62, 0x61, 0x73, 0x65, 0x6C, 0x69, 0x6E, 0x65, 0x63, 0x72, 0x65, 0x6C, 0x60,
	0x6A, 0x70, 0x65, 0x72, 0x6D, 0x69, 0x73, 0x73, 0x69, 0x6F, 0x6E, 0x06, 0xA3, 0x6B, 0x73, 0x75,
	0x62, 0x6A, 0x65, 0x63, 0x74, 0x75, 0x75, 0x69, 0x64, 0x61, 0x2A, 0x69, 0x72, 0x65, 0x73, 0x6F,
	0x75, 0x72, 0x63, 0x65, 0x73, 0x81, 0xA3, 0x64, 0x68, 0x72, 0x65, 0x66, 0x75, 0x2F, 0x73, 0x65,
	0x63, 0x2F, 0x70, 0x72, 0x6F, 0x76, 0x69, 0x73, 0x69, 0x6F, 0x6E, 0x69, 0x6E, 0x67, 0x69, 0x6E,
	0x66, 0x6F, 0x62, 0x72, 0x74, 0x81, 0x78, 0x1E, 0x78, 0x2E, 0x63, 0x6F, 0x6D, 0x2E, 0x73, 0x61,
	0x6D, 0x73, 0x75, 0x6E, 0x67, 0x2E, 0x70, 0x72, 0x6F, 0x76, 0x69, 0x73, 0x69, 0x6F, 0x6E, 0x69,
	0x6E, 0x67, 0x69, 0x6E, 0x66, 0x6F, 0x62, 0x69, 0x66, 0x81, 0x68, 0x6F, 0x69, 0x63, 0x2E, 0x69,
	0x66, 0x2E, 0x61, 0x6A, 0x70, 0x65, 0x72, 0x6D, 0x69, 0x73, 0x73, 0x69, 0x6F, 0x6E, 0x07, 0xA3,
	0x6B, 0x73, 0x75, 0x62, 0x6A, 0x65, 0x63, 0x74, 0x75, 0x75, 0x69, 0x64, 0x61, 0x2A, 0x69, 0x72,
	0x65, 0x73, 0x6F, 0x75, 0x72, 0x63, 0x65, 0x73, 0x81, 0xA3, 0x64, 0x68, 0x72, 0x65, 0x66, 0x74,
	0x2F, 0x73, 0x65, 0x63, 0x2F, 0x61, 0x63, 0x63, 0x65, 0x73, 0x73, 0x70, 0x6F, 0x69, 0x6E, 0x74,
	0x6C, 0x69, 0x73, 0x74, 0x62, 0x72, 0x74, 0x81, 0x78, 0x1D, 0x78, 0x2E, 0x63, 0x6F, 0x6D, 0x2E,
	0x73, 0x61, 0x6D, 0x73, 0x75, 0x6E, 0x67, 0x2E, 0x61, 0x63, 0x63, 0x65, 0x73, 0x73, 0x70, 0x6F,
	0x69, 0x6E, 0x74, 0x6C, 0x69, 0x73, 0x74, 0x62, 0x69, 0x66, 0x81, 0x68, 0x6F, 0x69, 0x63, 0x2E,
	0x69, 0x66, 0x2E, 0x73, 0x6A, 0x70, 0x65, 0x72, 0x6D, 0x69, 0x73, 0x73, 0x69, 0x6F, 0x6E, 0x02,
	0x6A, 0x72, 0x6F, 0x77, 0x6E, 0x65, 0x72, 0x75, 0x75, 0x69, 0x64, 0x78, 0x24, 0x30, 0x30, 0x30,
	0x30, 0x30, 0x30, 0x30, 0x30, 0x2D, 0x30, 0x30, 0x30, 0x30, 0x2D, 0x30, 0x30, 0x30, 0x30, 0x2D,
	0x30, 0x30, 0x30, 0x30, 0x2D, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
	0x30, 0x62, 0x72, 0x74, 0x81, 0x69, 0x6F, 0x69, 0x63, 0x2E, 0x72, 0x2E, 0x61, 0x63, 0x6C, 0x62,
	0x69, 0x66, 0x81, 0x6F, 0x6F, 0x69, 0x63, 0x2E, 0x69, 0x66, 0x2E, 0x62, 0x61, 0x73, 0x65, 0x6C,
	0x69, 0x6E, 0x65, 0x65, 0x70, 0x73, 0x74, 0x61, 0x74, 0x58, 0x9D, 0xA9, 0x64, 0x69, 0x73, 0x6F,
	0x70, 0xF4, 0x62, 0x63, 0x6D, 0x02, 0x62, 0x74, 0x6D, 0x00, 0x62, 0x6F, 0x6D, 0x04, 0x62, 0x73,
	0x6D, 0x04, 0x6A, 0x64, 0x65, 0x76, 0x69, 0x63, 0x65, 0x75, 0x75, 0x69, 0x64, 0x78, 0x24, 0x30,
	0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x2D, 0x30, 0x30, 0x30, 0x30, 0x2D, 0x30, 0x30, 0x30,
	0x30, 0x2D, 0x30, 0x30, 0x30, 0x30, 0x2D, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
	0x30, 0x30, 0x30, 0x6A, 0x72, 0x6F, 0x77, 0x6E, 0x65, 0x72, 0x75, 0x75, 0x69, 0x64, 0x78, 0x24,
	0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x2D, 0x30, 0x30, 0x30, 0x30, 0x2D, 0x30, 0x30,
	0x30, 0x30, 0x2D, 0x30, 0x30, 0x30, 0x30, 0x2D, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
	0x30, 0x30, 0x30, 0x30, 0x62, 0x72, 0x74, 0x81, 0x6B, 0x6F, 0x69, 0x63, 0x2E, 0x72, 0x2E, 0x70,
	0x73, 0x74, 0x61, 0x74, 0x62, 0x69, 0x66, 0x81, 0x6F, 0x6F, 0x69, 0x63, 0x2E, 0x69, 0x66, 0x2E,
	0x62, 0x61, 0x73, 0x65, 0x6C, 0x69, 0x6E, 0x65, 0x64, 0x64, 0x6F, 0x78, 0x6D, 0x58
};

//, 0xD9 + oxm array size /*length of doxm header + length of doxm payload */,
#define DOXM_PAYLOAD_SIZE (0xD4)

const unsigned char OIC_SVR_DB_DOXM_HEADER[] = {
	0xBF, 0x64, 0x6F, 0x78, 0x6D, 0x73
};

//0x80 + oxm array size /*oxm size*/,
#define DOXM_OXM_SIZE (0x80)

// 0x00, 0x01, 0x02 : oxm value as array

const unsigned char OIC_SVR_DB_DOXM_PAYLOAD[] = {
	0x66, 0x6F, 0x78, 0x6D, 0x73, 0x65, 0x6C, 0x00, 0x63, 0x73, 0x63, 0x74, 0x01, 0x65, 0x6F, 0x77,
	0x6E, 0x65, 0x64, 0xF4, 0x6A, 0x64, 0x65, 0x76, 0x69, 0x63, 0x65, 0x75, 0x75, 0x69, 0x64, 0x78,
	0x24, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x2D, 0x30, 0x30, 0x30, 0x30, 0x2D, 0x30,
	0x30, 0x30, 0x30, 0x2D, 0x30, 0x30, 0x30, 0x30, 0x2D, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
	0x30, 0x30, 0x30, 0x30, 0x30, /*0x63, 0x6D, 0x6F, 0x6D, 0x01, *//*"mom":1 */ 0x6C, 0x64, 0x65, 0x76, 0x6F, 0x77,
	0x6E, 0x65, 0x72, 0x75, 0x75, 0x69, 0x64, 0x78, 0x24, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
	0x30, 0x2D, 0x30, 0x30, 0x30, 0x30, 0x2D, 0x30, 0x30, 0x30, 0x30, 0x2D, 0x30, 0x30, 0x30, 0x30,
	0x2D, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x6A, 0x72, 0x6F,
	0x77, 0x6E, 0x65, 0x72, 0x75, 0x75, 0x69, 0x64, 0x78, 0x24, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
	0x30, 0x30, 0x2D, 0x30, 0x30, 0x30, 0x30, 0x2D, 0x30, 0x30, 0x30, 0x30, 0x2D, 0x30, 0x30, 0x30,
	0x30, 0x2D, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x62, 0x72,
	0x74, 0x81, 0x6A, 0x6F, 0x69, 0x63, 0x2E, 0x72, 0x2E, 0x64, 0x6F, 0x78, 0x6D, 0x62, 0x69, 0x66,
	0x81, 0x6F, 0x6F, 0x69, 0x63, 0x2E, 0x69, 0x66, 0x2E, 0x62, 0x61, 0x73, 0x65, 0x6C, 0x69, 0x6E,
	0x65, 0xFF, 0xFF
};

#ifdef CONFIG_SVR_DB_SECURESTORAGE
FILE *server_secure_fopen(const char *path, const char *mode)	// pkss
{
	THINGS_LOG_D(TAG, "F SVR DB File Path : %s", SVR_DB_PATH);
	(void)path;
	return secure_fopen(SVR_DB_PATH, mode);
}
#else
FILE *server_fopen(const char *path, const char *mode)	// pkss
{
	THINGS_LOG_D(TAG, "F SVR DB File Path : %s", SVR_DB_PATH);
	(void)path;
	return fopen(SVR_DB_PATH, mode);
}
#endif

size_t server_fread(FAR void *ptr, size_t size, size_t n_items, FAR FILE *stream)
{
	struct timeval start, end;
	size_t ret = 0;
	gettimeofday(&start, NULL);
	ret = fread(ptr, size, n_items, stream);
	gettimeofday(&end, NULL);
	return ret;
}

size_t server_fwrite(FAR const void *ptr, size_t size, size_t n_items, FAR FILE *stream)
{
	struct timeval start, end;
	size_t ret = 0;
	gettimeofday(&start, NULL);

	ret = fwrite(ptr, size, n_items, stream);

	gettimeofday(&end, NULL);
	return ret;
}

int server_fclose(FILE *stream)
{
	int ret = fclose(stream);
	return ret;
}

int server_unlink(const char *path)
{
	return unlink(path);
}

/*
 * This API Setup Security key from filepath
 */
static OCStackResult seckey_setup(const char *filename, OicSecKey_t *key, OicEncodingType_t encoding)
{
	THINGS_LOG_D(TAG, "IN: %s", __func__);

	key->data = NULL;
	key->len = 0;
	key->encoding = OIC_ENCODING_UNKNOW;
	FILE *fp = fopen(filename, "rb");

	if (fp == NULL) {
		THINGS_LOG_E(TAG, "Can not open file[%s].", filename);
		THINGS_LOG_D(TAG, "OUT[FAIL]: %s", __func__);
		return OC_STACK_ERROR;
	}

	if (encoding == OIC_ENCODING_UNKNOW) {
		const char *file_ext;
		int filename_len = strlen(filename);
		file_ext = filename + filename_len;

		while (file_ext >= filename) {
			if (*file_ext == '.') {
				break;
			}
			file_ext--;
		}

		if (strncmp(file_ext + 1, "PEM", 3) == 0 || strncmp(file_ext + 1, "pem", 3) == 0) {
			key->encoding = OIC_ENCODING_PEM;
		} else {
			key->encoding = OIC_ENCODING_DER;
		}
	} else {
		key->encoding = encoding;
	}

	if (0 == fseek(fp, 0L, SEEK_END)) {
		size_t size = ftell(fp);
		rewind(fp);
		key->data = (uint8_t *)things_malloc(size);
		if (key->data == NULL) {
			THINGS_LOG_E(TAG, "Memory Full");
			THINGS_LOG_D(TAG, "OUT[FAIL]: %s", __func__);
			fclose(fp);
			return OC_STACK_NO_MEMORY;
		}
		fread(key->data, 1, size, fp);
		key->len = size;
	}

	fclose(fp);
	THINGS_LOG_D(TAG, "OUT: %s", __func__);
	return OC_STACK_OK;
}

/*
 * This API added as workaround to test certificate based (D)TLS connection.
 * It will be replaced to use TZ or eSE based key protection.
 */
static OCStackResult save_signed_asymmetric_key(OicUuid_t *subject_uuid);

static int GenerateSvrDb(OCPersistentStorage *ps)
{
	THINGS_LOG_V(TAG, "In %s", __func__);

	if (AUTH_UNKNOW == g_auth_type) {
		THINGS_LOG_V(TAG, "Unknown authentication type for ownership transfer.");
		THINGS_LOG_V(TAG, "please make sure your configuration in json file.");
		THINGS_LOG_V(TAG, "----------------------");
		THINGS_LOG_V(TAG, "Justwork authentication will be used as default authentication method.");
		THINGS_LOG_V(TAG, "----------------------");
		g_auth_type = AUTH_JUST_WORKS;
	}

	FILE *fp = NULL;
	fp = ps->open(SVR_DB_PATH, "w");

	OICSecurityResult res = OIC_SEC_ERROR;

	if (NULL != fp) {
		int idx = 0;
		unsigned char *svrdb = NULL;
		unsigned char *cur_pos = NULL;
		//To generate OxM Array.
		uint16_t oxms[OIC_OXM_COUNT + 3 /*Number of vendor specific OxM */] = { 0 };
		size_t oxm_cnt = 0;
		size_t oxm_size = 0;
		size_t svrdb_size = 0;

		if (g_auth_type & AUTH_JUST_WORKS) {
			THINGS_LOG_D(TAG, "Added Justworks OxM.");
			oxms[oxm_cnt++] = OIC_JUST_WORKS;
			oxm_size++;
		}
		if (g_auth_type & AUTH_RANDOM_PIN) {
			THINGS_LOG_D(TAG, "Added Random PIN based OxM.");
			oxms[oxm_cnt++] = OIC_RANDOM_DEVICE_PIN;
			oxm_size++;
		}
		if (g_auth_type & AUTH_CERTIFICATE || g_auth_type & AUTH_CERTIFICATE_CONFIRM) {
			THINGS_LOG_D(TAG, "Added Certificate based OxM.");
			oxms[oxm_cnt++] = OIC_MANUFACTURER_CERTIFICATE;
			oxm_size++;
		}
		if (g_auth_type & AUTH_DECENTRALIZED_PUB_KEY) {
			THINGS_LOG_D(TAG, "Added Decentralized public key based OxM.");
			oxms[oxm_cnt++] = OIC_DECENTRALIZED_PUBLIC_KEY;
			oxm_size++;
		}
		if (g_auth_type & AUTH_PRECONF_PIN) {
			THINGS_LOG_D(TAG, "Added Preconfigured-PIN based OxM (for MOT only).");
			oxms[oxm_cnt++] = 0xFF00;	//OIC_PRECONFIG_PIN
			oxm_size += 3;
		}
		if (g_auth_type & AUTH_JUST_WORKS_MUTUAL_VERIFIED) {
			THINGS_LOG_D(TAG, "Added Mutual Verification Justworks based OxM.");
			oxms[oxm_cnt++] = 0xFF01;
			oxm_size += 3;
		}
		if (g_auth_type & AUTH_CERTIFICATE || g_auth_type & AUTH_CERTIFICATE_CONFIRM) {
			THINGS_LOG_D(TAG, "Added Certificate+Confirm based OxM.");
			oxms[oxm_cnt++] = 0xFF02;
			oxm_size += 3;
		}
		if (0 == oxm_cnt) {
			THINGS_LOG_E(TAG, "Failed to extract authentication types.");
			THINGS_LOG_E(TAG, "Please make sure your configuration in json file.");
			res = OIC_SEC_INVALID_PARAM;
			goto error;
		}

		svrdb_size = sizeof(OIC_SVR_DB_COMMON) + sizeof(OIC_SVR_DB_DOXM_HEADER) + sizeof(OIC_SVR_DB_DOXM_PAYLOAD) + 2 /*size of payload length */  + oxm_size;
		svrdb = (unsigned char *)things_malloc(svrdb_size + 32);
		if (NULL == svrdb) {
			THINGS_LOG_E(TAG, "Failed to memory allocation.");
			goto error;
		}
		//
		//Generate default SVR DB
		cur_pos = svrdb;

		//Construct Common part of SVR DB
		memcpy(cur_pos, OIC_SVR_DB_COMMON, sizeof(OIC_SVR_DB_COMMON));
		cur_pos += sizeof(OIC_SVR_DB_COMMON);

		//Construct Doxm payload size
		*cur_pos = (unsigned char)(DOXM_PAYLOAD_SIZE + oxm_size);
		cur_pos++;

		//Construct Doxm Header
		memcpy(cur_pos, OIC_SVR_DB_DOXM_HEADER, sizeof(OIC_SVR_DB_DOXM_HEADER));
		cur_pos += sizeof(OIC_SVR_DB_DOXM_HEADER);

		//Construct Length of OxM array
		*cur_pos = (unsigned char)(DOXM_OXM_SIZE + oxm_cnt);
		cur_pos++;

		//Construct OxM array
		for (idx = 0; idx < oxm_cnt; idx++) {
			if (oxms[idx] < OIC_OXM_COUNT) {
				*cur_pos = oxms[idx];
				cur_pos++;
			} else if (oxms[idx] == 0xFF00) {
				*cur_pos = 0x19;
				cur_pos++;
				*cur_pos = 0xFF;
				cur_pos++;
				*cur_pos = 0x00;
				cur_pos++;
			} else if (oxms[idx] == 0xFF01) {
				*cur_pos = 0x19;
				cur_pos++;
				*cur_pos = 0xFF;
				cur_pos++;
				*cur_pos = 0x01;
				cur_pos++;
			} else if (oxms[idx] == 0xFF02) {
				*cur_pos = 0x19;
				cur_pos++;
				*cur_pos = 0xFF;
				cur_pos++;
				*cur_pos = 0x02;
				cur_pos++;
			}
		}

		//Construct Doxm payload
		memcpy(cur_pos, OIC_SVR_DB_DOXM_PAYLOAD, sizeof(OIC_SVR_DB_DOXM_PAYLOAD));

		//End of default SVR DB generation
		//

		//Save the constructed SVR DB into persistent storage.
		ps->write(svrdb, 1, svrdb_size, fp);
		ps->close(fp);
		things_free(svrdb);
		THINGS_LOG_V(TAG, "Out %s", __func__);
		return OIC_SEC_OK;
error:
		ps->close(fp);
		things_free(svrdb);
		return OIC_SEC_ERROR;
	} else {
		THINGS_LOG_E(TAG, "Can not open the [%s], Please make sure the access permission of file system.", SVR_DB_PATH);
	}

	return res;					// return 0 when failed, 1 otherwise..
}

static OCStackResult sm_secure_resource_check(OicUuid_t *device_id)
{
	// Check Device is Owned
	bool isOwned = false;
	OCStackResult oc_res = GetDoxmIsOwned(&isOwned);
	if (OC_STACK_OK != oc_res) {
		THINGS_LOG_E(TAG, "Error in GetDoxmIsOwned : %d", (int)oc_res);
		return oc_res;
	}
	if (!isOwned) {
		oc_res = SetDoxmDeviceID(device_id);
		if (OC_STACK_OK != oc_res) {
			THINGS_LOG_E(TAG, "Error in SetDoxmDeviceID : %d", (int)oc_res);
			return oc_res;
		}
	}

#ifdef CONFIG_ST_THINGS_HW_CERT_KEY
	if (dm_get_easy_setup_use_artik_crt()) {
		oc_res = save_signed_asymmetric_key(device_id);
		if (OC_STACK_OK != oc_res) {
			THINGS_LOG_E(TAG, "Error in save_signed_asymmetric_key : %d", (int)oc_res);
			return oc_res;
		}
	} else
#endif
	{
		// Check SVR_DB has CA & Key
		uint8_t cred_ret = 0;
		OicSecCred_t *temp = NULL;
		LL_FOREACH(GetCredList(), temp) {
			if (temp->credUsage) {
				if (0 == strcmp(temp->credUsage, TRUST_CA)) {
					cred_ret |= (1 << 0);
				} else if (0 == strcmp(temp->credUsage, PRIMARY_CERT)) {
					cred_ret |= (1 << 1);
				} else if (0 == strcmp(temp->credUsage, MF_TRUST_CA)) {
					cred_ret |= (1 << 2);
				} else if (0 == strcmp(temp->credUsage, MF_PRIMARY_CERT)) {
					cred_ret |= (1 << 3);
				}
			}
		}

		if (!(cred_ret & (1 << 0)) || !(cred_ret & (1 << 1)) || !(cred_ret & (1 << 2)) || !(cred_ret & (1 << 3))) {
			DeleteCredList(GetCredList());
			oc_res = save_signed_asymmetric_key(device_id);
			if (OC_STACK_OK != oc_res) {
				THINGS_LOG_E(TAG, "Error in save_signed_asymmetric_key : %d", (int)oc_res);
				return oc_res;
			}
		}
	}
	return oc_res;
}

static int get_mac_addr(unsigned char *p_id_buf, size_t p_id_buf_size, unsigned int *p_id_out_len)
{
	THINGS_LOG_D(TAG, "In %s", __func__);

#ifdef __ST_THINGS_RTOS__
	wifi_manager_info_s st_wifi_info;
	wifi_manager_get_info(&st_wifi_info);

	if (wifi_manager_get_info(&st_wifi_info) != WIFI_MANAGER_SUCCESS) {

		THINGS_LOG_E(TAG, "MAC Get Error\n");
		return OIC_SEC_ERROR;
	}

	snprintf((char *)p_id_buf, MAC_BUF_SIZE - 1, "%02X%02X%02X%02X%02X%02X", st_wifi_info.mac_address[0], st_wifi_info.mac_address[1], st_wifi_info.mac_address[2], st_wifi_info.mac_address[3], st_wifi_info.mac_address[4], st_wifi_info.mac_address[5]);
#else
	struct ifaddrs *ifaddr = NULL;
	struct ifaddrs *ifa = NULL;

	if (getifaddrs(&ifaddr) == -1) {
		THINGS_LOG_E(TAG, "Failed to read network address information.");
		return OIC_SEC_ERROR;
	} else {
		for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
			if ((ifa->ifa_addr) && (ifa->ifa_addr->sa_family == AF_PACKET)) {
				struct sockaddr_ll *s = (struct sockaddr_ll *)ifa->ifa_addr;
				memset(p_id_buf, 0x00, p_id_buf_size);
				int i = 0;
				for (i = 0; i < s->sll_halen && i < p_id_buf_size; i++) {
					snprintf((char *)(p_id_buf + (i * 2)), MAC_BUF_SIZE - i, "%02X", (s->sll_addr[i]));
				}
			}
		}
		freeifaddrs(ifaddr);
	}
#endif
	THINGS_LOG_D(TAG, "MAC Address : %s", (char *)p_id_buf);
	*p_id_out_len = strlen((char *)p_id_buf);

	THINGS_LOG_D(TAG, "Out %s", __func__);

	return OIC_SEC_OK;
}

// To support MAC based UUID
static int sm_generate_mac_based_device_id(void)
{
	THINGS_LOG_D(TAG, "In %s", __func__);

	OicUuid_t device_id;
	unsigned char mac_id[MAC_BUF_SIZE];
	unsigned char hash_value[SS_SHA256_DIGEST_SIZE + 1];
	unsigned int id_len = 0;
	OICSecurityResult res = get_mac_addr(mac_id, MAC_BUF_SIZE, &id_len);
	if (OIC_SEC_OK != res) {
		THINGS_LOG_E(TAG, "Failed to read MAC Address.");
		return res;
	}
	THINGS_LOG_V(TAG, "MAC Address : %s", mac_id);

	ss_sha256_ctx sha256_ctx;
	ss_sha256_init(&sha256_ctx);
	ss_sha256_update(&sha256_ctx, mac_id, id_len);
	ss_sha256_final(&sha256_ctx, hash_value);

	memcpy(device_id.id, hash_value, sizeof(device_id.id));
	char *uuid_str = NULL;
	OCStackResult oc_res = ConvertUuidToStr(&device_id, &uuid_str);
	if (OC_STACK_OK != oc_res) {
		THINGS_LOG_E(TAG, "Error in ConvertUuidToStr : %d", (int)oc_res);
		return OIC_SEC_ERROR;
	}
	THINGS_LOG_V(TAG, "MACbased UUID : %s", uuid_str);
	things_free(uuid_str);

	if (sm_secure_resource_check(&device_id) != OC_STACK_OK) {
		return OIC_SEC_ERROR;
	}

	THINGS_LOG_D(TAG, "Out %s", __func__);

	return res;
}

#if defined(CONFIG_ST_THINGS_HW_CERT_KEY) && defined(CONFIG_TLS_WITH_SSS)
static int sm_generate_artik_device_id(void)
{
	THINGS_LOG_D(TAG, "In %s", __func__);

	OicUuid_t device_id;
	unsigned char uuid_str[((UUID_LENGTH * 2) + 4 + 1)];
	unsigned int uuid_len;

	memset(uuid_str, 0, sizeof(uuid_str));
	get_artik_crt_uuid(uuid_str, &uuid_len);
	OCStackResult oc_res = ConvertStrToUuid(uuid_str, &device_id);
	if (OC_STACK_OK != oc_res) {
		THINGS_LOG_E(TAG, "Error in ConvertStrToUuid : %d", (int)oc_res);
		return OIC_SEC_ERROR;
	}
	THINGS_LOG_V(TAG, "Artik UUID : %s", uuid_str);

	if (sm_secure_resource_check(&device_id) != OC_STACK_OK) {
		return OIC_SEC_ERROR;
	}

	THINGS_LOG_D(TAG, "Out %s", __func__);

	return oc_res;
}
#endif

int sm_generate_device_id(void)
{
	int ret = -1;
#if defined(CONFIG_ST_THINGS_HW_CERT_KEY) && defined(CONFIG_TLS_WITH_SSS)
	if (dm_get_easy_setup_use_artik_crt()) {
		ret = sm_generate_artik_device_id();
	} else
#endif
	{
		ret = sm_generate_mac_based_device_id();
	}
	return ret;
}

int sm_init_things_security(int auth_type, const char *db_path)
{
	OICSecurityResult res = OIC_SEC_ERROR;

	THINGS_LOG_D(TAG, "In %s", __func__);

	memset(SVR_DB_PATH, 0x00, MAX_PATH_LEN);

	if (db_path != NULL && strlen(db_path) > 0) {
		things_strncpy(SVR_DB_PATH, db_path, MAX_PATH_LEN);
	} else {
		THINGS_LOG_E(TAG, "DB Path Not Inserted. Using Default Value");
		things_strncpy(SVR_DB_PATH, STRING_SVR_DB_PATH, MAX_PATH_LEN);
	}

	THINGS_LOG_D(TAG, "SVR DB PATH : %s", SVR_DB_PATH);

	g_auth_type = auth_type;
	if ((g_auth_type & AUTH_JUST_WORKS_MUTUAL_VERIFIED) || (g_auth_type & AUTH_CERTIFICATE_CONFIRM)) {
		SetVerifyOption(USER_CONFIRM);
	}

	g_is_mfg_cert_required = false;
	if ((g_auth_type & AUTH_CERTIFICATE) || (g_auth_type & AUTH_CERTIFICATE_CONFIRM)) {
		g_is_mfg_cert_required = true;
	}

#ifdef CONFIG_SVR_DB_SECURESTORAGE
	static OCPersistentStorage ps = { server_secure_fopen,
		secure_fread,
		secure_fwrite,
		secure_fclose,
		server_unlink };
#else
	static OCPersistentStorage ps = { server_fopen, server_fread, server_fwrite, server_fclose, server_unlink };
#endif
	res = SM_InitSvrDb(&ps);
	if (OIC_SEC_OK != res) {
		THINGS_LOG_E(TAG, "Failed to create SVR DB.");
		return res;
	}

	THINGS_LOG_V(TAG, "******* WARNING : SVR DB will be used without encryption *******");
	iotivity_api_lock();
	OCStackResult oc_res = OCRegisterPersistentStorageHandler(&ps);
	iotivity_api_unlock();
	if (OC_STACK_INCONSISTENT_DB == oc_res || OC_STACK_SVR_DB_NOT_EXIST == oc_res) {
		//If failed to load SVR DB
		THINGS_LOG_W(TAG, "SVR DB[%s] is inconsistent or not exist : %d", SVR_DB_PATH, oc_res);
		THINGS_LOG_W(TAG, "SVR DB will be reinstalled as default SVR DB.");

		//re-generate and re-install SVR DB
		if (g_is_svr_db_exist) {
			g_is_svr_db_exist = false;
			res = GenerateSvrDb(&ps);
			if (OIC_SEC_OK != res) {
				THINGS_LOG_E(TAG, "Failed to Generate SVR DB.");
				return res;
			}
		}
		res = SM_InitSvrDb();
		if (OIC_SEC_OK != res) {
			THINGS_LOG_E(TAG, "Failed to Open SVR DB.");
			return res;
		}
		//Re-register PSI
		iotivity_api_lock();
		oc_res = OCRegisterPersistentStorageHandler(&ps);
		iotivity_api_unlock();
		if (OC_STACK_OK != oc_res) {
			THINGS_LOG_E(TAG, "Failed to register persistent storage for SVR DB : %d", (int)oc_res);
			return OIC_SEC_ERROR;
		}
	} else if (OC_STACK_OK != oc_res) {
		THINGS_LOG_E(TAG, "Failed to register persistent storage for SVR DB : %d", (int)oc_res);
		return OIC_SEC_ERROR;
	}

	THINGS_LOG_D(TAG, "Out %s", __func__);

	return OIC_SEC_OK;
}

int SM_InitSvrDb(OCPersistentStorage *ps)
{
	FILE *fp = ps->open(SVR_DB_PATH, "r");
	if (fp == NULL) {
		THINGS_LOG_V(TAG, "Can not find the [%s], SVR DB will be automatically generated...", SVR_DB_PATH);
		THINGS_LOG_V(TAG, "Out %s", __func__);
		return GenerateSvrDb(ps);
	} else {
		THINGS_LOG_V(TAG, "SVR DB [%s] is already exist.", SVR_DB_PATH);
		g_is_svr_db_exist = true;
	}

	ps->close(fp);
	THINGS_LOG_V(TAG, "Out %s", __func__);

	return OIC_SEC_OK;
}

int sm_reset_svrdb(void)
{
	THINGS_LOG_D(TAG, "In %s", __func__);
	OCStackResult oc_res = ResetSecureResourceInPS();
	if (OC_STACK_OK != oc_res) {
		THINGS_LOG_E(TAG, "ResetSecureResourceInPS error : %d", oc_res);
		return OIC_SEC_ERROR;
	}
	THINGS_LOG_D(TAG, "Out %s", __func__);
	return OIC_SEC_OK;
}

//
// To test certificate based (D)TLS connection for  D2D & D2S

static OicSecKey_t primary_cert;
static OicSecKey_t primary_key;

//Samsung_OCF_RootCA.der
unsigned char g_regional_root_ca[] = {
	0x30, 0x82, 0x02, 0x5D, 0x30, 0x82, 0x02, 0x01, 0xA0, 0x03, 0x02, 0x01, 0x02, 0x02, 0x01, 0x01, 0x30, 0x0C, 0x06, 0x08,
	0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03, 0x02, 0x05, 0x00, 0x30, 0x6B, 0x31, 0x28, 0x30, 0x26, 0x06, 0x03, 0x55, 0x04,
	0x03, 0x13, 0x1F, 0x53, 0x61, 0x6D, 0x73, 0x75, 0x6E, 0x67, 0x20, 0x45, 0x6C, 0x65, 0x63, 0x74, 0x72, 0x6F, 0x6E, 0x69,
	0x63, 0x73, 0x20, 0x4F, 0x43, 0x46, 0x20, 0x52, 0x6F, 0x6F, 0x74, 0x20, 0x43, 0x41, 0x31, 0x14, 0x30, 0x12, 0x06, 0x03,
	0x55, 0x04, 0x0B, 0x13, 0x0B, 0x4F, 0x43, 0x46, 0x20, 0x52, 0x6F, 0x6F, 0x74, 0x20, 0x43, 0x41, 0x31, 0x1C, 0x30, 0x1A,
	0x06, 0x03, 0x55, 0x04, 0x0A, 0x13, 0x13, 0x53, 0x61, 0x6D, 0x73, 0x75, 0x6E, 0x67, 0x20, 0x45, 0x6C, 0x65, 0x63, 0x74,
	0x72, 0x6F, 0x6E, 0x69, 0x63, 0x73, 0x31, 0x0B, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13, 0x02, 0x4B, 0x52, 0x30,
	0x20, 0x17, 0x0D, 0x31, 0x36, 0x31, 0x31, 0x32, 0x34, 0x30, 0x32, 0x35, 0x35, 0x31, 0x31, 0x5A, 0x18, 0x0F, 0x32, 0x30,
	0x36, 0x39, 0x31, 0x32, 0x33, 0x31, 0x31, 0x34, 0x35, 0x39, 0x35, 0x39, 0x5A, 0x30, 0x6B, 0x31, 0x28, 0x30, 0x26, 0x06,
	0x03, 0x55, 0x04, 0x03, 0x13, 0x1F, 0x53, 0x61, 0x6D, 0x73, 0x75, 0x6E, 0x67, 0x20, 0x45, 0x6C, 0x65, 0x63, 0x74, 0x72,
	0x6F, 0x6E, 0x69, 0x63, 0x73, 0x20, 0x4F, 0x43, 0x46, 0x20, 0x52, 0x6F, 0x6F, 0x74, 0x20, 0x43, 0x41, 0x31, 0x14, 0x30,
	0x12, 0x06, 0x03, 0x55, 0x04, 0x0B, 0x13, 0x0B, 0x4F, 0x43, 0x46, 0x20, 0x52, 0x6F, 0x6F, 0x74, 0x20, 0x43, 0x41, 0x31,
	0x1C, 0x30, 0x1A, 0x06, 0x03, 0x55, 0x04, 0x0A, 0x13, 0x13, 0x53, 0x61, 0x6D, 0x73, 0x75, 0x6E, 0x67, 0x20, 0x45, 0x6C,
	0x65, 0x63, 0x74, 0x72, 0x6F, 0x6E, 0x69, 0x63, 0x73, 0x31, 0x0B, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13, 0x02,
	0x4B, 0x52, 0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01, 0x06, 0x08, 0x2A, 0x86, 0x48,
	0xCE, 0x3D, 0x03, 0x01, 0x07, 0x03, 0x42, 0x00, 0x04, 0x62, 0xC7, 0xFF, 0x2B, 0x1F, 0xAC, 0x50, 0x50, 0x50, 0x11, 0x26,
	0xEE, 0xCA, 0xD4, 0xC3, 0x3F, 0x02, 0xCF, 0x21, 0xED, 0x17, 0xFF, 0xCF, 0xC1, 0xD4, 0xBE, 0xDB, 0xDD, 0xA6, 0xF1, 0x13,
	0xDC, 0x34, 0x81, 0x06, 0x40, 0x7C, 0x8F, 0x16, 0x61, 0x49, 0x0A, 0x7C, 0xD7, 0xCF, 0xEC, 0x75, 0xE1, 0xD4, 0xCE, 0x52,
	0x0A, 0x73, 0xA4, 0x7F, 0x05, 0xAB, 0x6A, 0x5B, 0x46, 0x38, 0xA4, 0xDE, 0x5F, 0xA3, 0x81, 0x91, 0x30, 0x81, 0x8E, 0x30,
	0x0E, 0x06, 0x03, 0x55, 0x1D, 0x0F, 0x01, 0x01, 0xFF, 0x04, 0x04, 0x03, 0x02, 0x01, 0xC6, 0x30, 0x32, 0x06, 0x03, 0x55,
	0x1D, 0x1F, 0x04, 0x2B, 0x30, 0x29, 0x30, 0x27, 0xA0, 0x25, 0xA0, 0x23, 0x86, 0x21, 0x68, 0x74, 0x74, 0x70, 0x3A, 0x2F,
	0x2F, 0x70, 0x72, 0x6F, 0x64, 0x63, 0x61, 0x2E, 0x73, 0x61, 0x6D, 0x73, 0x75, 0x6E, 0x67, 0x69, 0x6F, 0x74, 0x73, 0x2E,
	0x63, 0x6F, 0x6D, 0x2F, 0x63, 0x72, 0x6C, 0x30, 0x0F, 0x06, 0x03, 0x55, 0x1D, 0x13, 0x01, 0x01, 0xFF, 0x04, 0x05, 0x30,
	0x03, 0x01, 0x01, 0xFF, 0x30, 0x37, 0x06, 0x08, 0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x01, 0x01, 0x04, 0x2B, 0x30, 0x29,
	0x30, 0x27, 0x06, 0x08, 0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x01, 0x86, 0x1B, 0x68, 0x74, 0x74, 0x70, 0x3A, 0x2F,
	0x2F, 0x6F, 0x63, 0x73, 0x70, 0x2E, 0x73, 0x61, 0x6D, 0x73, 0x75, 0x6E, 0x67, 0x69, 0x6F, 0x74, 0x73, 0x2E, 0x63, 0x6F,
	0x6D, 0x30, 0x0C, 0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03, 0x02, 0x05, 0x00, 0x03, 0x48, 0x00, 0x30, 0x45,
	0x02, 0x20, 0x11, 0x63, 0xD6, 0x92, 0x13, 0x7D, 0x2A, 0xDF, 0x5A, 0xB9, 0xBF, 0xC0, 0x78, 0xB0, 0x97, 0x33, 0x06, 0xA3,
	0xA9, 0xEC, 0x0B, 0x03, 0xF6, 0x8F, 0x19, 0x22, 0xE3, 0x66, 0x1F, 0xB2, 0x30, 0x4B, 0x02, 0x21, 0x00, 0xB7, 0xD1, 0xE7,
	0xA8, 0xDC, 0x5E, 0x81, 0x62, 0xB3, 0xF9, 0xC3, 0xC7, 0x4B, 0x50, 0xDB, 0x14, 0xC8, 0xFD, 0xFD, 0x1B, 0xEC, 0x5E, 0xAD,
	0x58, 0xFA, 0xA3, 0xDA, 0xFC, 0x8A, 0x41, 0xE3, 0x51
};

#ifdef CONFIG_ST_THINGS_STG_MODE
unsigned char g_regional_test_root_ca[] = {
	0x30, 0x82, 0x02, 0x68, 0x30, 0x82, 0x02, 0x0C, 0xA0, 0x03, 0x02, 0x01, 0x02, 0x02, 0x01, 0x02, 0x30, 0x0C, 0x06, 0x08,
	0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03, 0x02, 0x05, 0x00, 0x30, 0x70, 0x31, 0x2D, 0x30, 0x2B, 0x06, 0x03, 0x55, 0x04,
	0x03, 0x13, 0x24, 0x53, 0x61, 0x6D, 0x73, 0x75, 0x6E, 0x67, 0x20, 0x45, 0x6C, 0x65, 0x63, 0x74, 0x72, 0x6F, 0x6E, 0x69,
	0x63, 0x73, 0x20, 0x4F, 0x43, 0x46, 0x20, 0x52, 0x6F, 0x6F, 0x74, 0x20, 0x43, 0x41, 0x20, 0x54, 0x45, 0x53, 0x54, 0x31,
	0x14, 0x30, 0x12, 0x06, 0x03, 0x55, 0x04, 0x0B, 0x13, 0x0B, 0x4F, 0x43, 0x46, 0x20, 0x52, 0x6F, 0x6F, 0x74, 0x20, 0x43,
	0x41, 0x31, 0x1C, 0x30, 0x1A, 0x06, 0x03, 0x55, 0x04, 0x0A, 0x13, 0x13, 0x53, 0x61, 0x6D, 0x73, 0x75, 0x6E, 0x67, 0x20,
	0x45, 0x6C, 0x65, 0x63, 0x74, 0x72, 0x6F, 0x6E, 0x69, 0x63, 0x73, 0x31, 0x0B, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06,
	0x13, 0x02, 0x4B, 0x52, 0x30, 0x20, 0x17, 0x0D, 0x31, 0x36, 0x31, 0x31, 0x32, 0x34, 0x30, 0x32, 0x34, 0x37, 0x32, 0x37,
	0x5A, 0x18, 0x0F, 0x32, 0x30, 0x36, 0x39, 0x31, 0x32, 0x33, 0x31, 0x31, 0x34, 0x35, 0x39, 0x35, 0x39, 0x5A, 0x30, 0x70,
	0x31, 0x2D, 0x30, 0x2B, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x24, 0x53, 0x61, 0x6D, 0x73, 0x75, 0x6E, 0x67, 0x20, 0x45,
	0x6C, 0x65, 0x63, 0x74, 0x72, 0x6F, 0x6E, 0x69, 0x63, 0x73, 0x20, 0x4F, 0x43, 0x46, 0x20, 0x52, 0x6F, 0x6F, 0x74, 0x20,
	0x43, 0x41, 0x20, 0x54, 0x45, 0x53, 0x54, 0x31, 0x14, 0x30, 0x12, 0x06, 0x03, 0x55, 0x04, 0x0B, 0x13, 0x0B, 0x4F, 0x43,
	0x46, 0x20, 0x52, 0x6F, 0x6F, 0x74, 0x20, 0x43, 0x41, 0x31, 0x1C, 0x30, 0x1A, 0x06, 0x03, 0x55, 0x04, 0x0A, 0x13, 0x13,
	0x53, 0x61, 0x6D, 0x73, 0x75, 0x6E, 0x67, 0x20, 0x45, 0x6C, 0x65, 0x63, 0x74, 0x72, 0x6F, 0x6E, 0x69, 0x63, 0x73, 0x31,
	0x0B, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13, 0x02, 0x4B, 0x52, 0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2A, 0x86,
	0x48, 0xCE, 0x3D, 0x02, 0x01, 0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07, 0x03, 0x42, 0x00, 0x04, 0x1C,
	0xF3, 0xBA, 0xBC, 0xBB, 0xA7, 0xC1, 0xC0, 0x35, 0x59, 0xFE, 0xBF, 0x80, 0x88, 0x6B, 0x68, 0x7F, 0x47, 0xF4, 0x80, 0xB7,
	0x75, 0x55, 0xB2, 0xDF, 0xAF, 0x4E, 0xFE, 0x3F, 0x91, 0x1F, 0xA5, 0x81, 0x4D, 0x4E, 0x12, 0x24, 0xF7, 0xB7, 0xDF, 0xA6,
	0x39, 0x61, 0x3B, 0x27, 0xEA, 0x1D, 0x76, 0x94, 0x68, 0x7C, 0x55, 0xB6, 0x0D, 0xD7, 0x89, 0x92, 0x97, 0xD0, 0x51, 0x53,
	0xA7, 0xE0, 0xD3, 0xA3, 0x81, 0x92, 0x30, 0x81, 0x8F, 0x30, 0x0E, 0x06, 0x03, 0x55, 0x1D, 0x0F, 0x01, 0x01, 0xFF, 0x04,
	0x04, 0x03, 0x02, 0x01, 0xC6, 0x30, 0x2E, 0x06, 0x03, 0x55, 0x1D, 0x1F, 0x04, 0x27, 0x30, 0x25, 0x30, 0x23, 0xA0, 0x21,
	0xA0, 0x1F, 0x86, 0x1D, 0x68, 0x74, 0x74, 0x70, 0x3A, 0x2F, 0x2F, 0x63, 0x61, 0x2E, 0x73, 0x61, 0x6D, 0x73, 0x75, 0x6E,
	0x67, 0x69, 0x6F, 0x74, 0x73, 0x2E, 0x63, 0x6F, 0x6D, 0x2F, 0x63, 0x72, 0x6C, 0x30, 0x0F, 0x06, 0x03, 0x55, 0x1D, 0x13,
	0x01, 0x01, 0xFF, 0x04, 0x05, 0x30, 0x03, 0x01, 0x01, 0xFF, 0x30, 0x3C, 0x06, 0x08, 0x2B, 0x06, 0x01, 0x05, 0x05, 0x07,
	0x01, 0x01, 0x04, 0x30, 0x30, 0x2E, 0x30, 0x2C, 0x06, 0x08, 0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x01, 0x86, 0x20,
	0x68, 0x74, 0x74, 0x70, 0x3A, 0x2F, 0x2F, 0x6F, 0x63, 0x73, 0x70, 0x2D, 0x74, 0x65, 0x73, 0x74, 0x2E, 0x73, 0x61, 0x6D,
	0x73, 0x75, 0x6E, 0x67, 0x69, 0x6F, 0x74, 0x73, 0x2E, 0x63, 0x6F, 0x6D, 0x30, 0x0C, 0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE,
	0x3D, 0x04, 0x03, 0x02, 0x05, 0x00, 0x03, 0x48, 0x00, 0x30, 0x45, 0x02, 0x21, 0x00, 0x88, 0xB2, 0x2D, 0xC1, 0x70, 0xE4,
	0x0C, 0x5C, 0xEF, 0xE9, 0x0A, 0x25, 0x00, 0xF9, 0x2E, 0xF9, 0x6D, 0x81, 0x56, 0x4B, 0x6E, 0xC4, 0x18, 0x0A, 0xBD, 0x7B,
	0x61, 0x37, 0xFA, 0x14, 0x36, 0x4C, 0x02, 0x20, 0x5A, 0xB4, 0xE2, 0x78, 0x50, 0x19, 0xE7, 0x14, 0x47, 0xDC, 0x19, 0xD2,
	0x1C, 0x6F, 0x97, 0x10, 0x5D, 0x87, 0x3C, 0x3F, 0x7D, 0xCB, 0xF4, 0x98, 0x49, 0xAE, 0x93, 0xE7, 0xD6, 0x16, 0xFA, 0x31
};
#endif

/*
 * This API added as workaround to test certificate based TLS connection.
 * It will be replaced to use TZ or eSE based key protection.
 *
 * NOTE : This API should be invoked after sm_generate_mac_based_device_id invoked.
 */
#ifdef CONFIG_ST_THINGS_HW_CERT_KEY
static bool g_b_init_key;
static bool g_b_init_cert;
#endif

static OCStackResult save_signed_asymmetric_key(OicUuid_t *subject_uuid)
{
	uint16_t cred_id = 0;

	THINGS_LOG_D(TAG, "IN: %s", __func__);

	// For D2S

	/*
	 * 1. Save the Trust CA cert chain.
	 */
#ifdef CONFIG_ST_THINGS_STG_MODE
	OCStackResult res = CredSaveTrustCertChain(subject_uuid, g_regional_test_root_ca, sizeof(g_regional_test_root_ca), OIC_ENCODING_DER, TRUST_CA, &cred_id);
	if (OC_STACK_OK != res) {
		THINGS_LOG_E(TAG, "SRPCredSaveTrustCertChain #2 error");
		return res;
	}
	THINGS_LOG_D(TAG, "Samsung_OCF_Test_RootCA.der saved w/ cred ID=%d", cred_id);
#else
	OCStackResult res = CredSaveTrustCertChain(subject_uuid, (uint8_t *)g_regional_root_ca, sizeof(g_regional_root_ca), OIC_ENCODING_DER, TRUST_CA, &cred_id);
	if (OC_STACK_OK != res) {
		THINGS_LOG_E(TAG, "SRPCredSaveTrustCertChain #1 error");
		return res;
	}
	THINGS_LOG_D(TAG, "Samsung_OCF_RootCA.der saved w/ cred ID=%d", cred_id);
#endif

#ifdef CONFIG_ST_THINGS_HW_CERT_KEY
	if (dm_get_easy_setup_use_artik_crt()) {
		if (!g_b_init_key && things_sss_key_handler_init() < 0) {
			THINGS_LOG_E(TAG, "InitializeSSSKeyHandlers() Fail");
			return OC_STACK_ERROR;
		}
		g_b_init_key = true;

		if (!g_b_init_cert && things_sss_rootca_handler_init(subject_uuid) < 0) {
			THINGS_LOG_E(TAG, "SSSRootCAHandler() Fail");
			return OC_STACK_ERROR;
		}
		g_b_init_cert = true;
	} else
#endif
	{
		/*
		* 2. Save the key for D2S (primary cert & key)
		*/
#ifndef CONFIG_ST_THINGS_HW_CERT_KEY
		res = seckey_setup(dm_get_certificate_file_path(), &primary_cert, OIC_ENCODING_UNKNOW);
		if (OC_STACK_OK != res) {
			THINGS_LOG_E(TAG, "seckey_setup error");
			return res;
		}
		res = seckey_setup(dm_get_privatekey_file_path(), &primary_key, OIC_ENCODING_RAW);
		if (OC_STACK_OK != res) {
			THINGS_LOG_E(TAG, "seckey_setup error");
			return res;
		}
		res = CredSaveOwnCert(subject_uuid, &primary_cert, &primary_key, PRIMARY_CERT, &cred_id);
		if (OC_STACK_OK != res) {
			THINGS_LOG_E(TAG, "SRPCredSaveOwnCertChain error");
			return res;
		}
		THINGS_LOG_D(TAG, "Primary cert & key saved w/ cred ID=%d", cred_id);
#endif
		// For D2D
		if (g_is_mfg_cert_required) {
			/*
			* 3. Save the MFG trust CA cert chain.
			*/
#ifdef CONFIG_ST_THINGS_STG_MODE
			res = CredSaveTrustCertChain(subject_uuid, g_regional_test_root_ca, sizeof(g_regional_test_root_ca), OIC_ENCODING_DER, MF_TRUST_CA, &cred_id);
#else
			res = CredSaveTrustCertChain(subject_uuid, g_regional_root_ca, sizeof(g_regional_root_ca), OIC_ENCODING_DER, MF_TRUST_CA, &cred_id);
#endif

			if (OC_STACK_OK != res) {
				THINGS_LOG_E(TAG, "SRPCredSaveOwnCertChain error");
				return res;
			}
			THINGS_LOG_D(TAG, "MFG trust CA chain saved w/ cred ID=%d", cred_id);

			/*
			* 4. Save the key for D2D (manufacturer cert & key)
			*/
			res = CredSaveOwnCert(subject_uuid, &primary_cert, &primary_key, MF_PRIMARY_CERT, &cred_id);
			if (OC_STACK_OK != res) {
				THINGS_LOG_E(TAG, "SRPCredSaveOwnCertChain error");
				return res;
			}
			THINGS_LOG_D(TAG, "MFG primary cert & key saved w/ cred ID=%d", cred_id);
		}

		if (primary_cert.data != NULL) {
			things_free(primary_cert.data);
		}

		if (primary_key.data != NULL) {
			things_free(primary_key.data);
		}
	}

	THINGS_LOG_D(TAG, "Out: %s", __func__);
	return res;
}

int sm_save_cloud_acl(const char *cloud_uuid)
{
	THINGS_LOG_D(TAG, "IN : %s", __func__);

	if (!cloud_uuid) {
		THINGS_LOG_E(TAG, "cloud_uuid is NULL");
		return OIC_SEC_ERROR;
	}

	OicUuid_t oic_uuid;
	if (OC_STACK_OK != ConvertStrToUuid(cloud_uuid, &oic_uuid)) {
		THINGS_LOG_E(TAG, "cloud_uuid is NULL");
		return OIC_SEC_ERROR;
	}
	// allocate memory for |acl| struct
	OicSecAcl_t *acl = (OicSecAcl_t *) things_calloc(1, sizeof(OicSecAcl_t));
	if (!acl) {
		THINGS_LOG_E(TAG, " %s : things_calloc error return", __func__);
		return OIC_SEC_ERROR;
	}
	OicSecAce_t *ace = (OicSecAce_t *) things_calloc(1, sizeof(OicSecAce_t));
	if (!ace) {
		THINGS_LOG_E(TAG, "%s : things_calloc error return", __func__);
		return OIC_SEC_ERROR;
	}
	LL_APPEND(acl->aces, ace);

	memcpy(ace->subjectuuid.id, oic_uuid.id, sizeof(oic_uuid.id));

	OicSecRsrc_t *rsrc = (OicSecRsrc_t *) things_calloc(1, sizeof(OicSecRsrc_t));
	if (!rsrc) {
		THINGS_LOG_E(TAG, "%s : things_calloc error return", __func__);
		DeleteACLList(acl);
		return OIC_SEC_ERROR;
	}

	char href[] = "*";
	size_t len = strlen(href) + 1;	// '1' for null termination
	rsrc->href = (char *)things_calloc(len, sizeof(char));
	if (!rsrc->href) {
		THINGS_LOG_E(TAG, "%s : things_calloc error return", __func__);
		FreeRsrc(rsrc);
		DeleteACLList(acl);
		return OIC_SEC_ERROR;
	}
	things_strncpy(rsrc->href, href, len);

	size_t arrLen = 1;
	rsrc->typeLen = arrLen;
	rsrc->types = (char **)things_calloc(arrLen, sizeof(char *));
	if (!rsrc->types) {
		THINGS_LOG_E(TAG, "%s : things_calloc error return", __func__);
		FreeRsrc(rsrc);
		DeleteACLList(acl);
		return OIC_SEC_ERROR;
	}
	rsrc->types[0] = things_strdup("x.com.samsung.cloudconnection");	// ignore
	if (!rsrc->types[0]) {
		THINGS_LOG_E(TAG, "%s : things_strdup error return", __func__);
		FreeRsrc(rsrc);
		DeleteACLList(acl);
		return OIC_SEC_ERROR;
	}

	rsrc->interfaceLen = 1;
	rsrc->interfaces = (char **)things_calloc(arrLen, sizeof(char *));
	if (!rsrc->interfaces) {
		THINGS_LOG_E(TAG, "%s : things_calloc error return", __func__);
		FreeRsrc(rsrc);
		DeleteACLList(acl);
		return OIC_SEC_ERROR;
	}
	rsrc->interfaces[0] = things_strdup("oic.if.baseline");	// ignore
	if (!rsrc->interfaces[0]) {
		THINGS_LOG_E(TAG, "%s : things_strdup error return", __func__);
		FreeRsrc(rsrc);
		DeleteACLList(acl);
		return OIC_SEC_ERROR;
	}
	LL_APPEND(ace->resources, rsrc);

	ace->permission = 31;		// CRUDN

	OCStackResult installRes = InstallACL(acl);
	if (OC_STACK_DUPLICATE_REQUEST == installRes) {
		THINGS_LOG_W(TAG, "%s : [%s]'s ACL already installed.", __func__, cloud_uuid);
	} else if (OC_STACK_OK != installRes) {
		THINGS_LOG_E(TAG, "%s : things_strdup error return", __func__);
		/*SVACE warning fix */
		DeleteACLList(acl);
		return OIC_SEC_ERROR;
	}
	DeleteACLList(acl);

	THINGS_LOG_D(TAG, "OUT : %s", __func__);

	return OIC_SEC_OK;
}

void sm_set_otm_event_handler(OicSecOtmEventHandler_t otmEventHandler)
{
	THINGS_LOG_D(TAG, "IN : %s", __func__);

	SetOtmEventHandler(otmEventHandler);

	THINGS_LOG_D(TAG, "OUT : %s", __func__);
}
