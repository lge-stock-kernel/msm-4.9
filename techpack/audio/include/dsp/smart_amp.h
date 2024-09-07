#ifndef _SMART_AMP_H
#define _SMART_AMP_H

#include <linux/types.h>
#include <dsp/apr_audio-v2.h>
#include <linux/delay.h>


/* Below 3 should be same as in aDSP code */
#define AFE_PARAM_ID_SMARTAMP_DEFAULT   0x10001166
#define AFE_SMARTAMP_MODULE_RX          0x1000FC00  /*Rx module*/
#define AFE_SMARTAMP_MODULE_TX          0x1000FD00  /*Tx module*/

#define AFE_SMARTAMP_MODULE		AFE_SMARTAMP_MODULE_RX
#define SMART_AMP
#define MAX_DSP_PARAM_INDEX		600

#define TAS_GET_PARAM		1
#define TAS_SET_PARAM		0
#define TAS_PAYLOAD_SIZE	14
#define TAS_RX_PORT_ID		0x1016 //AFE_PORT_ID_QUINARY_MI2S_RX, refer to apr_audio-v2.h
#define TAS_TX_PORT_ID		0x1017 //AFE_PORT_ID_QUINARY_MI2S_TX
#define TAS_RX_PORT		TAS_RX_PORT_ID
#define TAS_TX_PORT		TAS_TX_PORT_ID
#define SLAVE1		0x98
#define SLAVE2		0x9A
#define SLAVE3		0x9C
#define SLAVE4		0x9E
#define SMARTAMP_STATUS_NORMAL 0
#define SMARTAMP_STATUS_BYPASS 1
#define SMARTAMP_STATUS_MUTE   2

#define AFE_SA_GET_F0          3810
#define AFE_SA_GET_Q           3811
#define AFE_SA_GET_TV          3812
#define AFE_SA_GET_RE          3813
#define AFE_SA_CALIB_INIT      3814
#define AFE_SA_CALIB_DEINIT    3815
#define AFE_SA_SET_RE          3816
#define AFE_SA_F0_TEST_INIT    3817
#define AFE_SA_F0_TEST_DEINIT  3818
#define AFE_SA_SET_PROFILE     3819
#define AFE_SA_SET_STATUS      3820



#define CAPI_V2_TAS_TX_ENABLE 0x10012D14
#define CAPI_V2_TAS_TX_CFG    0x10012D16

#define CAPI_V2_TAS_RX_ENABLE 0x10012D13
#define CAPI_V2_TAS_RX_CFG    0x10012D15

#define AFE_SA_IS_SPL_IDX(X)	((((X) >= 3810) && ((X) < 3899)) ? 1 : 0)

#define SA_MAX_PROFILES 5

struct afe_smartamp_set_params_t {
	uint32_t  payload[TAS_PAYLOAD_SIZE];
} __packed;

struct afe_smartamp_config_command {
	struct apr_hdr                      hdr;
	struct afe_port_cmd_set_param_v2    param;
	struct afe_port_param_data_v2       pdata;
	struct afe_smartamp_set_params_t  prot_config;
} __packed;

struct afe_smartamp_get_params_t {
    uint32_t payload[TAS_PAYLOAD_SIZE];
} __packed;

struct afe_smartamp_get_calib {
	struct apr_hdr hdr;
	struct afe_port_cmd_get_param_v2   get_param;
	struct afe_port_param_data_v2      pdata;
	struct afe_smartamp_get_params_t   res_cfg;
} __packed;

struct afe_smartamp_calib_get_resp {
	uint32_t status;
	struct afe_port_param_data_v2 pdata;
	struct afe_smartamp_get_params_t res_cfg;
} __packed;

int afe_smartamp_get_calib_data(struct afe_smartamp_get_calib *calib_resp,
		uint32_t param_id, uint32_t module_id);

int afe_smartamp_set_calib_data(uint32_t param_id,struct afe_smartamp_set_params_t *prot_config,
		uint8_t length, uint32_t module_id);

int afe_smartamp_algo_ctrl(u8 *data, uint32_t param_id, uint8_t get_set,
		int length, int module_id);

int smartamp_set_status(uint32_t st);

#endif
