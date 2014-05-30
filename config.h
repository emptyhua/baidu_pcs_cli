/*
 * Copyright (c) 2013 emptyhua@gmail.com
 */

#ifndef _BAIDU_PCS_CONFIG_H
#define _BAIDU_PCS_CONFIG_H

#include <stdint.h>
#include <errno.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "iniparser.h"

typedef struct pcs_config {
    char api_key[256];
    char api_secret[256];
} pcs_config_t, *pcs_config_ptr_t;

static char ini_path[][1024] = {
    "~/.baidu_pcs.ini",
    "./etc/baidu_pcs.ini"
};

static char* _PCSConfig_GetConfigFile(void) {
    int i;
    for(i = 0; i < sizeof(ini_path) / sizeof(ini_path[0]); i++){
        struct stat sts;
        if(stat(ini_path[i], &sts) == 0 && errno != ENOENT) {
            return ini_path[i];
        }
    }
    return NULL;
}

int PCSConfig_Get (pcs_config_t *config) {
    dictionary  *ini;

    char *ini_name = _PCSConfig_GetConfigFile();
    if (ini_name == NULL) {
        fprintf(stderr, "don't fine config file\n");
        int i;
        for(i = 0; i < sizeof(ini_path) / sizeof(ini_path[0]); i++){
            fprintf(stderr, "check config file path:%s\n", ini_path[i]);
        }
    }

    /* Some temporary variables to hold query results */
    ini = iniparser_load(ini_name);
    if (ini == NULL) {
        fprintf(stderr, "cannot parse file: %s\n", ini_name);
        return -1 ;
    }
    iniparser_dump(ini, stderr);

    /* Get baidu_pcs api_key, api_secret */
    printf("parse params:api_key, api_secret\n");

    char *api_key = iniparser_getstring(ini, "global:api_key", "not found");
    printf("global:api_key:%s\n", api_key);
    assert(strlen(api_key) < sizeof(config->api_key));
    strcpy(config->api_key, api_key);

    char *api_secret = iniparser_getstring(ini, "global:api_secret", "not found");
    printf("global:api_secret:%s\n", api_secret);
    assert(strlen(api_secret) < sizeof(config->api_secret));
    strcpy(config->api_secret, api_secret);

    iniparser_freedict(ini);
    return 0 ;
}

#endif
