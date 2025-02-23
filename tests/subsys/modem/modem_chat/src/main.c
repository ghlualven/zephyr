/*
 * Copyright (c) 2022 Trackunit Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*************************************************************************************************/
/*                                        Dependencies                                           */
/*************************************************************************************************/
#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <string.h>

#include <zephyr/modem/chat.h>
#include <modem_backend_mock.h>

/*************************************************************************************************/
/*                                         Instances                                             */
/*************************************************************************************************/
static struct modem_chat cmd;
static uint8_t cmd_delimiter[] = {'\r', '\n'};
static uint8_t cmd_receive_buf[128];
static uint8_t *cmd_argv[32];
static uint32_t cmd_user_data = 0x145212;

static struct modem_backend_mock mock;
static uint8_t mock_rx_buf[128];
static uint8_t mock_tx_buf[128];
static struct modem_pipe *mock_pipe;

/*************************************************************************************************/
/*                                        Track callbacks                                        */
/*************************************************************************************************/
#define MODEM_CHAT_UTEST_ON_IMEI_CALLED_BIT		 (0)
#define MODEM_CHAT_UTEST_ON_CREG_CALLED_BIT		 (1)
#define MODEM_CHAT_UTEST_ON_CGREG_CALLED_BIT		 (2)
#define MODEM_CHAT_UTEST_ON_QENG_SERVINGCELL_CALLED_BIT	 (3)
#define MODEM_CHAT_UTEST_ON_NO_CARRIER_CALLED_BIT	 (4)
#define MODEM_CHAT_UTEST_ON_ERROR_CALLED_BIT		 (5)
#define MODEM_CHAT_UTEST_ON_RDY_CALLED_BIT		 (6)
#define MODEM_CHAT_UTEST_ON_APP_RDY_CALLED_BIT		 (7)
#define MODEM_CHAT_UTEST_ON_NORMAL_POWER_DOWN_CALLED_BIT (8)
#define MODEM_CHAT_UTEST_ON_SCRIPT_CALLBACK_BIT		 (9)
#define MODEM_CHAT_UTEST_ON_CMGL_PARTIAL_CALLED_BIT	 (10)
#define MODEM_CHAT_UTEST_ON_CMGL_PARTIAL_ANY_CALLED_BIT	 (11)

static atomic_t callback_called;

/*************************************************************************************************/
/*                                  Script callbacks args copy                                   */
/*************************************************************************************************/
static uint8_t argv_buffers[32][128];
static uint16_t argc_buffers;

static void clone_args(char **argv, uint16_t argc)
{
	argc_buffers = argc;

	for (uint16_t i = 0; i < argc; i++) {
		memcpy(argv_buffers[i], argv[i], strlen(argv[i]) + 1);
	}
}

/*************************************************************************************************/
/*                                   Script match callbacks                                      */
/*************************************************************************************************/
static void on_imei(struct modem_chat *cmd, char **argv, uint16_t argc, void *user_data)
{
	atomic_set_bit(&callback_called, MODEM_CHAT_UTEST_ON_IMEI_CALLED_BIT);
	clone_args(argv, argc);
}

static void on_creg(struct modem_chat *cmd, char **argv, uint16_t argc, void *user_data)
{
	atomic_set_bit(&callback_called, MODEM_CHAT_UTEST_ON_CREG_CALLED_BIT);
	clone_args(argv, argc);
}

static void on_cgreg(struct modem_chat *cmd, char **argv, uint16_t argc, void *user_data)
{
	atomic_set_bit(&callback_called, MODEM_CHAT_UTEST_ON_CGREG_CALLED_BIT);
	clone_args(argv, argc);
}

static void on_qeng_serving_cell(struct modem_chat *cmd, char **argv, uint16_t argc,
				 void *user_data)
{
	atomic_set_bit(&callback_called, MODEM_CHAT_UTEST_ON_QENG_SERVINGCELL_CALLED_BIT);
	clone_args(argv, argc);
}

