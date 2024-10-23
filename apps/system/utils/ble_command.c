#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include <ble_manager/ble_manager.h>
#include "ble_packet_type.h"
#include <apps/shell/tash.h>

#define BLE_LOG_VERBOSE printf
#define BLE_LOG_INFO printf
#define BLE_LOG_DEBUG(...)
#define BLE_LOG_ERROR printf

#define BLE_ES_TEST_SERVICE (0x7A11)
#define BLE_ES_TEST_CHAR (0x7A12)
#define BLE_ES_TEST_CHAR_CCCD (0x7A14)

static ble_client_ctx *g_client_contexts[BLE_MAX_CONNECTION_COUNT];
static int g_client_context_count = 0;

static void ble_command_print_help_message(void)
{
	char *help_message =
		"Usage :\n"
		"\tcble status\n"
		"\tcble client create\n"
		"\tcble client destroy {ctx_idx}\n"
		"\tcble client connect {ctx_idx} {MAC}\n"
		"\tcble client disconnect {ctx_idx}\n"
		"\tcble scan start all\n"
		"\tcble scan start [whitelist: true/false] {duration[sec]} {packet filter length} {packet filter}\n" // if scan duration is 0, scan duration will be set as 5 mins.
		"\tcble scan stop\n";
	puts(help_message);
}

