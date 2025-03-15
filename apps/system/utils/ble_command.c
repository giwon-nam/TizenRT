#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <tinyara/pm/pm.h>

#include <ble_manager/ble_manager.h>
#include <apps/shell/tash.h>

#define BLE_LOG_VERBOSE printf
#define BLE_LOG_INFO printf
#define BLE_LOG_DEBUG(...)
#define BLE_LOG_ERROR printf

static ble_conn_handle g_ble_conn_handle = 0;
static uint8_t adv_handle = 200;
static ble_client_ctx *client_ctx = 0;

static void ble_command_print_help_message(void)
{
	char *help_message =
		"Usage :\n"
		"\tcble init\n"
		"\tcble pm\n"
		"\tcble init\n"
		"\tcble status\n"
		"\tcble del\n"
		"\tcble start\n"
		"\tcble create\n"
		"\tcble connect {mac}\n"
		"\tcble bond\n"
		"\tcble confirm\n";
	puts(help_message);
}

static void ble_command_print_mac(char *prefix, uint8_t *mac)
{
	BLE_LOG_INFO("%s%02x:%02x:%02x:%02x:%02x:%02x\n", prefix,
				 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void str_to_ble_mac(char *src, uint8_t *dest)
{
	for (int i = 0; i < 6; i++) {
		dest[i] = (uint8_t)strtol(src + i * 3, NULL, 16);
	}
}

static void ble_client_print_context(void)
{
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
}

static void ble_command_status(void)
{
	uint8_t ble_mac[BLE_BD_ADDR_MAX_LEN] = {0};
	ble_manager_get_mac_addr(ble_mac);

	ble_command_print_mac("BLE mac address of current device : ", ble_mac);

	ble_client_print_context();
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

static void ble_server_passkey_display_cb(uint32_t passkey, ble_conn_handle conn_handle)
{
	g_ble_conn_handle = conn_handle;
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
		.arg = "char_a_1",
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
	ble_server_passkey_display_cb,
	true,
	gatt_profile,
	sizeof(gatt_profile) / sizeof(ble_server_gatt_t),
};

static void ble_client_connected_cb(ble_client_ctx *ctx, ble_device_connected *connected_device)
{
	BLE_LOG_INFO("Connected. bonded : %d\n", connected_device->is_bonded);

	if (connected_device->is_bonded == false) {
		ble_manager_start_bond(ctx->conn_handle);
	}
	return;
}

static void ble_client_disconnected_cb(ble_client_ctx *ctx)
{
	return;
}

static void ble_client_notification_cb(ble_client_ctx *ctx, ble_attr_handle attr_handle, ble_data *read_result)
{
	return;
}

static void ble_client_indication_cb(ble_client_ctx *ctx, ble_attr_handle attr_handle, ble_data *read_result)
{
	return;
}

static void ble_client_passkey_cb(ble_client_ctx *ctx, uint32_t passkey, ble_conn_handle conn_handle)
{
	g_ble_conn_handle = conn_handle;
	return;
}

static ble_client_callback_list g_ble_client_callback = {
	.connected_cb = ble_client_connected_cb,
	.disconnected_cb = ble_client_disconnected_cb,
	.notification_cb = ble_client_notification_cb,
	.indication_cb = ble_client_indication_cb,
	.passkey_display_cb = ble_client_passkey_cb,
};

static int ble_command(int argc, char *argv[])
{
	if (argc < 2) {
		ble_command_print_help_message();
		return 0;
	}

	if (strcmp(argv[1], "status") == 0) {
		ble_command_status();
	} else if (strcmp(argv[1], "pm") == 0) {
		int fd = open(PM_DRVPATH, O_WRONLY);
		if (fd < 0) {
			return 0;
		}

		if (ioctl(fd, PMIOC_START, NULL) < 0) {
			close(fd);
			return 0;
		}

		close(fd);
	} else if (strcmp(argv[1], "del") == 0) {
		ble_manager_delete_bonded_all();
	} else if (strcmp(argv[1], "init") == 0) {
		ble_manager_init(&server_config);

		ble_sec_param sec_param = {
			.io_cap = 0x01,
			.oob_data_flag = 0,
			.bond_flag = 1,
			.mitm_flag = 1,
			.sec_pair_flag = 0x01,
			.use_fixed_key = 0,
			.fixed_key = 000000,
		};
		ble_manager_set_secure_param(&sec_param);
		uint32_t adv_interval[2] = {100, 100};
		uint8_t addr_val[6] = {0};
		ble_server_create_multi_adv(0x13, adv_interval, 0, addr_val, &adv_handle);
	} else if (strcmp(argv[1], "start") == 0) {
		ble_server_start_multi_adv(adv_handle);
	} else if (strcmp(argv[1], "confirm") == 0) {
		ble_manager_passkey_confirm(g_ble_conn_handle, true);
	} else if (strcmp(argv[1], "create") == 0) {
		client_ctx = ble_client_create_ctx(&g_ble_client_callback);
	} else if (strcmp(argv[1], "connect") == 0) {
		ble_conn_info conn_info = {0};
		conn_info.addr.type = BLE_ADDR_TYPE_PUBLIC;
		str_to_ble_mac(argv[2], (uint8_t *)conn_info.addr.mac);
		conn_info.conn_interval = 11;
		conn_info.slave_latency = 0;
		conn_info.mtu = 512;
		conn_info.scan_timeout = 5000;
		ble_command_print_mac("Connect to ", (uint8_t *)conn_info.addr.mac);
		ble_client_connect(client_ctx, &conn_info);
	} else if (strcmp(argv[1], "bond") == 0) {
		ble_manager_start_bond(client_ctx->conn_handle);
	} else if (strcmp(argv[1], "dis") == 0) {
		ble_client_disconnect(client_ctx);
	} else {
		ble_command_print_help_message();
	}
	return 0;
}

void ble_command_set_tash_cmd_install(void)
{
	tash_cmd_install("cble", ble_command, TASH_EXECMD_SYNC);
}

void dummy()
{
}
