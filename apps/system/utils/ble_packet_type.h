#pragma once

#include <ble_manager/ble_common.h>
#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BLE_ADV_DATA_SERVICE_ID_SAMSUNG_CONNECT 0x0C

typedef enum {
	BLE_ADV_DATA_FLAGS = 0x01,
	BLE_ADV_DATA_SHORTENED_LOCAL_NAME = 0x08,
	BLE_ADV_DATA_COMPLETE_LOCAL_NAME = 0x09,
	BLE_ADV_DATA_MANUFACTURER_SPECIFIC_DATA = 0xFF
} ble_adv_data_ad_type;

typedef enum {
	BLE_ADV_DATA_LE_LIMITED_DISCOVERABLE_MODE = 1 << 0,
	BLE_ADV_DATA_LE_GENERAL_DISCOVERABLE_MODE = 1 << 1,
	BLE_ADV_DATA_BR_EDR_NOT_SUPPORTED = 1 << 2,
	BLE_ADV_DATA_SIMULTANEOUS_LE_AND_BR_EDR_TO_SAME_DEVICE_CAPABLE_CONTROLLER = 1 << 3,
	BLE_ADV_DATA_SIMULTANEOUS_LE_AND_BR_EDR_TO_SAME_DEVICE_CAPABLE_HOST = 1 << 4
	// RFU
} ble_adv_data_flags;

typedef enum {
	BLE_ADV_DATA_WIFI = 1 << 0,
	BLE_ADV_DATA_L3 = 1 << 1,
	BLE_ADV_DATA_BLE = 1 << 2,
	BLE_ADV_DATA_P2P = 1 << 3,
	BLE_ADV_DATA_ZIGBEE = 1 << 4,
	// RFU
	BLE_ADV_DATA_BT_BLE_SAME_ADDRESS = 1 << 6,
	BLE_ADV_DATA_BLE_RANDOM_ADDRESS = 1 << 7
} ble_adv_data_setup_available_network;

typedef enum {
	BLE_ADV_DATA_BT_ADDR = 1 << 0,
	BLE_ADV_DATA_WIFI_P2P_ADDR = 1 << 1,
	BLE_ADV_DATA_WIFI_BSSID = 1 << 2
	// RFU
} ble_adv_data_address_type;

typedef enum {
	BLE_ADV_DATA_SETUP_ID = 0x01,
	BLE_ADV_DATA_SERIAL = 0x02,
	BLE_ADV_DATA_HYBRID_SERIAL = 0x03,
	BLE_ADV_DATA_OTM_SUPPORT_CONFIRM_METHOD = 0x04
} ble_adv_data_custom_data_type;

typedef uint8_t ble_adv_data_company_id_t[2];
typedef uint8_t ble_adv_data_manufacturer_id_t[4];
typedef uint8_t ble_adv_data_setup_id_t[3];
typedef uint8_t ble_adv_data_mac_addr_t[6];
typedef uint8_t ble_adv_data_serial_t[4];
typedef uint8_t ble_adv_data_device_name_t[BLE_BD_ADDR_STR_LEN];

typedef struct {
	uint8_t version : 4;
	uint8_t RFU : 2;
	uint8_t active_scan : 1;	   // 1: active scan required
	uint8_t multiple_services : 1; // 0 : single service, 1: multiple services
} ble_adv_data_control_and_version_t;

typedef struct {
	uint8_t version : 4;
	uint8_t mode : 3;	  // 0: setup mode, 1: opertaion mode
	uint8_t features : 1; // 0: using all predefined fucntion fields except features field, 1: using predefined function fields set in features field
} ble_adv_data_samsung_connect_packet_version_t;

typedef struct {
	uint8_t onboarding_supported : 1;
	uint8_t owned_state : 1;
	uint8_t onboarding_available : 1;
	uint8_t d2d_device : 1; // 0: D2S_DEVICE, 1: D2D Device
	uint8_t oob : 1;
	uint8_t two_factor_authentication_onboarding_support : 1; // 0: not supported, 1: supported. first_ownership_authentication_status bit is valid
	uint8_t calm_mode_support : 1;
	uint8_t first_ownership_authentication_status : 1; // 0: First Ownership Authentication ready, 1: First Ownership Authentication Completed
} ble_adv_data_onboarding_info_t;

typedef struct {
	uint8_t setup_info : 1;
	uint8_t device_info : 1;
	uint8_t device_status : 1;
	uint8_t setup_available_network : 1;
	uint8_t address : 1;
	uint8_t channel_info : 1;
	uint8_t custom_data : 1;
	uint8_t name : 1;
} ble_adv_data_features_t;

typedef struct {
	uint8_t device_on : 1;
	uint8_t ap_connected : 1;
	uint8_t health_status : 1;
	uint8_t p2p_connected : 1;
	uint8_t p2p_connection_ready : 1;
	uint8_t setup : 1;
	uint8_t samsung_account_required : 1;
	uint8_t RFU : 1;
} ble_adv_data_device_status_t;

typedef struct {
	uint8_t wifi : 1;
	uint8_t l3 : 1;
	uint8_t ble : 1;
	uint8_t p2p : 1;
	uint8_t zigbee : 1;
	uint8_t RFU : 1;
	uint8_t bt_ble_same_address : 1;
	uint8_t ble_random_address : 1;
} ble_adv_data_setup_available_network_t;

typedef struct {
	uint8_t support_ultrasound_pin_when_perform_random_pin_otm : 1;
	uint8_t support_auto_confirm_via_ultrasound_pin : 1;
	uint8_t ir_confirm_otm_setup : 1;
	uint8_t ble_ranging_based_otm : 1;
	uint8_t RFU1 : 4;
	uint8_t RFU2 : 3;
	uint8_t preceding_confirm_otm_setup : 1;
	uint8_t RFU3 : 4;
} ble_adv_data_otm_support_confirm_method_t;

typedef struct {
	uint8_t flags_len;
	ble_adv_data_ad_type flags_type : 8;
	ble_adv_data_flags flags_value : 8;
	uint8_t manufacturer_specific_data_len;
	ble_adv_data_ad_type manufacturer_specific_data_type : 8;
	ble_adv_data_company_id_t company_id;
	ble_adv_data_control_and_version_t control_and_version;
	uint8_t service_id;
	ble_adv_data_samsung_connect_packet_version_t samsung_connect_packet_version;
	ble_adv_data_onboarding_info_t onboarding_info;
	ble_adv_data_features_t features;
	ble_adv_data_manufacturer_id_t setup_info_manufacturer_id;
	ble_adv_data_setup_id_t setup_info_setup_id;
	ble_adv_data_device_status_t device_status;
	ble_adv_data_setup_available_network_t setup_available_network;
	ble_adv_data_address_type address_type : 8;
	ble_adv_data_mac_addr_t address_addr1;
	uint8_t custom_data_len;
	ble_adv_data_custom_data_type serial_type : 8;
	uint8_t serial_len;
} ble_adv_ind_t;

typedef struct {
	uint8_t manufacturer_specific_data_len;
	ble_adv_data_ad_type manufacturer_specific_data_type : 8;
	ble_adv_data_company_id_t company_id;
	ble_adv_data_serial_t serial_value;
	ble_adv_data_custom_data_type otm_support_confirm_method_type : 8;
	uint8_t otm_support_confirm_method_len;
	ble_adv_data_otm_support_confirm_method_t otm_support_confirm_method_value;
	uint8_t device_name_len;
	ble_adv_data_ad_type device_name_type : 8;
	ble_adv_data_device_name_t device_name_value;
} ble_scan_resp_t;

#ifdef __cplusplus
}
#endif