static void ble_command_print_mac(char *prefix, uint8_t *mac)
{
	BLE_LOG_INFO("%s%02x:%02x:%02x:%02x:%02x:%02x\n", prefix,
				 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void disconnected_cb(ble_client_ctx *ctx)
{
	BLE_LOG_INFO("Device disconnected callback called. client 0x%04x", ctx->conn_handle);
}

static void connected_cb(ble_client_ctx *ctx, ble_device_connected *connected_device)
{
	BLE_LOG_INFO("Device connected callback called. client 0x%04x server 0x%04x", ctx->conn_handle, connected_device->conn_handle);
}

static void notification_cb(ble_client_ctx *ctx, ble_attr_handle attr_handle, ble_data *read_result)
{
	BLE_LOG_INFO("Device notification callback called. client 0x%04x", ctx->conn_handle);
	if (read_result->length > 0) {
		BLE_LOG_INFO("Data from notification callback: %s", read_result->data);
	}
}

static void indication_cb(ble_client_ctx *ctx, ble_attr_handle attr_handle, ble_data *read_result)
{
	BLE_LOG_INFO("Device indication callback called. client 0x%04x", ctx->conn_handle);
	if (read_result->length > 0) {
		BLE_LOG_INFO("Data from indication callback: %s", read_result->data);
	}
}

static ble_client_callback_list g_client_callback_list = {
	disconnected_cb,
	connected_cb,
	notification_cb,
	indication_cb,
};

static void disconnected_cb_copy(ble_client_ctx *ctx)
{
	BLE_LOG_INFO("Second Device disconnected callback called. client 0x%04x", ctx->conn_handle);
}

static void connected_cb_copy(ble_client_ctx *ctx, ble_device_connected *connected_device)
{
	BLE_LOG_INFO("Second Device connected callback called. client 0x%04x server 0x%04x", ctx->conn_handle, connected_device->conn_handle);
}

static void notification_cb_copy(ble_client_ctx *ctx, ble_attr_handle attr_handle, ble_data *read_result)
{
	BLE_LOG_INFO("Second Device notification callback called. client 0x%04x", ctx->conn_handle);
}

static void indication_cb_copy(ble_client_ctx *ctx, ble_attr_handle attr_handle, ble_data *read_result)
{
	BLE_LOG_INFO("Second Device indication callback called. client 0x%04x", ctx->conn_handle);
}

static ble_client_callback_list g_client_callback_list_copy = {
	disconnected_cb_copy,
	connected_cb_copy,
	notification_cb_copy,
	indication_cb_copy,
};

static int ble_client_create_context(void)
{
	if (g_client_context_count >= BLE_MAX_CONNECTION_COUNT) {
		BLE_LOG_ERROR("Client context count reached max count");
		return -1;
	}

	if (g_client_context_count == 0) {
		g_client_contexts[g_client_context_count] = ble_client_create_ctx(&g_client_callback_list);
	} else {
		g_client_contexts[g_client_context_count] = ble_client_create_ctx(&g_client_callback_list_copy);
	}

	if (g_client_contexts[g_client_context_count] == NULL) {
		BLE_LOG_ERROR("BLE client create ctx failed");
		return -1;
	}

	BLE_LOG_INFO("BLE client created successfully! Index : %d", g_client_context_count++);
	return g_client_context_count;
}

static void ble_client_destory_context(int conn_idx)
{

	if (conn_idx < 0 || conn_idx >= g_client_context_count) {
		BLE_LOG_ERROR("Invalid parameter");
		return;
	}
	ble_client_ctx *ctx = g_client_contexts[conn_idx];

	if (ble_client_destroy_ctx(ctx) != BLE_MANAGER_SUCCESS) {
		BLE_LOG_ERROR("Destroy context failed.");
		return;
	}

	for (int i = conn_idx + 1; i < g_client_context_count; i++) {
		g_client_contexts[i - 1] = g_client_contexts[i];
	}
	g_client_context_count--;
}

static void scan_state_changed_cb(ble_scan_state_e scan_state)
{
	BLE_LOG_INFO("Scan state changed callback called. scan state: %d ", scan_state);
}

static void ble_adv_packet_print(ble_adv_ind_t *raw_data, ble_scan_resp_t *scan_resp)
{
	uint8_t adv_ind_copy[BLE_ADV_RAW_DATA_MAX_LEN] = {0};
	memcpy(&adv_ind_copy, raw_data, BLE_ADV_RAW_DATA_MAX_LEN);

	uint8_t scan_resp_copy[BLE_ADV_RESP_DATA_MAX_LEN] = {0};
	memcpy(&scan_resp_copy, scan_resp, BLE_ADV_RESP_DATA_MAX_LEN);

	char data_str[150] = {0};
	int ptr = 0;
	for (int i = 0; i < sizeof(adv_ind_copy); i++) {
		if (i == 10 || i == 11) // for onboardingdata
		{
			snprintf(&data_str[ptr * 2], 3, "%s", "   ");
			ptr++;
		}
		snprintf(&data_str[ptr * 2], 3, "%02X", adv_ind_copy[i]);
		ptr++;
	}
	int adv_len = ptr * 2;
	data_str[adv_len] = '\t';
	ptr = (adv_len + 1);
	for (int i = 0; i < sizeof(scan_resp_copy); i++, ptr += 2) {
		snprintf(&data_str[ptr], 3, "%02X", scan_resp_copy[i]);
	}

	BLE_LOG_INFO("%s", data_str);
}

static void parse_onboarding_packet(ble_adv_ind_t *raw_data, ble_scan_resp_t *scan_resp)
{
	// Raw Data
	if (raw_data->flags_len != 0x02)
		return;
	if (raw_data->flags_type != BLE_ADV_DATA_FLAGS)
		return;
	BLE_LOG_DEBUG("Flags : %02X", raw_data->flags_value);

	if (raw_data->manufacturer_specific_data_len != 0x1b)
		return;
	if (raw_data->manufacturer_specific_data_type != BLE_ADV_DATA_MANUFACTURER_SPECIFIC_DATA)
		return;
	BLE_LOG_DEBUG("Company ID : 0x%02X%02X %s", raw_data->company_id[0], raw_data->company_id[1], (raw_data->company_id[0] == 0x75 && raw_data->company_id[1] == 0x00 ? "(Samsung Electronics)" : ""));
	BLE_LOG_DEBUG("Control & Version : 0x%02X // Version : %u, Active Scan : %u, Multiple Services : %u", raw_data->control_and_version, raw_data->control_and_version.version, raw_data->control_and_version.active_scan, raw_data->control_and_version.multiple_services);
	BLE_LOG_DEBUG("Service Id : 0x%02X %s", raw_data->service_id, (raw_data->service_id == 0x0c ? "(Samsung Connect)" : ""));
	BLE_LOG_DEBUG("Samsung Connect Packet Version : 0x%02X", raw_data->samsung_connect_packet_version);
	BLE_LOG_DEBUG("Onboarding Info: 0x%02X", raw_data->onboarding_info);
	BLE_LOG_DEBUG("\tOnboarding Supported: %u %s", raw_data->onboarding_info.onboarding_supported, (raw_data->onboarding_info.onboarding_supported == 0 ? "(not supported)" : "(supported)"));
	BLE_LOG_DEBUG("\tOwned State: %u %s", raw_data->onboarding_info.owned_state, (raw_data->onboarding_info.owned_state == 0 ? "(unowned)" : "(owned)"));
	BLE_LOG_DEBUG("\tOnboarding Available: %u %s", raw_data->onboarding_info.onboarding_available, (raw_data->onboarding_info.onboarding_available == 0 ? "(unavailable)" : "(available)"));
	BLE_LOG_DEBUG("\tD2D Device: %u %s", raw_data->onboarding_info.d2d_device, (raw_data->onboarding_info.d2d_device == 0 ? "(D2S Device)" : "(D2D Device)"));
	BLE_LOG_DEBUG("\tOOB: %u", raw_data->onboarding_info.oob);
	BLE_LOG_DEBUG("\tAssisted Onboarding Support: %u %s", raw_data->onboarding_info.two_factor_authentication_onboarding_support, (raw_data->onboarding_info.two_factor_authentication_onboarding_support == 0 ? "(not supported)" : "(supported)"));
	BLE_LOG_DEBUG("\tCalm Mode Support: %u %s", raw_data->onboarding_info.calm_mode_support, (raw_data->onboarding_info.calm_mode_support == 0 ? "(not supported)" : "(supported)"));
	BLE_LOG_DEBUG("\tFOA Support: %u %s", raw_data->onboarding_info.first_ownership_authentication_status, (raw_data->onboarding_info.first_ownership_authentication_status == 0 ? (raw_data->onboarding_info.two_factor_authentication_onboarding_support == 0 ? "(Default)" : "(First Ownership Authentication Ready)") : "(First Ownership Authentication Completed)"));

	if (raw_data->features.setup_info != 0) {
		BLE_LOG_DEBUG("MNID : 0x%02X%02X%02X%02X (%c%c%c%c)",
					  raw_data->setup_info_manufacturer_id[0],
					  raw_data->setup_info_manufacturer_id[1],
					  raw_data->setup_info_manufacturer_id[2],
					  raw_data->setup_info_manufacturer_id[3],
					  raw_data->setup_info_manufacturer_id[0],
					  raw_data->setup_info_manufacturer_id[1],
					  raw_data->setup_info_manufacturer_id[2],
					  raw_data->setup_info_manufacturer_id[3]);

		BLE_LOG_DEBUG("Setup ID : 0x%02X%02X%02X (%c%c%c)",
					  raw_data->setup_info_setup_id[0],
					  raw_data->setup_info_setup_id[1],
					  raw_data->setup_info_setup_id[2],
					  raw_data->setup_info_setup_id[0],
					  raw_data->setup_info_setup_id[1],
					  raw_data->setup_info_setup_id[2]);
	}

	if (raw_data->features.device_status != 0) {
		BLE_LOG_DEBUG("Device Status : 0x%02X", raw_data->device_status);
		BLE_LOG_DEBUG("\tDevice ON: %u %s", raw_data->device_status.device_on, (raw_data->device_status.device_on == 0 ? "(Off)" : "(On)"));
		BLE_LOG_DEBUG("\tAP Connected: %u %s", raw_data->device_status.ap_connected, (raw_data->device_status.ap_connected == 0 ? "(not connected)" : "(connected)"));
		BLE_LOG_DEBUG("\tHealth Status: %u %s", raw_data->device_status.health_status, (raw_data->device_status.health_status == 0 ? "(offline)" : "(online)"));
		BLE_LOG_DEBUG("\tP2P Connected: %u", raw_data->device_status.p2p_connected);
		BLE_LOG_DEBUG("\tP2P Connection Ready: %u", raw_data->device_status.p2p_connection_ready);
		BLE_LOG_DEBUG("\tSetup: %u", raw_data->device_status.setup);
		BLE_LOG_DEBUG("\tSamsung Account Required: %u", raw_data->device_status.samsung_account_required);
	}

	if (raw_data->features.setup_available_network != 0) {
		BLE_LOG_DEBUG("Setup Available Network : 0x%02X // %s %s %s",
					  raw_data->setup_available_network,
					  (raw_data->setup_available_network.wifi ? "Wi-Fi" : ""),
					  (raw_data->setup_available_network.l3 ? "L3" : ""),
					  (raw_data->setup_available_network.ble ? "BLE" : ""));
	}

	if (raw_data->features.address != 0) {
		BLE_LOG_DEBUG("%s MAC Address : %02x:%02x:%02x:%02x:%02x:%02x",
					  (raw_data->address_type == BLE_ADV_DATA_BT_ADDR ? "BLE" : "Wi-Fi"),
					  raw_data->address_addr1[0],
					  raw_data->address_addr1[1],
					  raw_data->address_addr1[2],
					  raw_data->address_addr1[3],
					  raw_data->address_addr1[4],
					  raw_data->address_addr1[5]);
	}

	if (raw_data->custom_data_len != 0x0A)
		return;
	if (raw_data->serial_type != BLE_ADV_DATA_SERIAL)
		return;
	if (raw_data->serial_len != 0x04)
		return;

	// Scan Response
	if (scan_resp->manufacturer_specific_data_len != 0x0B)
		return;
	if (scan_resp->manufacturer_specific_data_type != BLE_ADV_DATA_MANUFACTURER_SPECIFIC_DATA)
		return;

	BLE_LOG_DEBUG("Company ID : 0x%02X%02X %s", scan_resp->company_id[0], scan_resp->company_id[1], (scan_resp->company_id[0] == 0x75 && scan_resp->company_id[1] == 0x00 ? "(Samsung Electronics)" : ""));
	BLE_LOG_DEBUG("Serial : 0x%02X%02X%02X%02X (%c%c%c%c)",
				  scan_resp->serial_value[0],
				  scan_resp->serial_value[1],
				  scan_resp->serial_value[2],
				  scan_resp->serial_value[3],
				  scan_resp->serial_value[0],
				  scan_resp->serial_value[1],
				  scan_resp->serial_value[2],
				  scan_resp->serial_value[3]);

	if (scan_resp->otm_support_confirm_method_type != BLE_ADV_DATA_OTM_SUPPORT_CONFIRM_METHOD)
		return;
	if (scan_resp->otm_support_confirm_method_len != 0x02)
		return;

	BLE_LOG_DEBUG("OTM support confirm method : 0x%04X", scan_resp->otm_support_confirm_method_value);
	if (scan_resp->otm_support_confirm_method_value.support_ultrasound_pin_when_perform_random_pin_otm != 0) {
		BLE_LOG_DEBUG("\t[0] : Support Ultrasound Pin when perform RandomPin OTM");
	}
	if (scan_resp->otm_support_confirm_method_value.support_auto_confirm_via_ultrasound_pin != 0) {
		BLE_LOG_DEBUG("\t[1] : Support Auto confirm via Ultrasound Pin");
	}
	if (scan_resp->otm_support_confirm_method_value.ir_confirm_otm_setup != 0) {
		BLE_LOG_DEBUG("\t[2] : IR confirm OTM setup");
	}
	if (scan_resp->otm_support_confirm_method_value.ble_ranging_based_otm != 0) {
		BLE_LOG_DEBUG("\t[3] : BLE Ranging-based OTM");
	}
	if (scan_resp->otm_support_confirm_method_value.preceding_confirm_otm_setup != 0) {
		BLE_LOG_DEBUG("\t[11] : Preceding confirm OTM setup");
	}

	if (scan_resp->device_name_type != BLE_ADV_DATA_COMPLETE_LOCAL_NAME)
		return;

	char device_name[100] = {0};
	char device_name_string[100] = {0};
	for (int i = 0; i < scan_resp->device_name_len; i++) {
		snprintf(&device_name[i * 2], 3, "%02X", scan_resp->device_name_value[i]);
		snprintf(&device_name_string[i], 2, "%c", scan_resp->device_name_value[i]);
	}
	BLE_LOG_DEBUG("BLE adv name : 0x%s (%s)", device_name, device_name_string);

	ble_adv_packet_print(raw_data, scan_resp);
}

static void parse_advertising_packet(ble_scanned_device *raw_data, ble_scanned_device *scan_resp)
{
	if (raw_data->adv_type != BLE_ADV_TYPE_IND && raw_data->adv_type != BLE_ADV_TYPE_NONCONN_IND) {
		return;
	}

	if (scan_resp->adv_type != BLE_ADV_TYPE_SCAN_RSP) {
		return;
	}

	if (memcmp(&raw_data->addr, &scan_resp->addr, sizeof(ble_addr)) != 0) {
		return;
	}

	if (raw_data->raw_data_length != 31 || scan_resp->resp_data_length != 31) {
		return;
	}

	ble_adv_ind_t parsed_raw_data = {0};
	memcpy(&parsed_raw_data, &raw_data->raw_data, raw_data->raw_data_length);
	ble_scan_resp_t parsed_scan_resp = {0};
	memcpy(&parsed_scan_resp, &scan_resp->resp_data, scan_resp->resp_data_length);

	parse_onboarding_packet(&parsed_raw_data, &parsed_scan_resp);
}

ble_scanned_device scanned_raw_data = {0};
ble_scanned_device scanned_scan_resp = {0};

static void device_scanned_cb(ble_scanned_device *scanned_device)
{
	BLE_LOG_INFO("Device scanned callback called. adv_type : %d%s, rssi : %d", scanned_device->adv_type, (scanned_device->adv_type == BLE_ADV_TYPE_IND ? "(BLE_ADV_TYPE_IND)" : (scanned_device->adv_type == BLE_ADV_TYPE_SCAN_RSP ? "(BLE_ADV_TYPE_SCAN_RSP)" : "")), scanned_device->rssi);
	ble_command_print_mac("Scanned mac : ", scanned_device->addr.mac);

	if (scanned_device->adv_type == BLE_ADV_TYPE_IND || scanned_device->adv_type == BLE_ADV_TYPE_NONCONN_IND) {
		memcpy(&scanned_raw_data, scanned_device, sizeof(ble_scanned_device));
	} else if (scanned_device->adv_type == BLE_ADV_TYPE_SCAN_RSP) {
		memcpy(&scanned_scan_resp, scanned_device, sizeof(ble_scanned_device));
		parse_advertising_packet(&scanned_raw_data, &scanned_scan_resp);
	}
}

static ble_scan_callback_list g_scan_callback_list = {
	scan_state_changed_cb,
	device_scanned_cb,
};

static void ble_command_scan(int argc, char *argv[])
{
	if (argc < 3) {
		ble_command_print_help_message();
		return;
	}

	if (strcmp(argv[2], "start") == 0) {
		if (argc < 4) {
			ble_command_print_help_message();
			return;
		}

		if (strcmp(argv[3], "all") == 0) {
			if (ble_client_start_scan(NULL, &g_scan_callback_list) != BLE_MANAGER_SUCCESS) {
				BLE_LOG_ERROR("Failed to start scan");
			}
		} else {
			if (argc < 5) {
				ble_command_print_help_message();
				return;
			}
			// TODO check filtering with raw_data is enable
			ble_scan_filter scan_filter = {0};
			if (strcmp(argv[3], "true") == 0) {
				scan_filter.whitelist_enable = true;
			} else {
				scan_filter.whitelist_enable = false;
			}
			scan_filter.scan_duration = (uint32_t)(atoi(argv[4]) * 1000);

			scan_filter.raw_data_length = (uint8_t)(atoi(argv[5]));
			if (scan_filter.raw_data_length >= BLE_ADV_RAW_DATA_MAX_LEN) {
				ble_command_print_help_message();
				return;
			}
			if (scan_filter.raw_data_length > 0) {
				if (argc < 6) {
					ble_command_print_help_message();
					return;
				}

				char hex_value[3] = {0};
				for (int i = 0; i < scan_filter.raw_data_length; i++) {
					hex_value[0] = argv[6][i * 2];
					hex_value[1] = argv[6][i * 2 + 1];
					scan_filter.raw_data[i] = (uint8_t)strtol(hex_value, NULL, 16);
				}

				char filter_data[150] = {0};
				for (int i = 0; i < scan_filter.raw_data_length; i++) {
					snprintf(&filter_data[i * 2], 3, "%02X", scan_filter.raw_data[i]);
				}
				BLE_LOG_INFO("raw data filter : %s", filter_data);
			}

			if (ble_client_start_scan(&scan_filter, &g_scan_callback_list) != BLE_MANAGER_SUCCESS) {
				BLE_LOG_ERROR("Failed to start scan");
			}
		}

	} else if (strcmp(argv[2], "stop") == 0) {
		if (ble_client_stop_scan() != BLE_MANAGER_SUCCESS) {
			BLE_LOG_ERROR("Failed to stop scan");
		}
	} else {
		ble_command_print_help_message();
	}
	return;
}

static void str_to_ble_mac(char *src, uint8_t *dest)
{
	for (int i = 0; i < 6; i++) {
		dest[i] = (uint8_t)strtol(src + i * 3, NULL, 16);
	}
}

static void ble_command_filter(int argc, char *argv[])
{
	if (argc < 4) {
		ble_command_print_help_message();
		return;
	}

	ble_addr addr = {0};
	str_to_ble_mac(argv[3], (uint8_t *)&addr.mac);
	addr.type = BLE_ADDR_TYPE_PUBLIC;

	if (strcmp(argv[2], "add") == 0) {
		ble_command_print_mac("Add mac in whitelist : ", addr.mac);

		if (ble_scan_whitelist_add(&addr) != BLE_MANAGER_SUCCESS) {
			BLE_LOG_ERROR("Failed to add in whitelist");
		}
	} else if (strcmp(argv[2], "delete") == 0) {
		ble_command_print_mac("Remove mac from whitelist : ", addr.mac);

		if (ble_scan_whitelist_delete(&addr) != BLE_MANAGER_SUCCESS) {
			BLE_LOG_ERROR("Failed to delete from whitelist");
		}
	} else {
		ble_command_print_help_message();
	}
}

static void ble_command_connect(int conn_idx, char *mac)
{
	if (conn_idx < 0 || conn_idx >= g_client_context_count) {
		BLE_LOG_ERROR("Invalid parameter");
		return;
	}
	ble_client_ctx *ctx = g_client_contexts[conn_idx];

	// Stick Cleaner conn_info
	ble_conn_info conn_info = {0};
	conn_info.conn_interval = 9;
	conn_info.slave_latency = 128;
	conn_info.mtu = 240;
	conn_info.is_secured_connect = true;
	conn_info.scan_timeout = 5 * 1000;
	str_to_ble_mac(mac, (uint8_t *)&conn_info.addr.mac);
	conn_info.addr.type = BLE_ADDR_TYPE_PUBLIC;

	if (ble_client_connect(ctx, &conn_info) != BLE_MANAGER_SUCCESS) {
		BLE_LOG_ERROR("Failed to connect to %s", mac);
	}
}

static void ble_command_disconnect(int conn_idx)
{
	if (conn_idx < 0 || conn_idx >= g_client_context_count) {
		BLE_LOG_ERROR("Invalid parameter");
		return;
	}
	ble_client_ctx *ctx = g_client_contexts[conn_idx];

	if (ble_client_disconnect(ctx) != BLE_MANAGER_SUCCESS) {
		BLE_LOG_ERROR("Failed to disconnect");
	}
}

#define DATA_LEN 100

static void ble_command_read(int conn_idx)
{
	if (conn_idx < 0 || conn_idx >= g_client_context_count) {
		BLE_LOG_ERROR("Invalid parameter");
		return;
	}
	ble_client_ctx *ctx = g_client_contexts[conn_idx];

	uint8_t dummy[DATA_LEN + 1];
	ble_data data = {dummy, 0};

	ble_result_e result = BLE_MANAGER_SUCCESS;
	BLE_LOG_INFO("Read test start");
	result = ble_client_operation_read(ctx, BLE_ES_TEST_CHAR + 1, &data);
	BLE_LOG_INFO("\t BLE_ES_TEST_CHAR result : %d%s", result, (result == BLE_MANAGER_SUCCESS ? "(BLE_MANAGER_SUCCESS)" : ""));
}

static void ble_command_write(int conn_idx, char *input)
{
	if (conn_idx < 0 || conn_idx >= g_client_context_count) {
		BLE_LOG_ERROR("Invalid parameter");
		return;
	}
	ble_client_ctx *ctx = g_client_contexts[conn_idx];

	int data_len = strnlen(input, DATA_LEN);
	ble_data data = {(uint8_t *)input, data_len + 1};

	ble_result_e result = BLE_MANAGER_SUCCESS;
	BLE_LOG_INFO("Write test start");
	result = ble_client_operation_write(ctx, BLE_ES_TEST_CHAR + 1, &data);
	BLE_LOG_INFO("\t BLE_ES_TEST_CHAR result : %d%s", result, (result == BLE_MANAGER_SUCCESS ? "(BLE_MANAGER_SUCCESS)" : ""));

	BLE_LOG_INFO("Write no response test start");
	result = ble_client_operation_write(ctx, BLE_ES_TEST_CHAR + 1, &data);
	BLE_LOG_INFO("\t BLE_ES_TEST_CHAR result : %d%s", result, (result == BLE_MANAGER_SUCCESS ? "(BLE_MANAGER_SUCCESS)" : ""));
}

static void ble_command_enable(int conn_idx, int type)
{
	if (conn_idx < 0 || conn_idx >= g_client_context_count) {
		BLE_LOG_ERROR("Invalid parameter");
		return;
	}
	ble_client_ctx *ctx = g_client_contexts[conn_idx];

	ble_result_e result = BLE_MANAGER_SUCCESS;
	if (type == 0) {
		BLE_LOG_INFO("Enable notification test start");
		result = ble_client_operation_enable_notification(ctx, BLE_ES_TEST_CHAR_CCCD);
		BLE_LOG_INFO("\t BLE_ES_TEST_CHAR_CCCD result : %d%s", result, (result == BLE_MANAGER_SUCCESS ? "(BLE_MANAGER_SUCCESS)" : ""));
	} else if (type == 1) {
		BLE_LOG_INFO("Enable indication test start");
		result = ble_client_operation_enable_indication(ctx, BLE_ES_TEST_CHAR_CCCD);
		BLE_LOG_INFO("\t BLE_ES_TEST_CHAR_CCCD result : %d%s", result, (result == BLE_MANAGER_SUCCESS ? "(BLE_MANAGER_SUCCESS)" : ""));
	} else if (type == 2) {
		BLE_LOG_INFO("Enable notification & indication test start");
		result = ble_client_operation_enable_notification_and_indication(ctx, BLE_ES_TEST_CHAR_CCCD);
		BLE_LOG_INFO("\t BLE_ES_TEST_CHAR_CCCD result : %d%s", result, (result == BLE_MANAGER_SUCCESS ? "(BLE_MANAGER_SUCCESS)" : ""));
	}
}

static void ble_command_client(int argc, char *argv[])
{
	if (argc < 3) {
		ble_command_print_help_message();
		return;
	}

	if (strcmp(argv[2], "create") == 0) {
		ble_client_create_context();
	} else if (strcmp(argv[2], "destroy") == 0) {
		if (argc < 4) {
			ble_command_print_help_message();
			return;
		}
		ble_client_destory_context(atoi(argv[3]));
	} else if (strcmp(argv[2], "connect") == 0) {
		if (argc < 5) {
			ble_command_print_help_message();
			return;
		}
		ble_command_connect(atoi(argv[3]), argv[4]);
	} else if (strcmp(argv[2], "disconnect") == 0) {
		if (argc < 4) {
			ble_command_print_help_message();
			return;
		}
		ble_command_disconnect(atoi(argv[3]));
	} else if (strcmp(argv[2], "read") == 0) {
		if (argc < 4) {
			ble_command_print_help_message();
			return;
		}
		ble_command_read(atoi(argv[3]));
	} else if (strcmp(argv[2], "write") == 0) {
		if (argc < 5) {
			ble_command_print_help_message();
			return;
		}
		ble_command_write(atoi(argv[3]), argv[4]);
	} else if (strcmp(argv[2], "enable") == 0) {
		if (argc < 5) {
			ble_command_print_help_message();
			return;
		}
		ble_command_enable(atoi(argv[3]), atoi(argv[4]));
	} else {
		ble_command_print_help_message();
	}
}

static void ble_client_print_context(void)
{
	BLE_LOG_INFO("BLE client context count : %d\n", g_client_context_count);
	ble_device_connected_list connected_list = {0};
	uint8_t mac[BLE_BD_ADDR_MAX_LEN] = {0};
	if (ble_client_connected_device_list(&connected_list) == BLE_MANAGER_SUCCESS) {
		BLE_LOG_INFO("Connected devices count : %d\n", connected_list.connected_count);
		for (int i = 0; i < connected_list.connected_count; i++) {
			if (ble_server_get_mac_addr_by_conn_handle(connected_list.conn_handle[i], mac) == BLE_MANAGER_SUCCESS) {
				ble_command_print_mac("\tConnected device mac address: ", mac);
			} else {
				BLE_LOG_ERROR("Failed to get mac address from conn handle\n");
			}
		}
	}

	ble_bonded_device_list bonded_list[BLE_MAX_BONDED_DEVICE] = {0};
	uint16_t bonded_count = 0;
	if (ble_manager_get_bonded_device(bonded_list, &bonded_count) == BLE_MANAGER_SUCCESS) {
		BLE_LOG_INFO("Bonded devices count : %d\n", bonded_count);
		for (int i = 0; i < bonded_count; i++) {
			ble_command_print_mac("\tBonded mac address: ", bonded_list[i].bd_addr.mac);
		}
	}

	for (int i = 0; i < g_client_context_count; i++) {
		BLE_LOG_INFO("BLE client context conn handle : 0x%04x\n", g_client_contexts[i]->conn_handle);
		ble_device_connected device_info = {0};
		if (ble_client_connected_info(g_client_contexts[i], &device_info) == BLE_MANAGER_SUCCESS) {
			ble_command_print_mac("\tConnected to ", device_info.conn_info.addr.mac);
			BLE_LOG_INFO("\tBonded : %d\n", device_info.is_bonded);
		}
		ble_client_state_e state = ble_client_get_state(g_client_contexts[i]);
		BLE_LOG_INFO("\tstate : %d%s\n", state, (state == BLE_CLIENT_CONNECTED ? "(CONNECTED)" : ""));
	}

	// BLE_LOG_INFO("BLE service client state");
	// for (int i = 0; i < BLE_MAX_CONNECTION_COUNT; i++) {
	// 	ble_service_client_state_e state = ble_service_client_get_state(service_client_handles[i]);
	// 	BLE_LOG_INFO("\tstate : %d%s", state, (state == BLE_CLIENT_CONNECTED ? "(CONNECTED)" : ""));
	// }
}

static void ble_command_status(void)
{
	uint8_t ble_mac[BLE_BD_ADDR_MAX_LEN] = {0};
	ble_manager_get_mac_addr(ble_mac);

	ble_command_print_mac("BLE mac address of current device : ", ble_mac);

	ble_client_print_context();
}

static int total_count = 0;
static int total_scan_count = 0;
static int raw_count[3] = {0};
static int resp_count[3] = {0};
static bool raw_flag[3] = {0};
static bool resp_flag[3] = {0};
static ble_adv_ind_t adv_ind[3] = {0};
static ble_scan_resp_t scan_resp[3] = {0};
static ble_addr target_addr[3] = {
	{.mac = {0x50, 0xfd, 0xd5, 0x57, 0x26, 0x55}, .type = BLE_ADDR_TYPE_PUBLIC},
	{.mac = {0x50, 0xfd, 0xd5, 0x57, 0x28, 0xf9}, .type = BLE_ADDR_TYPE_PUBLIC},
	{.mac = {0x88, 0x57, 0x1d, 0xef, 0x18, 0xe7}, .type = BLE_ADDR_TYPE_PUBLIC},
};
static bool scanning = false;
static uint32_t duration = 0;
static uint32_t test_count = 0;
#define SCAN_REST_TIME 5000

static void _ble_scan_test_state_changed_0(ble_scan_state_e scan_state)
{
	if (scan_state == BLE_SCAN_STARTED) {
		BLE_LOG_INFO("Scanner0 start scan\n");

		raw_flag[0] = false;
		resp_flag[0] = false;
		memset(&adv_ind[0], 0, sizeof(ble_adv_ind_t));
		memset(&scan_resp[0], 0, sizeof(ble_scan_resp_t));
	} else if (scan_state == BLE_SCAN_STOPPED) {
		BLE_LOG_INFO("Scanner0 stop scan\n");

		if (raw_flag[0] == true) {
			raw_count[0]++;
		}
		if (resp_flag[0] == true) {
			resp_count[0]++;
		}

		if (raw_flag[0] && resp_flag[0]) {
			BLE_LOG_INFO("Scanner0 packet\n");
			// ble_adv_packet_print(&adv_ind[0], &scan_resp[0]);
		}
	}
}

static void _ble_scan_test_scanned_0(ble_scanned_device *scanned_device)
{
	if (memcmp(&scanned_device->addr, &target_addr[0], sizeof(ble_addr)) != 0) {
		return;
	}

	if (scanned_device->adv_type == BLE_ADV_TYPE_IND) {
		if (raw_flag[0] == true) {
			return;
		}

		raw_flag[0] = true;
		ble_command_print_mac("Scanner0 raw_data scanned. mac address: ", scanned_device->addr.mac);
		memcpy(&adv_ind[0], scanned_device->raw_data, sizeof(uint8_t) * scanned_device->raw_data_length);
	} else if (scanned_device->adv_type == BLE_ADV_TYPE_SCAN_RSP) {
		if (resp_flag[0] == true) {
			return;
		}

		resp_flag[0] = true;
		ble_command_print_mac("Scanner0 scan_resp scanned. mac address: ", scanned_device->addr.mac);
		memcpy(&scan_resp[0], scanned_device->resp_data, sizeof(uint8_t) * scanned_device->resp_data_length);
	}
}

static ble_scan_callback_list scan_test_callback_list[1] = {
	{.state_changed_cb = _ble_scan_test_state_changed_0, .device_scanned_cb = _ble_scan_test_scanned_0},
};

static pthread_t ble_scan_test_thread;

static void *ble_scan_test_task(void)
{
	ble_scan_filter filter = {0};
	filter.scan_duration = duration;
	scanning = true;

	for (int i = 0; scanning && i < test_count; i++) {
		total_count++;
		ble_client_start_scan(&filter, &scan_test_callback_list[0]);

		sleep((duration + SCAN_REST_TIME) / 1000);
		if (raw_flag[0] && resp_flag[0]) {
			total_scan_count++;
		}
	}

	BLE_LOG_INFO("Total count : %d / %d\n", total_scan_count, total_count);
	BLE_LOG_INFO("\tScanner%d raw_data scanned count : %d / %d\n", 0, raw_count[0], total_count);
	BLE_LOG_INFO("\tScanner%d scan_resp scanned count : %d / %d\n", 0, resp_count[0], total_count);
	scanning = false;

	return (void *)0;
}

static void ble_command_test(int argc, char *argv[])
{
	if (argc < 4) {
		ble_command_print_help_message();
		return;
	}

	if (strcmp(argv[2], "scan") == 0) {
		if (strcmp(argv[3], "start") == 0) {
			total_count = 0;
			total_scan_count = 0;
			memset(&raw_count, 0, sizeof(raw_count));
			memset(&resp_count, 0, sizeof(resp_count));
			memset(&raw_flag, 0, sizeof(raw_flag));
			memset(&resp_flag, 0, sizeof(resp_flag));

			duration = atoi(argv[4]) * 1000;
			test_count = atoi(argv[5]);
			pthread_create(&ble_scan_test_thread, NULL, (pthread_startroutine_t)ble_scan_test_task, NULL);
		} else if (strcmp(argv[3], "stop") == 0) {
			scanning = false;
			pthread_cancel(ble_scan_test_thread);
		}
	} else {
		ble_command_print_help_message();
	}
}

static void ble_server_connected_cb(ble_conn_handle con_handle, ble_server_connection_type_e conn_type, uint8_t mac[BLE_BD_ADDR_MAX_LEN])
{
	return;
}

static void ble_server_disconnected_cb(ble_conn_handle con_handle, uint16_t cause)
{
	return;
}

static void ble_server_mtu_update_cb(ble_conn_handle con_handle, uint16_t mtu_size)
{
	return;
}

static void ble_server_oneshot_adv_cb(uint16_t adv_result)
{
	printf("result : %d\n", adv_result);
	return;
}

static void utc_cb_charact_a_1(ble_server_attr_cb_type_e type, ble_conn_handle conn_handle, ble_attr_handle attr_handle, void *arg)
{
	char *arg_str = "None";
	if (arg != NULL) {
		arg_str = (char *)arg;
	}
}

static void utc_cb_desc_b_1(ble_server_attr_cb_type_e type, ble_conn_handle conn_handle, ble_attr_handle attr_handle, void *arg)
{
	char *arg_str = "None";
	if (arg != NULL) {
		arg_str = (char *)arg;
	}
}

static ble_server_gatt_t gatt_profile[] = {
	{
		.type = BLE_SERVER_GATT_SERVICE,
		.uuid = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01},
		.uuid_length = 16,
		.attr_handle = 0x006a,
	},

	{
		.type = BLE_SERVER_GATT_CHARACT, 
		.uuid = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x02}, 
		.uuid_length = 16, 
		.property = BLE_ATTR_PROP_RWN | BLE_ATTR_PROP_WRITE_NO_RSP, 
		.permission = BLE_ATTR_PERM_R_PERMIT | BLE_ATTR_PERM_W_PERMIT, 
		.attr_handle = 0x006b, 
		.cb = utc_cb_charact_a_1, 
		.arg = "char_a_1"
	},

	{
		.type = BLE_SERVER_GATT_DESC, 
		.uuid = {0x02, 0x29}, 
		.uuid_length = 2, 
		.permission = BLE_ATTR_PERM_R_PERMIT | BLE_ATTR_PERM_W_PERMIT, 
		.attr_handle = 0x006c, 
		.cb = utc_cb_desc_b_1, 
		.arg = "desc_b_1",
	},
};

