/****************************************************************************
 *
 * Copyright 2019 Samsung Electronics All Rights Reserved.
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
#include <tinyara/config.h>
#include <tinyara/lwnl/lwnl80211.h>

#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <errno.h>
#include <debug.h>

#include "wifi_utils.h"

#define HALF_SECOND_USEC_USEC   500000L

#define WU_TAG "[WU]"

#define WU_ERR                                                                  \
	do {                                                                        \
		ndbg(WU_TAG"[ERR] %s:%d code(%s)\n",                                    \
			   __FILE__, __LINE__, strerror(errno));                            \
	} while (0)

#define WU_ERR_FD(fd)                                                           \
	do {                                                                        \
		ndbg(WU_TAG"[ERR] %s:%d fd(%d) code(%s)\n",                             \
			   __FILE__, __LINE__, fd, strerror(errno));                        \
	} while (0)

#define WU_CHECK_ERR(arg)                                    \
	do {                                                     \
		if (arg < 0) {                                       \
			WU_ERR;                                          \
			return WIFI_UTILS_FAIL;                          \
		}                                                    \
	} while (0)

#define WU_ENTER                                                                \
	do {                                                                        \
		ndbg(WU_TAG"%s:%d\n", __FILE__, __LINE__);                              \
	} while (0)

#define WU_CALL(fd, code, param)                                                \
	do {                                                                        \
		int res = ioctl(fd, code, (unsigned long)((uintptr_t)&param));          \
		if (res < 0) {                                                          \
			WU_CLOSE(fd);                                                       \
			WU_ERR_FD(fd);                                                      \
			return WIFI_UTILS_FAIL;                                             \
		}                                                                       \
	} while (0)

#define WU_CALL_ERROUT(fd, code, param)                                         \
	do {                                                                        \
		int res = ioctl(fd, code, (unsigned long)((uintptr_t)&param));          \
		if (res < 0) {                                                          \
			WU_ERR;                                                             \
			ret = WIFI_UTILS_FAIL;                                              \
			goto errout;                                                        \
		}                                                                       \
	} while (0)

#define WU_CLOSE(fd)                            \
	do {                                        \
		close(fd);                              \
	} while (0)

static wifi_utils_cb_s g_cbk = {NULL, NULL, NULL, NULL, NULL};

struct _wifi_utils_s g_lwnl_hnd = {"", -1};

sem_t g_lwnl_signal;

static void close_cb_handler(void)
{
	lwnl80211_cb_data data_s = {LWNL80211_EXIT, .u.data = NULL, 0, 0};
	mqd_t mqfd;
	struct mq_attr attr;
	int sret;
	attr.mq_maxmsg = LWNL80211_MQUEUE_MAX_DATA_NUM;
	attr.mq_msgsize = sizeof(lwnl80211_cb_data);
	attr.mq_flags = 0;
	attr.mq_curmsgs = 0;

	mqfd = mq_open(g_lwnl_hnd.mqname, O_RDWR | O_CREAT, 0666, &attr);
	if (mqfd == NULL) {
		ndbg("Failed to open mq\n");
		return;
	}
	sret = mq_send(mqfd, (const char *)&data_s, sizeof(data_s), 100);
	if (sret < 0) {
		ndbg("Failed to send msg to mq\n");
		mq_close(mqfd);
		return;
	}
	sem_wait(&g_lwnl_signal);

	mq_close(mqfd);
	mq_unlink(g_lwnl_hnd.mqname);
	g_lwnl_hnd.cb_receiver = -1;
}

static void free_scan_data(wifi_utils_scan_list_s *scan_list)
{
	wifi_utils_scan_list_s *cur = scan_list;
	wifi_utils_scan_list_s *prev = NULL;
	while (cur) {
		prev = cur;
		cur = cur->next;
		free(prev);
	}
	scan_list = NULL;
}

static wifi_utils_result_e receive_scan_data(mqd_t mq, wifi_utils_scan_list_s *scan_list)
{
	int ret;
	int prio;
	int nbytes;
	int cnt = 0;
	int msglen = sizeof(lwnl80211_cb_data);
	wifi_utils_scan_list_s *prev = scan_list;

	while (true) {
		lwnl80211_cb_data msg;
		wifi_utils_scan_list_s *cur = NULL;
		nbytes = mq_receive(mq, (char *)&msg, msglen, &prio);
		if (nbytes < 0 || nbytes != msglen) {
			ndbg("Failed to receive (nbytes=%d, msglen=%d)\n", nbytes, msglen);
			WU_ERR;
			ret = WIFI_UTILS_FAIL;
			break;
		}

		if (msg.status != LWNL80211_SCAN_DONE) {
			WU_ERR;
			ret =  WIFI_UTILS_FAIL;
			break;
		}

		cur = (wifi_utils_scan_list_s *)malloc(sizeof(wifi_utils_scan_list_s));
		if (!cur) {
			free_scan_data(scan_list);
			ret = WIFI_UTILS_FAIL;
			break;
		}
		cur->next = NULL;
		memcpy(&(cur->ap_info), &(msg.u.ap_info), sizeof(wifi_utils_ap_scan_info_s));
		cnt++;

		prev->next = cur;
		prev = cur;

		if (msg.md == 0 || cnt >= LWNL80211_MQUEUE_MAX_DATA_NUM) {
			ndbg("End of scanning (%d data)\n", cnt);
			ret = WIFI_UTILS_SUCCESS;
			break;
		}
	}

	return ret;
}

int wifi_utils_callback_handler(int argc, char *argv[])
{
	WU_ENTER;

	int prio;
	int terminate = 0;
	int nbytes;
	lwnl80211_cb_data msg;
	int msglen = sizeof(lwnl80211_cb_data);
	mqd_t mqfd;
	struct mq_attr attr;
	attr.mq_maxmsg = LWNL80211_MQUEUE_MAX_DATA_NUM;
	attr.mq_msgsize = sizeof(lwnl80211_cb_data);
	attr.mq_flags = 0;
	attr.mq_curmsgs = 0;

	mqfd = mq_open(g_lwnl_hnd.mqname, O_RDWR | O_CREAT, 0666, &attr);
	if (mqfd == NULL) {
		ndbg("Failed to open mq\n");
		sem_post(&g_lwnl_signal);
		return -1;
	}

	sem_post(&g_lwnl_signal);

	while (true) {
		if (terminate) {
			break;
		}
		nbytes = mq_receive(mqfd, (char *)&msg, msglen, &prio);
		if (nbytes < 0 || nbytes != msglen) {
			ndbg("Failed to receive (nbytes=%d, msglen=%d)\n", nbytes, msglen);
			WU_ERR;
			break;
		}

		switch (msg.status) {
		case LWNL80211_STA_CONNECTED:
			g_cbk.sta_connected(WIFI_UTILS_SUCCESS, NULL);
			break;
		case LWNL80211_STA_CONNECT_FAILED:
			g_cbk.sta_connected(WIFI_UTILS_FAIL, NULL);
			break;
		case LWNL80211_STA_DISCONNECTED:
			g_cbk.sta_disconnected(NULL);
			break;
		case LWNL80211_SOFTAP_STA_JOINED:
			g_cbk.softap_sta_joined(NULL);
			break;
		case LWNL80211_SOFTAP_STA_LEFT:
			g_cbk.softap_sta_left(NULL);
			break;
		case LWNL80211_SCAN_FAILED:
			g_cbk.scan_done(WIFI_UTILS_FAIL, NULL, NULL);
			break;
		case LWNL80211_SCAN_DONE:
		{
			wifi_utils_scan_list_s *scan_list;
			scan_list = (wifi_utils_scan_list_s *)malloc(sizeof(wifi_utils_scan_list_s));
			scan_list->next = NULL;
			if (!scan_list) {
				free_scan_data(scan_list);
				WU_ERR;
				break;
			}
			memcpy(&(scan_list->ap_info), &(msg.u.ap_info), sizeof(wifi_utils_ap_scan_info_s));
			if (msg.md) {
				int ret = receive_scan_data(mqfd, scan_list);
				if (ret == WIFI_UTILS_SUCCESS) {
					g_cbk.scan_done(WIFI_UTILS_SUCCESS, scan_list, NULL);
				}
				free_scan_data(scan_list);
			} else {
				g_cbk.scan_done(WIFI_UTILS_SUCCESS, scan_list, NULL);
				free(&(scan_list->ap_info));
			}
			break;
		}
		case LWNL80211_EXIT:
			nvdbg("Terminate this thread (%d)\n", msg.status);
			terminate = 1;
			break;
		default:
			ndbg("Bad status received (%d)\n", msg.status);
			WU_ERR;
			terminate = 1;
			return -1;
			break;
		}
	}

	mq_close(mqfd);

	sem_post(&g_lwnl_signal);

	return 0;
}

wifi_utils_result_e wifi_utils_init(void)
{
	WU_ENTER;

	wifi_utils_result_e ret = WIFI_UTILS_FAIL;

	int fd = open(LWNL80211_PATH, O_RDWR);
	WU_CHECK_ERR(fd);

	sem_init(&g_lwnl_signal, 0, 0);

	snprintf(g_lwnl_hnd.mqname, sizeof(g_lwnl_hnd.mqname), "%01x", (unsigned long)((uintptr_t)&g_lwnl_hnd));

	g_lwnl_hnd.cb_receiver = task_create("lwnl8021 cb handler", 110, 4096, (main_t)wifi_utils_callback_handler, NULL);

	sem_wait(&g_lwnl_signal);

	/* Start to send ioctl */

	lwnl80211_data data_in = {NULL, 0, 0};
	data_in.data_len = sizeof(g_lwnl_hnd.mqname);
	data_in.data = (mqd_t *)malloc(data_in.data_len);
	if (!data_in.data) {
		ndbg("Failed to alloc lw80211_data input\n");
		ret = WIFI_UTILS_FAIL;
		goto errout;
	}
	memcpy(data_in.data, g_lwnl_hnd.mqname, data_in.data_len);

	WU_CALL_ERROUT(fd, LWNL80211_REGISTERMQ, data_in);
	free(data_in.data);
	data_in.data = NULL;
	data_in.data_len = 0;
	data_in.res = 0;

	WU_CALL_ERROUT(fd, LWNL80211_INIT, data_in);

	WU_CLOSE(fd);
	return WIFI_UTILS_SUCCESS;