static void on_no_carrier(struct modem_chat *cmd, char **argv, uint16_t argc, void *user_data)
{
	atomic_set_bit(&callback_called, MODEM_CHAT_UTEST_ON_NO_CARRIER_CALLED_BIT);
	clone_args(argv, argc);
}

static void on_error(struct modem_chat *cmd, char **argv, uint16_t argc, void *user_data)
{
	atomic_set_bit(&callback_called, MODEM_CHAT_UTEST_ON_ERROR_CALLED_BIT);
	clone_args(argv, argc);
}

static void on_rdy(struct modem_chat *cmd, char **argv, uint16_t argc, void *user_data)
{
	atomic_set_bit(&callback_called, MODEM_CHAT_UTEST_ON_RDY_CALLED_BIT);
	clone_args(argv, argc);
}

static void on_app_rdy(struct modem_chat *cmd, char **argv, uint16_t argc, void *user_data)
{
	atomic_set_bit(&callback_called, MODEM_CHAT_UTEST_ON_APP_RDY_CALLED_BIT);
	clone_args(argv, argc);
}

static void on_normal_power_down(struct modem_chat *cmd, char **argv, uint16_t argc,
				 void *user_data)
{
	atomic_set_bit(&callback_called, MODEM_CHAT_UTEST_ON_NORMAL_POWER_DOWN_CALLED_BIT);
	clone_args(argv, argc);
}

static void on_cmgl_partial(struct modem_chat *cmd, char **argv, uint16_t argc, void *user_data)
{
	atomic_set_bit(&callback_called, MODEM_CHAT_UTEST_ON_CMGL_PARTIAL_CALLED_BIT);
	clone_args(argv, argc);
}

static void on_cmgl_any_partial(struct modem_chat *cmd, char **argv, uint16_t argc,
				void *user_data)
{
	atomic_set_bit(&callback_called, MODEM_CHAT_UTEST_ON_CMGL_PARTIAL_ANY_CALLED_BIT);
	clone_args(argv, argc);
}

/*************************************************************************************************/
/*                                       Script callback                                         */
/*************************************************************************************************/
static enum modem_chat_script_result script_result;
static void *script_result_user_data;

static void on_script_result(struct modem_chat *cmd, enum modem_chat_script_result result,
			     void *user_data)
{
	atomic_set_bit(&callback_called, MODEM_CHAT_UTEST_ON_SCRIPT_CALLBACK_BIT);
	script_result = result;
	script_result_user_data = user_data;
}

/*************************************************************************************************/
/*                                            Script                                             */
/*************************************************************************************************/
MODEM_CHAT_MATCH_DEFINE(ok_match, "OK", "", NULL);
MODEM_CHAT_MATCH_DEFINE(imei_match, "", "", on_imei);
MODEM_CHAT_MATCH_DEFINE(creg_match, "CREG: ", ",", on_creg);
MODEM_CHAT_MATCH_DEFINE(cgreg_match, "CGREG: ", ",", on_cgreg);
MODEM_CHAT_MATCH_DEFINE(qeng_servinc_cell_match, "+QENG: \"servingcell\",", ",",
			on_qeng_serving_cell);

MODEM_CHAT_MATCHES_DEFINE(unsol_matches, MODEM_CHAT_MATCH("RDY", "", on_rdy),
			  MODEM_CHAT_MATCH("APP RDY", "", on_app_rdy),
			  MODEM_CHAT_MATCH("NORMAL POWER DOWN", "", on_normal_power_down));

MODEM_CHAT_SCRIPT_CMDS_DEFINE(
	script_cmds, MODEM_CHAT_SCRIPT_CMD_RESP("AT", ok_match),
	MODEM_CHAT_SCRIPT_CMD_RESP("ATE0", ok_match),
	MODEM_CHAT_SCRIPT_CMD_RESP("IMEI?", imei_match), MODEM_CHAT_SCRIPT_CMD_RESP("", ok_match),
	MODEM_CHAT_SCRIPT_CMD_RESP("AT+CREG?;+CGREG?", creg_match),
	MODEM_CHAT_SCRIPT_CMD_RESP("", cgreg_match), MODEM_CHAT_SCRIPT_CMD_RESP("", ok_match),
	MODEM_CHAT_SCRIPT_CMD_RESP("AT+QENG=\"servingcell\"", qeng_servinc_cell_match),
	MODEM_CHAT_SCRIPT_CMD_RESP("", ok_match));

