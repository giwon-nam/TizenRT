/****************************************************************************
 *
 * Copyright 2021 Samsung Electronics All Rights Reserved.
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
#include <time.h>

#include "mbedtls/config.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/debug.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/test/certs.h"

#include <string.h>

#define SERVER_PORT "4433"
static char *SERVER_ADDR = NULL;
#define GET_REQUEST "GET / HTTP/1.0\r\n\r\n"

#define DEBUG_LEVEL 0

#define mbedtls_printf printf

static void my_debug(void *ctx, int level,
					 const char *file, int line,
					 const char *str)
{
	((void)ctx);
	((void)level);

	printf("%s:%04d: %s", file, line, str);
	fflush((FILE *)ctx);
}

static unsigned char rootca[] =
	"-----BEGIN CERTIFICATE-----\r\n"
	"MIICLTCCAdOgAwIBAgIBATAKBggqhkjOPQQDAjCBjTELMAkGA1UEBhMCS1IxLjAs\r\n"
	"BgNVBAoMJVNhbXN1bmcgRWxlY3Ryb25pY3MgRGlnaXRhbCBBcHBsaWFuY2UxHDAa\r\n"
	"BgNVBAsME0RBIFRlc3QgT0NGIFJvb3QgQ0ExMDAuBgNVBAMMJ0RBIFRlc3QgU2Ft\r\n"
	"c3VuZyBFbGVjdHJvbmljcyBPQ0YgUm9vdCBDQTAgFw0yMTA4MTMwNDIzMzBaGA8y\r\n"
	"MDY5MTIzMTA0MjMzMFowgY0xCzAJBgNVBAYTAktSMS4wLAYDVQQKDCVTYW1zdW5n\r\n"
	"IEVsZWN0cm9uaWNzIERpZ2l0YWwgQXBwbGlhbmNlMRwwGgYDVQQLDBNEQSBUZXN0\r\n"
	"IE9DRiBSb290IENBMTAwLgYDVQQDDCdEQSBUZXN0IFNhbXN1bmcgRWxlY3Ryb25p\r\n"
	"Y3MgT0NGIFJvb3QgQ0EwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAAS2H+PCRt/X\r\n"
	"7HDgY4inei2uCKsYTq5mcVsaumMHtXqNrs1LF6M0T73hcbjwdFvGhHRPYLBrJRZy\r\n"
	"UmQwhYTy0KKuoyAwHjAPBgNVHRMBAf8EBTADAQH/MAsGA1UdDwQEAwICxDAKBggq\r\n"
	"hkjOPQQDAgNIADBFAiBlSErIUCMyKg75TSXQt47WctpwO57cFy398AMl1b+RpAIh\r\n"
	"AOh+ajEBIKgHNSm6amXOCTBg40J97MBfJflm2DEHLP6v\r\n"
	"-----END CERTIFICATE-----\r\n";

static int rootca_len = sizeof(rootca);

int tls_handshake_client(char *ipaddr)
{
	mbedtls_net_context server_fd;
	uint32_t flags;
	unsigned char buf[1024];
	const char *pers = "ssl_client1";

	mbedtls_entropy_context entropy;
	mbedtls_ctr_drbg_context ctr_drbg;
	mbedtls_ssl_context ssl;
	mbedtls_ssl_config conf;
	mbedtls_x509_crt cacert;
	int ret = 1;
	int len = 0;
	struct timespec ts;
	SERVER_ADDR = ipaddr;
	ts.tv_sec = 1633074152; // 2021-10-01
	ts.tv_nsec = 0;

	clock_settime(CLOCK_REALTIME, &ts);

#if defined(MBEDTLS_DEBUG_C)
	mbedtls_debug_set_threshold(DEBUG_LEVEL);
#endif

	/*
	 * 0. Initialize the RNG and the session data
	 */
	mbedtls_net_init(&server_fd);
	mbedtls_ssl_init(&ssl);
	mbedtls_ssl_config_init(&conf);
	mbedtls_x509_crt_init(&cacert);
	mbedtls_ctr_drbg_init(&ctr_drbg);

	mbedtls_printf("\n	. Seeding the random number generator...");
	fflush(stdout);

	mbedtls_entropy_init(&entropy);
	if ((ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
									 (const unsigned char *)pers,
									 strlen(pers))) != 0) {
		mbedtls_printf(" failed\n	 ! mbedtls_ctr_drbg_seed returned %d\n", ret);
		goto exit;
	}

	mbedtls_printf(" ok\n");

	/*
		 * 0. Initialize certificates
		 */
	mbedtls_printf("	. Loading the CA root certificate ...");
	fflush(stdout);

	ret = mbedtls_x509_crt_parse(&cacert, (const unsigned char *)rootca,
								 rootca_len);
	if (ret < 0) {
		mbedtls_printf(" failed\n	 !	mbedtls_x509_crt_parse returned -0x%x\n\n", (unsigned int)-ret);
		goto exit;
	}

	mbedtls_printf(" ok (%d skipped)\n", ret);

	/*
		 * 1. Start the connection
		 */
	mbedtls_printf("	. Connecting to tcp/%s/%s...", SERVER_ADDR, SERVER_PORT);
	fflush(stdout);

	if ((ret = mbedtls_net_connect(&server_fd, SERVER_ADDR,
								   SERVER_PORT, MBEDTLS_NET_PROTO_TCP)) != 0) {
		mbedtls_printf(" failed\n	 ! mbedtls_net_connect returned %d\n\n", ret);
		goto exit;
	}

	mbedtls_printf(" ok\n");

	/*
		 * 2. Setup stuff
		 */
	mbedtls_printf("	. Setting up the SSL/TLS structure...");
	fflush(stdout);

	if ((ret = mbedtls_ssl_config_defaults(&conf,
										   MBEDTLS_SSL_IS_CLIENT,
										   MBEDTLS_SSL_TRANSPORT_STREAM,
										   MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
		mbedtls_printf(" failed\n	 ! mbedtls_ssl_config_defaults returned %d\n\n", ret);
		goto exit;
	}

	mbedtls_printf(" ok\n");

	/* OPTIONAL is not optimal for security,
		 * but makes interop easier in this simplified example */
	mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_REQUIRED);
	mbedtls_ssl_conf_ca_chain(&conf, &cacert, NULL);
	mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
	mbedtls_ssl_conf_dbg(&conf, my_debug, stdout);

	if ((ret = mbedtls_ssl_setup(&ssl, &conf)) != 0) {
		mbedtls_printf(" failed\n	 ! mbedtls_ssl_setup returned %d\n\n", ret);
		goto exit;
	}

	mbedtls_ssl_set_bio(&ssl, &server_fd, mbedtls_net_send, mbedtls_net_recv, NULL);

	/*
		 * 4. Handshake
		 */
	mbedtls_printf("	. Performing the SSL/TLS handshake...");
	fflush(stdout);

	while ((ret = mbedtls_ssl_handshake(&ssl)) != 0) {
		if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
			mbedtls_printf(" failed\n	 ! mbedtls_ssl_handshake returned -0x%x\n\n", (unsigned int)-ret);
			goto exit;
		}
	}

	mbedtls_printf(" ok\n");

	/*
		 * 5. Verify the server certificate
		 */
	mbedtls_printf("	. Verifying peer X.509 certificate...");

	/* In real life, we probably want to bail out when ret != 0 */
	if ((flags = mbedtls_ssl_get_verify_result(&ssl)) != 0) {
		char vrfy_buf[512];

		mbedtls_printf(" failed\n");

		mbedtls_x509_crt_verify_info(vrfy_buf, sizeof(vrfy_buf), "	! ", flags);

		mbedtls_printf("%s\n", vrfy_buf);
	} else
		mbedtls_printf(" ok\n");

	/*
		 * 3. Write the GET request
		 */
	mbedtls_printf("	> Write to server:");
	fflush(stdout);

	len = sprintf((char *)buf, GET_REQUEST);

	while ((ret = mbedtls_ssl_write(&ssl, buf, len)) <= 0) {
		if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
			mbedtls_printf(" failed\n	 ! mbedtls_ssl_write returned %d\n\n", ret);
			goto exit;
		}
	}

	len = ret;
	mbedtls_printf(" %d bytes written\n\n%s", len, (char *)buf);

	/*
		 * 7. Read the HTTP response
		 */
	mbedtls_printf("	< Read from server:");
	fflush(stdout);

	do {
		len = sizeof(buf) - 1;
		memset(buf, 0, sizeof(buf));
		ret = mbedtls_ssl_read(&ssl, buf, len);

		if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
			continue;
		}
		if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
			ret = 0;
			break;
		}
		if (ret < 0) {
			mbedtls_printf("failed\n	! mbedtls_ssl_read returned %d\n\n", ret);
			break;
		}
		if (ret == 0) {
			mbedtls_printf("\n\nEOF\n\n");
			break;
		}

		len = ret;
		mbedtls_printf(" %d bytes read\n\n%s", len, (char *)buf);
	} while (1);

	mbedtls_ssl_close_notify(&ssl);

exit:

#ifdef MBEDTLS_ERROR_C
	if (ret != -1) {
		char error_buf[100];
		mbedtls_strerror(ret, error_buf, 100);
		mbedtls_printf("Last error was: %d - %s\n\n", ret, error_buf);
	}
#endif

	mbedtls_net_free(&server_fd);

	mbedtls_x509_crt_free(&cacert);
	mbedtls_ssl_free(&ssl);
	mbedtls_ssl_config_free(&conf);
	mbedtls_ctr_drbg_free(&ctr_drbg);
	mbedtls_entropy_free(&entropy);

	return 0;
}