errout:
	WU_CLOSE(fd);
	if (data_in.data) {
		free(data_in.data);
	}
	close_cb_handler();
	return ret;
}

wifi_utils_result_e wifi_utils_deinit(void)
{
	WU_ENTER;

	int fd = open(LWNL80211_PATH, O_RDWR);
	WU_CHECK_ERR(fd);

	lwnl80211_data data_in = {NULL, 0, 0};

	WU_CALL(fd, LWNL80211_DEINIT, data_in);

	WU_CALL(fd, LWNL80211_UNREGISTERMQ, data_in);

	g_cbk = (wifi_utils_cb_s){NULL, NULL, NULL, NULL, NULL};

	WU_CLOSE(fd);

	close_cb_handler();

	sem_destroy(&g_lwnl_signal);

	return WIFI_UTILS_SUCCESS;
}

wifi_utils_result_e wifi_utils_scan_ap(void *arg)
{
	WU_ENTER;

	int fd = open(LWNL80211_PATH, O_RDWR);
	WU_CHECK_ERR(fd);

	lwnl80211_data data_in = {NULL, 0, 0};

	WU_CALL(fd, LWNL80211_SCAN_AP, data_in);

	WU_CLOSE(fd);

	return WIFI_UTILS_SUCCESS;
}


wifi_utils_result_e wifi_utils_register_callback(wifi_utils_cb_s *cbk)
{
	wifi_utils_result_e wuret = WIFI_UTILS_INVALID_ARGS;
	if (cbk) {
		g_cbk = *cbk;
		wuret = WIFI_UTILS_SUCCESS;
	} else {
		ndbg("WiFi callback register failure (no callback)\n");
	}
	return wuret;
}