MODEM_CHAT_MATCHES_DEFINE(abort_matches, MODEM_CHAT_MATCH("NO CARRIER", "", on_no_carrier),
			  MODEM_CHAT_MATCH("ERROR ", ",:", on_error));

MODEM_CHAT_SCRIPT_DEFINE(script, script_cmds, abort_matches, on_script_result, 4);

/*************************************************************************************************/
/*                             Script implementing partial matches                               */
/*************************************************************************************************/
MODEM_CHAT_MATCHES_DEFINE(
	cmgl_matches,
	MODEM_CHAT_MATCH_INITIALIZER("+CMGL: ", ",", on_cmgl_partial, false, true),
	MODEM_CHAT_MATCH_INITIALIZER("", "", on_cmgl_any_partial, false, true),
	MODEM_CHAT_MATCH_INITIALIZER("OK", "", NULL, false, false)
);

MODEM_CHAT_SCRIPT_CMDS_DEFINE(
	script_partial_cmds,
	MODEM_CHAT_SCRIPT_CMD_RESP_MULT("AT+CMGL=4", cmgl_matches),
);

MODEM_CHAT_SCRIPT_DEFINE(script_partial, script_partial_cmds, abort_matches, on_script_result, 4);

/*************************************************************************************************/
/*                                      Script responses                                         */
/*************************************************************************************************/
static const char at_response[] = "AT\r\n";
static const char ok_response[] = "OK\r\n";
static const char imei_response[] = "23412354123123\r\n";
static const char creg_response[] = "CREG: 1,2\r\n";
static const char cgreg_response[] = "CGREG: 10,43\r\n";

static const char qeng_servinc_cell_response[] = "+QENG: \"servingcell\",\"NOCONN\",\"GSM\",260"
						 ",03,E182,AEAD,52,32,2,-68,255,255,0,38,38,1,,"
						 ",,,,,,,,\r\n";

static const char cmgl_response_0[] = "+CMGL: 1,1,,50\r\n";
static const char cmgl_response_1[] = "07911326060032F064A9542954\r\n";

/*************************************************************************************************/
/*                                         Test setup                                            */
/*************************************************************************************************/
static void *test_modem_chat_setup(void)
{
	const struct modem_chat_config cmd_config = {
		.user_data = &cmd_user_data,
		.receive_buf = cmd_receive_buf,
		.receive_buf_size = ARRAY_SIZE(cmd_receive_buf),
		.delimiter = cmd_delimiter,
		.delimiter_size = ARRAY_SIZE(cmd_delimiter),
		.filter = NULL,
		.filter_size = 0,
		.argv = cmd_argv,
		.argv_size = ARRAY_SIZE(cmd_argv),
		.unsol_matches = unsol_matches,
		.unsol_matches_size = ARRAY_SIZE(unsol_matches),
		.process_timeout = K_MSEC(2),
	};

	zassert(modem_chat_init(&cmd, &cmd_config) == 0, "Failed to init modem CMD");

	const struct modem_backend_mock_config mock_config = {
		.rx_buf = mock_rx_buf,
		.rx_buf_size = ARRAY_SIZE(mock_rx_buf),
		.tx_buf = mock_tx_buf,
		.tx_buf_size = ARRAY_SIZE(mock_tx_buf),
		.limit = 8,
	};

	mock_pipe = modem_backend_mock_init(&mock, &mock_config);
	zassert(modem_pipe_open(mock_pipe) == 0, "Failed to open mock pipe");
	zassert(modem_chat_attach(&cmd, mock_pipe) == 0, "Failed to attach pipe mock to modem CMD");
	return NULL;
}

static void test_modem_chat_before(void *f)
{
	/* Reset callback called */
	atomic_set(&callback_called, 0);

	/* Reset mock pipe */
	modem_backend_mock_reset(&mock);
}

