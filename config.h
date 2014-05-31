#ifndef _BAIDU_PCS_CONFIG_H
#define _BAIDU_PCS_CONFIG_H

typedef struct pcs_config {
    char api_key[256];
    char api_secret[256];
} pcs_config_t, *pcs_config_ptr_t;

int PCSConfig_Get (pcs_config_t *config);

#endif