wifi_utils_result_e wifi_utils_connect_ap(wifi_utils_ap_config_s *ap_connect_config, void *arg)
{
	WU_ENTER;

	int fd = open(LWNL80211_PATH, O_RDWR);
	WU_CHECK_ERR(fd);

	lwnl80211_data data_in = {(void *)ap_connect_config, sizeof(wifi_utils_ap_config_s), 0};

	WU_CALL(fd, LWNL80211_CONNECT_AP, data_in);

	WU_CLOSE(fd);

	return WIFI_UTILS_SUCCESS;
}

wifi_utils_result_e wifi_utils_disconnect_ap(void *arg)
{
	WU_ENTER;

	int fd = open(LWNL80211_PATH, O_RDWR);
	WU_CHECK_ERR(fd);

	lwnl80211_data data_in = {NULL, 0, 0};

	WU_CALL(fd, LWNL80211_DISCONNECT_AP, data_in);

	WU_CLOSE(fd);

	return WIFI_UTILS_SUCCESS;
}

wifi_utils_result_e wifi_utils_get_info(wifi_utils_info_s *wifi_info)
{
	WU_ENTER;

	int fd = open(LWNL80211_PATH, O_RDWR);
	WU_CHECK_ERR(fd);

	lwnl80211_data data_in = {(void *)wifi_info, sizeof(wifi_utils_info_s), 0};

	WU_CALL(fd, LWNL80211_GET_INFO, data_in);

	WU_CLOSE(fd);

	return WIFI_UTILS_SUCCESS;
}