static void test_modem_chat_after(void *f)
{
	/* Abort script */
	modem_chat_script_abort(&cmd);

	k_msleep(100);
}

/*************************************************************************************************/
/*                                          Buffers                                              */
/*************************************************************************************************/
static uint8_t buffer[4096];

/*************************************************************************************************/
/*                                           Tests                                               */
/*************************************************************************************************/
ZTEST(modem_chat, test_script_no_error)
{
	bool called;

	zassert_true(modem_chat_script_run(&cmd, &script) == 0, "Failed to start script");
	k_msleep(100);

	/*
	 * Script sends "AT\r\n"
	 * Modem responds "AT\r\n"
	 * Modem responds "OK\r\n"
	 */

	modem_backend_mock_get(&mock, buffer, ARRAY_SIZE(buffer));
	zassert_true(memcmp(buffer, "AT\r", sizeof("AT\r") - 1) == 0,
		     "Request not sent as expected");

	modem_backend_mock_put(&mock, at_response, sizeof(at_response) - 1);
	modem_backend_mock_put(&mock, ok_response, sizeof(ok_response) - 1);

	k_msleep(100);

	/*
	 * Script sends "ATE0\r\n"
	 * Modem responds "OK\r\n"
	 */

	modem_backend_mock_get(&mock, buffer, ARRAY_SIZE(buffer));
	zassert_true(memcmp(buffer, "ATE0\r\n", sizeof("ATE0\r\n") - 1) == 0,
		     "Request not sent as expected");

	modem_backend_mock_put(&mock, ok_response, sizeof(ok_response) - 1);

	k_msleep(100);

	/*
	 * Script sends "IMEI?\r\n"
	 * Modem responds "23412354123123\r\n"
	 * Modem responds "OK\r\n"
	 */

	modem_backend_mock_get(&mock, buffer, ARRAY_SIZE(buffer));
	zassert_true(memcmp(buffer, "IMEI?\r\n", sizeof("IMEI?\r\n") - 1) == 0,
		     "Request not sent as expected");

	modem_backend_mock_put(&mock, imei_response, sizeof(imei_response) - 1);
	modem_backend_mock_put(&mock, ok_response, sizeof(ok_response) - 1);

	k_msleep(100);

	zassert_true(atomic_test_bit(&callback_called, MODEM_CHAT_UTEST_ON_IMEI_CALLED_BIT) == true,
		     "Expected IMEI callback called");

	zassert_true(argv_buffers[0][0] == '\0', "Unexpected argv");
	zassert_true(memcmp(argv_buffers[1], "23412354123123", sizeof("23412354123123")) == 0,
		     "Unexpected argv");

	zassert_true(argc_buffers == 2, "Unexpected argc");

	/*
	 * Script sends "AT+CREG?;+CGREG?\r\n"
	 * Modem responds "CREG: 1,2\r\n"
	 * Modem responds "CGREG: 1,2\r\n"
	 * Modem responds "OK\r\n"
	 */

	modem_backend_mock_get(&mock, buffer, ARRAY_SIZE(buffer));
	zassert_true(memcmp(buffer, "AT+CREG?;+CGREG?\r\n", sizeof("AT+CREG?;+CGREG?\r\n") - 1) ==
			     0,
		     "Request not sent as expected");

	modem_backend_mock_put(&mock, creg_response, sizeof(creg_response) - 1);

	k_msleep(100);

	zassert_true(memcmp(argv_buffers[0], "CREG: ", sizeof("CREG: ")) == 0, "Unexpected argv");
	zassert_true(memcmp(argv_buffers[1], "1", sizeof("1")) == 0, "Unexpected argv");
	zassert_true(memcmp(argv_buffers[2], "2", sizeof("2")) == 0, "Unexpected argv");
	zassert_true(argc_buffers == 3, "Unexpected argc");
	modem_backend_mock_put(&mock, cgreg_response, sizeof(cgreg_response) - 1);

	k_msleep(100);

	zassert_true(memcmp(argv_buffers[0], "CGREG: ", sizeof("CGREG: ")) == 0, "Unexpected argv");
	zassert_true(memcmp(argv_buffers[1], "10", sizeof("10")) == 0, "Unexpected argv");
	zassert_true(memcmp(argv_buffers[2], "43", sizeof("43")) == 0, "Unexpected argv");
	zassert_true(argc_buffers == 3, "Unexpected argc");
	modem_backend_mock_put(&mock, ok_response, sizeof(ok_response) - 1);

	k_msleep(100);

	/*
	 * Script sends "AT+QENG=\"servingcell\"\r\n"
	 * Modem responds qeng_servinc_cell_response (long string)
	 * Modem responds "OK\r\n"
	 */

	modem_backend_mock_get(&mock, buffer, ARRAY_SIZE(buffer));
	zassert_true(memcmp(buffer, "AT+QENG=\"servingcell\"\r\n",
			    sizeof("AT+QENG=\"servingcell\"\r\n") - 1) == 0,
		     "Request not sent as expected");

	modem_backend_mock_put(&mock, qeng_servinc_cell_response,
			       sizeof(qeng_servinc_cell_response) - 1);

	k_msleep(100);

	zassert_true(memcmp(argv_buffers[0], "+QENG: \"servingcell\",",
			    sizeof("+QENG: \"servingcell\",")) == 0,
		     "Unexpected argv");

	zassert_true(memcmp(argv_buffers[1], "\"NOCONN\"", sizeof("\"NOCONN\"")) == 0,
		     "Unexpected argv");

	zassert_true(memcmp(argv_buffers[10], "-68", sizeof("-68")) == 0, "Unexpected argv");
	zassert_true(argv_buffers[25][0] == '\0', "Unexpected argv");
	zassert_true(argc_buffers == 26, "Unexpected argc");

	/*
	 * Script ends after modem responds OK
	 */

	called = atomic_test_bit(&callback_called, MODEM_CHAT_UTEST_ON_SCRIPT_CALLBACK_BIT);
	zassert_true(called == false, "Script callback should not have been called yet");
	modem_backend_mock_put(&mock, ok_response, sizeof(ok_response) - 1);

	k_msleep(100);

	called = atomic_test_bit(&callback_called, MODEM_CHAT_UTEST_ON_SCRIPT_CALLBACK_BIT);
	zassert_true(called == true, "Script callback should have been called");
	zassert_true(script_result == MODEM_CHAT_SCRIPT_RESULT_SUCCESS,
		     "Script result should be SUCCESS");
	zassert_true(script_result_user_data == &cmd_user_data,
		     "Script result callback user data is incorrect");
}