static ble_server_init_config server_config = {
	ble_server_connected_cb,
	ble_server_disconnected_cb,
	ble_server_mtu_update_cb,
	ble_server_oneshot_adv_cb,
	true,
	gatt_profile, sizeof(gatt_profile) / sizeof(ble_server_gatt_t)};

static int ble_command(int argc, char *argv[])
{
	if (argc < 2) {
		ble_command_print_help_message();
		return 0;
	}

	if (strcmp(argv[1], "status") == 0) {
		ble_command_status();
	} else if (strcmp(argv[1], "client") == 0) {
		ble_command_client(argc, argv);
	} else if (strcmp(argv[1], "scan") == 0) {
		ble_command_scan(argc, argv);
	} else if (strcmp(argv[1], "filter") == 0) {
		ble_command_filter(argc, argv);
	} else if (strcmp(argv[1], "del") == 0) {
		ble_manager_delete_bonded_all();
	} else if (strcmp(argv[1], "test") == 0) {
		ble_command_test(argc, argv);
	} else if (strcmp(argv[1], "init") == 0) {
		str_to_ble_mac(argv[2], (uint8_t *)&target_addr[0].mac);
		ble_manager_init(&server_config);
	} else {
		ble_command_print_help_message();
	}
	return 0;
}

void ble_command_set_tash_cmd_install(void)
{
	tash_cmd_install("cble", ble_command, TASH_EXECMD_SYNC);
}