wifi_utils_result_e wifi_utils_start_softap(wifi_utils_softap_config_s *softap_config)
{
	WU_ENTER;

	int fd = open(LWNL80211_PATH, O_RDWR);
	WU_CHECK_ERR(fd);

	lwnl80211_data data_in = {(void *)softap_config, sizeof(wifi_utils_softap_config_s), 0};

	WU_CALL(fd, LWNL80211_START_SOFTAP, data_in);

	WU_CLOSE(fd);

	return WIFI_UTILS_SUCCESS;
}

wifi_utils_result_e wifi_utils_start_sta(void)
{
	WU_ENTER;

	int fd = open(LWNL80211_PATH, O_RDWR);
	WU_CHECK_ERR(fd);

	lwnl80211_data data_in = {NULL, 0, 0};

	WU_CALL(fd, LWNL80211_START_STA, data_in);

	WU_CLOSE(fd);

	return WIFI_UTILS_SUCCESS;
}

wifi_utils_result_e wifi_utils_stop_softap(void)
{
	WU_ENTER;

	int fd = open(LWNL80211_PATH, O_RDWR);
	WU_CHECK_ERR(fd);

	lwnl80211_data data_in = {NULL, 0, 0};

	WU_CALL(fd, LWNL80211_STOP_SOFTAP, data_in);

	WU_CLOSE(fd);

	return WIFI_UTILS_SUCCESS;
}

wifi_utils_result_e wifi_utils_set_autoconnect(uint8_t check)
{
	WU_ENTER;

	int fd = open(LWNL80211_PATH, O_RDWR);
	WU_CHECK_ERR(fd);

	uint8_t *chk = &check;
	lwnl80211_data data_in = {(void *)chk, sizeof(uint8_t), 0};

	WU_CALL(fd, LWNL80211_SET_AUTOCONNECT, data_in);

	WU_CLOSE(fd);

	return WIFI_UTILS_SUCCESS;
}