ZTEST(modem_chat, test_start_script_twice_then_abort)
{
	bool called;

	zassert_true(modem_chat_script_run(&cmd, &script) == 0, "Failed to start script");

	k_msleep(100);

	zassert_true(modem_chat_script_run(&cmd, &script) == -EBUSY,
		     "Started new script while script is running");

	called = atomic_test_bit(&callback_called, MODEM_CHAT_UTEST_ON_SCRIPT_CALLBACK_BIT);
	zassert_true(called == false, "Script callback should not have been called yet");
	modem_chat_script_abort(&cmd);

	k_msleep(100);

	called = atomic_test_bit(&callback_called, MODEM_CHAT_UTEST_ON_SCRIPT_CALLBACK_BIT);
	zassert_true(called == true, "Script callback should have been called");
	zassert_true(script_result == MODEM_CHAT_SCRIPT_RESULT_ABORT,
		     "Script result should be ABORT");
	zassert_true(script_result_user_data == &cmd_user_data,
		     "Script result callback user data is incorrect");
}

ZTEST(modem_chat, test_start_script_then_time_out)
{
	bool called;

	zassert_true(modem_chat_script_run(&cmd, &script) == 0, "Failed to start script");
	k_msleep(100);

	called = atomic_test_bit(&callback_called, MODEM_CHAT_UTEST_ON_SCRIPT_CALLBACK_BIT);
	zassert_true(called == false, "Script callback should not have been called yet");

	k_msleep(5900);

	called = atomic_test_bit(&callback_called, MODEM_CHAT_UTEST_ON_SCRIPT_CALLBACK_BIT);
	zassert_true(called == true, "Script callback should have been called");
	zassert_true(script_result == MODEM_CHAT_SCRIPT_RESULT_TIMEOUT,
		     "Script result should be TIMEOUT");
	zassert_true(script_result_user_data == &cmd_user_data,
		     "Script result callback user data is incorrect");
}

ZTEST(modem_chat, test_script_with_partial_matches)
{
	bool called;

	zassert_true(modem_chat_script_run(&cmd, &script_partial) == 0, "Failed to start script");
	k_msleep(100);

	/*
	 * Script sends "AT+CMGL=4\r";
	 */

	modem_backend_mock_get(&mock, buffer, ARRAY_SIZE(buffer));
	zassert_true(memcmp(buffer, "AT+CMGL=4\r", sizeof("AT+CMGL=4\r") - 1) == 0,
		     "Request not sent as expected");

	/*
	 * Modem will return the following sequence 3 times
	 * "+CMGL: 1,1,,50\r";
	 * "07911326060032F064A9542954\r"
	 */

	for (uint8_t i = 0; i < 3; i++) {
		atomic_set(&callback_called, 0);
		modem_backend_mock_put(&mock, cmgl_response_0, sizeof(cmgl_response_0) - 1);
		k_msleep(100);

		called = atomic_test_bit(&callback_called,
					 MODEM_CHAT_UTEST_ON_CMGL_PARTIAL_CALLED_BIT);
		zassert_equal(called, true, "Match callback not called");
		zassert_equal(argc_buffers, 5, "Incorrect number of args");
		zassert_equal(strcmp(argv_buffers[0], "+CMGL: "), 0, "Incorrect argv received");
		zassert_equal(strcmp(argv_buffers[1], "1"), 0, "Incorrect argv received");
		zassert_equal(strcmp(argv_buffers[2], "1"), 0, "Incorrect argv received");
		zassert_equal(strcmp(argv_buffers[3], ""), 0, "Incorrect argv received");
		zassert_equal(strcmp(argv_buffers[4], "50"), 0, "Incorrect argv received");

		atomic_set(&callback_called, 0);
		modem_backend_mock_put(&mock, cmgl_response_1, sizeof(cmgl_response_1) - 1);
		k_msleep(100);

		called = atomic_test_bit(&callback_called,
					 MODEM_CHAT_UTEST_ON_CMGL_PARTIAL_ANY_CALLED_BIT);
		zassert_equal(called, true, "Match callback not called");
		zassert_equal(argc_buffers, 2, "Incorrect number of args");
		zassert_equal(strcmp(argv_buffers[0], ""), 0, "Incorrect argv received");
		zassert_equal(strcmp(argv_buffers[1], "07911326060032F064A9542954"), 0,
			      "Incorrect argv received");
	}

	atomic_set(&callback_called, 0);
	modem_backend_mock_put(&mock, ok_response, sizeof(ok_response) - 1);
	k_msleep(100);

	/*
	 * Modem returns "OK\r"
	 * Script terminates
	 */

	called = atomic_test_bit(&callback_called, MODEM_CHAT_UTEST_ON_SCRIPT_CALLBACK_BIT);
	zassert_true(called == true, "Script callback should have been called");
	zassert_equal(script_result, MODEM_CHAT_SCRIPT_RESULT_SUCCESS,
		      "Script should have stopped with success");

	/* Assert no data was sent except the request */
	zassert_equal(modem_backend_mock_get(&mock, buffer, ARRAY_SIZE(buffer)), 0,
		      "Script sent too many requests");
}

/*************************************************************************************************/
/*                                         Test suite                                            */
/*************************************************************************************************/
ZTEST_SUITE(modem_chat, NULL, test_modem_chat_setup, test_modem_chat_before, test_modem_chat_after,
	    NULL);
