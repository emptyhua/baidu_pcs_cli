/*
 * Copyright (c) 2013 emptyhua@gmail.com
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <libgen.h>
#include "pcs.h"

#define MIN(a, b) ((a) > (b) ? (b) : (a))
#define MAX(a, b) ((a) > (b) ? (a) : (b))


#define MAKE_JSON() \
error = HttpClient_GetError(client); \
if (error != NULL) { \
    BaiduPCS_ThrowError(api, "request failed: %s", error); \
    goto free; \
} \
response = HttpClient_ResponseText(client); \
json = cJSON_Parse(response); \
if (json == NULL) { \
    BaiduPCS_ThrowError(api, "JSON parse error"); \
    goto free; \
}\
item = cJSON_GetObjectItem(json, "error_code"); \
if (item != NULL && item->type == cJSON_Number) { \
    api->error_code = item->valueint; \
    item = cJSON_GetObjectItem(json, "error_msg"); \
    BaiduPCS_ThrowError(api, "%d-%s", api->error_code, item->valuestring); \
    goto free; \
}

static void _BaiduPCS_Json2File(BaiduPCS *api, PCSFile *file, cJSON *array) {
//{{{
    cJSON *item;

    item = cJSON_GetObjectItem(array, "path");
    if (item == NULL || item->type != cJSON_String) {
        BaiduPCS_ThrowError(api, "json.list.path is not string");
        goto free;
    }
    PCSFile_SetPath(file, item->valuestring);

    //获取尺寸
    item = cJSON_GetObjectItem(array, "size");
    if (item == NULL || item->type != cJSON_Number) {
        BaiduPCS_ThrowError(api, "json.list.size is not number");
        goto free;
    }
    file->size = (uint64_t)item->valueint;

    //是否为目录
    item = cJSON_GetObjectItem(array, "isdir");
    if (item != NULL && item->type == cJSON_Number) {
        file->is_dir = (char)item->valueint;
    }

    //创建时间
    item = cJSON_GetObjectItem(array, "ctime");
    if (item == NULL || item->type != cJSON_Number) {
        BaiduPCS_ThrowError(api, "json.list.ctime is not number");
        goto free;
    }
    file->ctime = (unsigned int)item->valueint;

    //修改时间
    item = cJSON_GetObjectItem(array, "mtime");
    if (item == NULL || item->type != cJSON_Number) {
        BaiduPCS_ThrowError(api, "json.list.mtime is not number");
        goto free;
    }
    file->mtime = (unsigned int)item->valueint;

free:;
}
//}}}

/* 新建PCS API */
BaiduPCS *BaiduPCS_New() {
//{{{
    BaiduPCS *api = calloc(sizeof(BaiduPCS), 1);
    api->client = HttpClient_New();
    return api;
}
//}}}

/* 释放API */
void BaiduPCS_Free(BaiduPCS *api) {
//{{{
    if (api == NULL) return;
    HttpClient_Free(api->client);
    free(api);
}
//}}}

/* 获取API错误信息 */
const char *BaiduPCS_GetError(BaiduPCS *api) {
//{{{
    if (api->error[0] == '\0') return NULL;
    return api->error;
}
//}}}

/* Oauth认证,返回token */
const char *BaiduPCS_Auth(BaiduPCS *api) {
//{{{
    HttpClient *client  = api->client;
    char *url_buffer    = api->util_buffer0;
    const char *error   = NULL;

    const char *response            = NULL;
    cJSON *json                     = NULL;
    cJSON *json2                    = NULL;
    cJSON *item                     = NULL;

    const char *device_code         = NULL;
    const char *user_code           = NULL;
    const char *verification_url    = NULL;
    const char *qrcode_url          = NULL;
    int interval                    = 0;

    BaiduPCS_ResetError(api);

    sprintf(url_buffer, "https://openapi.baidu.com/oauth/2.0/device/code?client_id=%s"
           "&response_type=device_code"
           "&scope=basic,netdisk", api->key);

#ifdef DEBUG
    fprintf(stderr, "request %s\n", url_buffer);
#endif

    HttpClient_Init(client);
    HttpClient_Get(client, url_buffer);
    error = HttpClient_GetError(client);
    if (error != NULL) {
        BaiduPCS_ThrowError(api, "http request failed: %s", error);
        goto free;
    }

    response = HttpClient_ResponseText(client);
    json = cJSON_Parse(response);

    if (json == NULL) {
        BaiduPCS_ThrowError(api, "JSON解析失败");
        goto free;
    }

    item = cJSON_GetObjectItem(json, "error_description");
    if (item != NULL && item->type == cJSON_String) {
        BaiduPCS_ThrowError(api, "%s", item->valuestring);
        goto free;
    }

    item = cJSON_GetObjectItem(json, "device_code");
    if (item == NULL || item->type != cJSON_String) {
        BaiduPCS_ThrowError(api, "无法获取device_code");
        goto free;
    }
    device_code = item->valuestring;

    item = cJSON_GetObjectItem(json, "user_code");
    if (item == NULL || item->type != cJSON_String) {
        BaiduPCS_ThrowError(api, "无法获取user_code");
        goto free;
    }
    user_code = item->valuestring;

    item = cJSON_GetObjectItem(json, "verification_url");
    if (item == NULL || item->type != cJSON_String) {
        BaiduPCS_ThrowError(api, "无法获取verification_url");
        goto free;
    }
    verification_url = item->valuestring;

    item = cJSON_GetObjectItem(json, "qrcode_url");
    if (item == NULL || item->type != cJSON_String) {
        BaiduPCS_ThrowError(api, "无法获取qrcode_url");
        goto free;
    }
    qrcode_url = item->valuestring;

    item = cJSON_GetObjectItem(json, "interval");
    if (item == NULL || item->type != cJSON_Number) {
        BaiduPCS_ThrowError(api, "无法获取interval");
        goto free;
    }
    interval = item->valueint;


    printf("授权码:%s\n"
    "请访问Web授权地址进行授权:%s\n"
    "二维码授权:%s\n",
        user_code,
        verification_url,
        qrcode_url);

    sprintf(url_buffer, "https://openapi.baidu.com/oauth/2.0/token?client_id=%s"
            "&grant_type=device_token"
            "&code=%s"
            "&client_secret=%s"
            , api->key
            , device_code
            , api->secret);

#ifdef DEBUG
    fprintf(stderr, "request %s\n", url_buffer);
#endif

    while(1) {
        printf("等待验证....\n");
        sleep(interval);

        HttpClient_Init(client);
        HttpClient_Get(client, url_buffer);
        error = HttpClient_GetError(client);
        if (error != NULL) {
            BaiduPCS_ThrowError(api, "http request failed: %s", error);
            goto free;
        }

        response = HttpClient_ResponseText(client);
        json2 = cJSON_Parse(response);
        if (json2 == NULL) {
            BaiduPCS_ThrowError(api, "JSON解析失败");
            goto free;
        }

        item = cJSON_GetObjectItem(json2, "error");
        if (item != NULL && item->type == cJSON_String) {
            if (strcmp(item->valuestring, "authorization_pending") == 0) {
                cJSON_Delete(json2);
                json2 = NULL;
                continue;
            } else {
                BaiduPCS_ThrowError(api, "%s", item->valuestring);
                cJSON_Delete(json2);
                json2 = NULL;
                goto free;
            }
        }

        item = cJSON_GetObjectItem(json2, "access_token");
        if (item != NULL && item->type == cJSON_String) {
            sprintf(api->token, "%s", item->valuestring);
            cJSON_Delete(json2);
            json2 = NULL;
            break;
        }

        cJSON_Delete(json2);
        json2 = NULL;
    }


free:
    if (json != NULL) {
        cJSON_Delete(json);
    }

    if (api->token[0] == '\0') return NULL;
    return api->token;
}
//}}}

/* 获取空间的用量 */
void BaiduPCS_Info(BaiduPCS *api, BaiduPCSInfo *info) {
//{{{
    if (info == NULL) return;
    HttpClient *client      = api->client;
    char *url_buffer        = api->util_buffer0;
    const char *token       = api->token;
    const char *error       = NULL;
    const char *response    = NULL;
    cJSON *json             = NULL; //需要释放
    cJSON *item             = NULL;

    BaiduPCS_ResetError(api);

    sprintf(url_buffer, "https://pcs.baidu.com/rest/2.0/pcs/quota?"
           "access_token=%s"
           "&method=info", token);

    HttpClient_Init(client);
    HttpClient_Get(client, url_buffer);

    MAKE_JSON();

    item = cJSON_GetObjectItem(json, "quota");
    if (item == NULL || item->type != cJSON_Number) {
        BaiduPCS_ThrowError(api, "can't find json.quota");
        goto free;
    }

    info->quota = (uint64_t)item->valueint;

    item = cJSON_GetObjectItem(json, "used");
    if (item == NULL || item->type != cJSON_Number) {
        BaiduPCS_ThrowError(api, "can't find json.used");
        goto free;
    }

    info->used = (uint64_t)item->valueint;
free:
    if (json != NULL) {
        cJSON_Delete(json);
    }
}
//}}}

//本地文件分片
static void local_file_split(PCSFile *file, uint64_t split_threshold) {
//{{{
    uint64_t offset               = 0;
    PCSFileBlock *last_block    = NULL;
    PCSFileBlock *block         = NULL;

    if (file->size > split_threshold) {
        file->block_size = split_threshold;

        while(offset < file->size) {
            block = PCSFileBlock_New();
            block->offset = offset;

            if (offset + split_threshold > file->size) {
                block->size = file->size - offset;
            } else {
                block->size = split_threshold;
            }

            if (file->block == NULL) {
                file->block = block;
            }

            if (last_block != NULL) {
                last_block->next = block;
            }
            last_block = block;

            offset += split_threshold;
        }
    }
}
//}}}

static size_t _BaiduPCS_UploadReadCallback(void *ptr, size_t size, size_t nitems, void *userdata) {
//{{{
    PCSFileBlock *block = (PCSFileBlock *)userdata;
    int len = size * nitems;
    int left = block->size - block->readed_size;
    if (left < len) {
        len = left;
    }
    if (!len) return 0;
    len = fread(ptr, 1, len, block->fp);
    block->readed_size += len;
#ifdef DEBUG
    fprintf(stderr, "block size %lld, uploaded %lld\n",
            (unsigned long long)block->size,
            (unsigned long long)block->readed_size);
#endif
    return len;
}
//}}}

static void _BaiduPCS_UploadResetCallback(void *userdata) {
//{{{
    PCSFileBlock *block = (PCSFileBlock *)userdata;
    fseek(block->fp, block->offset, SEEK_SET);
    block->readed_size = 0;
}
//}}}

static PCSFile *_BaiduPCS_UploadBigFile(
BaiduPCS *api,
PCSFile *local_file,
const char *remote_file,
const char *ondup
) {
//{{{
    PCSFile *result     = NULL;
    HttpClient *client  = api->client;
    char *url_buffer    = api->util_buffer0;
    const char *token   = api->token;
    FILE *fp            = NULL;
    PCSFileBlock *block = NULL;
    int i               = 0;
    int timeout         = 0;

    const char *error   = NULL;
    char *tmp           = NULL;
    const char *response      = NULL;
    char *remote_path_encode  = NULL;

    struct curl_httppost *post = NULL;
    struct curl_httppost *last = NULL;

    cJSON *json         = NULL;
    cJSON *item         = NULL;
    cJSON *array        = NULL;

    sprintf(url_buffer, "https://c.pcs.baidu.com/rest/2.0/pcs/file?"
                    "access_token=%s"
                    "&method=upload"
                    "&type=tmpfile", token);

    fp = fopen(local_file->path, "rb");
    if (fp == NULL) {
        BaiduPCS_ThrowError(api, "本地文件无法读取 %s", local_file->path);
        goto free;
    }

    //上传切片
    i = 0;
    block = local_file->block;
    while (block != NULL) {

#ifdef DEBUG
        fprintf(stderr, "开始上传分片%d\n", i);
#endif
        block->fp = fp;

        HttpClient_Init(client);
        curl_formadd(&post, &last,
                     CURLFORM_COPYNAME, "file",
                     CURLFORM_STREAM, block,
                     CURLFORM_CONTENTSLENGTH, block->size,
                     CURLFORM_CONTENTTYPE, "application/octet-stream",
                     CURLFORM_FILENAME, basename(local_file->path),
                     CURLFORM_END);

        curl_easy_setopt(client->curl, CURLOPT_READFUNCTION, _BaiduPCS_UploadReadCallback);
        /* 失败重试前，重置block */
        HttpClient_SetFailRetryCallback(client, _BaiduPCS_UploadResetCallback, block);
        /* 设置一个较为合理的超时时间 */
        timeout = MAX(20, block->size / (10 * 1024));
        curl_easy_setopt(client->curl, CURLOPT_TIMEOUT, timeout);

        HttpClient_PostHttpData(client, url_buffer, post);

        MAKE_JSON();

        item = cJSON_GetObjectItem(json, "md5");
        if (item == NULL || item->type != cJSON_String) {
            BaiduPCS_ThrowError(api, "can't find json.md5");
            goto free;
        }

        sprintf(block->md5, "%s", item->valuestring);

        cJSON_Delete(json);
        json = NULL;

        curl_formfree(post);
        post = NULL;
        last = NULL;

#ifdef DEBUG
        fprintf(stderr, "分片MD5 %s\n", block->md5);
#endif

        block = block->next;
        i ++;

    }

    remote_path_encode = curl_easy_escape(client->curl, remote_file, 0);
    //合并文件分片
    sprintf(url_buffer, "https://c.pcs.baidu.com/rest/2.0/pcs/file?"
                    "access_token=%s"
                    "&method=createsuperfile"
                    "&path=%s"
                    "&ondup=%s", token, remote_path_encode, ondup);
    curl_free(remote_path_encode);
    remote_path_encode = NULL;

#ifdef DEBUG
    fprintf(stderr, "request %s\n", url_buffer);
#endif

    array = cJSON_CreateArray();
    block = local_file->block;
    while (block != NULL) {
        cJSON_AddItemToArray(array, cJSON_CreateString(block->md5));
        block = block->next;
    }

    json = cJSON_CreateObject();
    cJSON_AddItemToObject(json, "block_list", array);
    tmp = cJSON_Print(json);

#ifdef DEBUG
        fprintf(stderr, "param %s\n", tmp);
#endif

    curl_formadd(&post, &last,
                     CURLFORM_COPYNAME, "param",
                     CURLFORM_COPYCONTENTS, tmp,
                     CURLFORM_END);

    cJSON_Delete(json);
    json = NULL;
    free(tmp);
    tmp = NULL;

    HttpClient_Init(client);
    HttpClient_PostHttpData(client, url_buffer, post);

    MAKE_JSON();

     result = PCSFile_New();
    _BaiduPCS_Json2File(api, result, json);

    if (api->error[0] != '\0') {
        PCSFile_Free(result);
        result = NULL;
    }

free:
    if (post != NULL) {
        curl_formfree(post);
    }

    if (fp != NULL) {
        fclose(fp);
    }

    if (json != NULL) {
        cJSON_Delete(json);
    }

    return result;
}
//}}}

static PCSFile *_BaiduPCS_UploadSmallFile(
BaiduPCS *api,
PCSFile *local_file,
const char *remote_file,
const char *ondup
) {
//{{{
    PCSFile *result     = NULL;
    HttpClient *client  = api->client;
    char *url_buffer    = api->util_buffer0;
    const char *token   = api->token;
    int timeout         = 0;

    const char *error   = NULL;
    const char *response      = NULL;
    char *remote_path_encode  = NULL;

    struct curl_httppost *post = NULL;
    struct curl_httppost *last = NULL;

    cJSON *json         = NULL;
    cJSON *item         = NULL;

    remote_path_encode = curl_easy_escape(client->curl, remote_file, 0);
    //合并文件分片
    sprintf(url_buffer, "https://c.pcs.baidu.com/rest/2.0/pcs/file?"
                    "access_token=%s"
                    "&method=upload"
                    "&path=%s"
                    "&ondup=%s", token, remote_path_encode, ondup);
    curl_free(remote_path_encode);
    remote_path_encode = NULL;

#ifdef DEBUG
    fprintf(stderr, "开始上传小文件\n");
#endif

    HttpClient_Init(client);
    curl_formadd(&post, &last,
                 CURLFORM_COPYNAME, "file",
                 CURLFORM_FILE, local_file->path,
                 CURLFORM_END);

    curl_easy_setopt(client->curl, CURLOPT_READFUNCTION, _BaiduPCS_UploadReadCallback);
    /* 设置一个较为合理的超时时间 */
    timeout = MAX(20, local_file->size / (10 * 1024));
    curl_easy_setopt(client->curl, CURLOPT_TIMEOUT, timeout);

    HttpClient_PostHttpData(client, url_buffer, post);
    curl_formfree(post);
    post = NULL;
    last = NULL;

    MAKE_JSON();


    result = PCSFile_New();
    _BaiduPCS_Json2File(api, result, json);

    if (api->error[0] != '\0') {
        PCSFile_Free(result);
        result = NULL;
    }

free:
    if (post != NULL) {
        curl_formfree(post);
    }

    if (json != NULL) {
        cJSON_Delete(json);
    }
    return result;
}
//}}}


/* 上传单个文件 */
PCSFile *BaiduPCS_Upload(BaiduPCS *api,
    PCSFile *local_file,
    const char *remote_file,
    uint64_t split_threshold,
    const char *ondup
) {
//{{{
    /* 最大分片 2G  */
    uint64_t max_split_size = 2 * 1024 * 1024 * (uint64_t)1024;
    /* 最小分片 10M */
    uint64_t min_split_size = 10 * 1024 * 1024;

    BaiduPCS_ResetError(api);

    if (local_file == NULL) {
        BaiduPCS_ThrowError(api, "请指定本地文件");
        goto free;
    }

    if (split_threshold < min_split_size) {
        split_threshold = min_split_size;
    } else if (split_threshold > max_split_size) {
        split_threshold = max_split_size;
    }


    //文件切片
    local_file_split(local_file, split_threshold);

    if (local_file->block != NULL) {
        return _BaiduPCS_UploadBigFile(api, local_file, remote_file, ondup);
    } else {
        return _BaiduPCS_UploadSmallFile(api, local_file, remote_file, ondup);
    }
free:
    return NULL;
}
//}}}

static size_t _BaiduPCS_Download_WriteData(void *ptr, size_t size, size_t nmemb, void *data) {
//{{{
    FILE *fp = (FILE *)data;
    int ret = fwrite(ptr, size, nmemb, fp);
    return ret;
}
//}}}


/* 下载单个文件 */
void BaiduPCS_Download(BaiduPCS *api, const char *remote_file, FILE *local_fp) {
//{{{
    if (remote_file == NULL || local_fp == NULL) return;

    HttpClient *client          = api->client;
    char *url_buffer            = api->util_buffer0;
    const char *token           = api->token;
    const char *error           = NULL;
    char *remote_path_encode    = NULL;

    BaiduPCS_ResetError(api);

    remote_path_encode = curl_easy_escape(client->curl, remote_file, 0);
    sprintf(url_buffer, "https://d.pcs.baidu.com/rest/2.0/pcs/file?"
           "access_token=%s"
           "&method=download"
           "&path=%s", token, remote_path_encode);
    curl_free(remote_path_encode);
    remote_path_encode = NULL;

#ifdef DEBUG
    fprintf(stderr, "request %s\n", url_buffer);
#endif

    HttpClient_Init(client);
    HttpClient_SetFailRetry(client, 0, 0);
    curl_easy_setopt(client->curl, CURLOPT_WRITEFUNCTION, _BaiduPCS_Download_WriteData);
    curl_easy_setopt(client->curl, CURLOPT_WRITEDATA, local_fp);
    /* 跟随重定向 */
    curl_easy_setopt(client->curl, CURLOPT_FOLLOWLOCATION, 1);
    /* 下载木有超时 */
    curl_easy_setopt(client->curl, CURLOPT_TIMEOUT, 0);
    HttpClient_Get(client, url_buffer);

    error = HttpClient_GetError(client);
    if (error != NULL) {
        BaiduPCS_ThrowError(api, "http request failed: %s", error);
    }
}
//}}}

/* 移动目录/文件 */
void BaiduPCS_Move(BaiduPCS *api, const char *remote_from, const char * remote_to) {
//{{{
    HttpClient *client      = api->client;
    char *url_buffer        = api->util_buffer0;
    const char *token       = api->token;
    const char *error       = NULL;
    const char *response    = NULL;
    cJSON *json             = NULL; /* 需要释放 */
    cJSON *item             = NULL;
    char *from_encode       = NULL;
    char *to_encode         = NULL;

    BaiduPCS_ResetError(api);

    from_encode = curl_easy_escape(client->curl, remote_from, 0);
    to_encode   = curl_easy_escape(client->curl, remote_to, 0);
    sprintf(url_buffer, "https://pcs.baidu.com/rest/2.0/pcs/file?"
           "access_token=%s"
           "&method=move"
           "&from=%s"
           "&to=%s"
           , token, from_encode, to_encode);
    curl_free(from_encode);
    curl_free(to_encode);
    from_encode = NULL;
    to_encode   = NULL;

#ifdef DEBUG
    fprintf(stderr, "reqeust %s\n", url_buffer);
#endif


    HttpClient_Init(client);
    HttpClient_PostHttpData(client, url_buffer, NULL);

    MAKE_JSON();

free:
    if (json != NULL) {
        cJSON_Delete(json);
    }
}
//}}}

/* 复制目录/文件 */
void BaiduPCS_Copy(BaiduPCS *api, const char *remote_from, const char * remote_to) {
//{{{
    HttpClient *client      = api->client;
    char *url_buffer        = api->util_buffer0;
    const char *token       = api->token;
    const char *error       = NULL;
    const char *response    = NULL;
    cJSON *json             = NULL; //需要释放
    cJSON *item             = NULL;
    char *from_encode       = NULL;
    char *to_encode         = NULL;

    BaiduPCS_ResetError(api);

    from_encode = curl_easy_escape(client->curl, remote_from, 0);
    to_encode   = curl_easy_escape(client->curl, remote_to, 0);
    sprintf(url_buffer, "https://pcs.baidu.com/rest/2.0/pcs/file?"
           "access_token=%s"
           "&method=copy"
           "&from=%s"
           "&to=%s", token, from_encode, to_encode);
    curl_free(from_encode);
    curl_free(to_encode);
    from_encode = NULL;
    to_encode   = NULL;

#ifdef DEBUG
    fprintf(stderr, "reqeust %s\n", url_buffer);
#endif

    HttpClient_Init(client);
    HttpClient_PostHttpData(client, url_buffer, NULL);

    MAKE_JSON();

free:
    if (json != NULL) {
        cJSON_Delete(json);
    }
}
//}}}

/* 删除目录/文件 */
void BaiduPCS_Remove(BaiduPCS *api, const char *remote_file) {
//{{{
    HttpClient *client      = api->client;
    char *url_buffer        = api->util_buffer0;
    const char *token       = api->token;
    const char *error       = NULL;
    const char *response    = NULL;
    cJSON *json             = NULL; //需要释放
    cJSON *item             = NULL;
    char *path_encode       = NULL;

    BaiduPCS_ResetError(api);

    path_encode = curl_easy_escape(client->curl, remote_file, 0);
    sprintf(url_buffer, "https://pcs.baidu.com/rest/2.0/pcs/file?"
           "access_token=%s"
           "&method=delete"
           "&path=%s", token, path_encode);
    curl_free(path_encode);
    path_encode = NULL;

#ifdef DEBUG
    fprintf(stderr, "reqeust %s\n", url_buffer);
#endif

    HttpClient_Init(client);
    HttpClient_PostHttpData(client, url_buffer, NULL);

    MAKE_JSON();

free:
    if (json != NULL) {
        cJSON_Delete(json);
    }
}
//}}}

PCSFile *BaiduPCS_NewRemoteFile(BaiduPCS *api, const char *path) {
//{{{
    PCSFile *file           = NULL;
    HttpClient *client      = api->client;
    char *url_buffer        = api->util_buffer0;
    const char *token       = api->token;
    const char *error       = NULL;
    const char *response    = NULL;
    char *path_encode       = NULL;
    cJSON *json             = NULL;
    cJSON *array            = NULL;
    cJSON *item             = NULL;

    BaiduPCS_ResetError(api);

    file          = PCSFile_New();

    path_encode = curl_easy_escape(client->curl, path, 0);
    sprintf(url_buffer, "https://pcs.baidu.com/rest/2.0/pcs/file?"
           "access_token=%s"
           "&method=meta"
           "&path=%s", token, path_encode);
    curl_free(path_encode);
    path_encode = NULL;

#ifdef DEBUG
    fprintf(stderr, "request %s\n", url_buffer);
#endif

    HttpClient_Init(client);
    HttpClient_Get(client, url_buffer);

    MAKE_JSON();

    item = cJSON_GetObjectItem(json, "list");
    if (item == NULL || item->type != cJSON_Array) {
        BaiduPCS_ThrowError(api, "can't find json.list");
        goto free;
    }

    array = item;
    if (cJSON_GetArraySize(array) == 0) {
        BaiduPCS_ThrowError(api, "json.list is blank");
        goto free;
    }

    item = cJSON_GetArrayItem(array, 0);
    if (item == NULL || item->type != cJSON_Object) {
        BaiduPCS_ThrowError(api, "json.list.item is not object");
        goto free;
    }

    _BaiduPCS_Json2File(api, file, item);

free:

    if (json != NULL) {
        cJSON_Delete(json);
    }

    if (api->error[0] != '\0') {
        if (file != NULL) {
            PCSFile_Free(file);
        }
        return NULL;
    }

    return file;
}
//}}}

PCSFileList *BaiduPCS_ListRemoteDir(BaiduPCS *api, const char *path) {
//{{{
    PCSFileList *list       = NULL;
    PCSFile *file           = NULL;
    HttpClient *client      = api->client;
    char *url_buffer        = api->util_buffer0;
    const char *token       = api->token;
    const char *error       = NULL;
    const char *response    = NULL;
    char *path_encode       = NULL;
    cJSON *json             = NULL;
    cJSON *array            = NULL;
    cJSON *item             = NULL;
    int i                   = 0;
    int length              = 0;

    BaiduPCS_ResetError(api);

    list          = PCSFileList_New();

    path_encode = curl_easy_escape(client->curl, path, 0);
    sprintf(url_buffer, "https://pcs.baidu.com/rest/2.0/pcs/file?"
           "access_token=%s"
           "&method=list"
           "&path=%s", token, path_encode);
    curl_free(path_encode);

#ifdef DEBUG
    fprintf(stderr, "request %s\n", url_buffer);
#endif

    HttpClient_Init(client);
    HttpClient_Get(client, url_buffer);

    MAKE_JSON();

    item = cJSON_GetObjectItem(json, "list");
    if (item == NULL || item->type != cJSON_Array) {
        BaiduPCS_ThrowError(api, "can't find json.list");
        goto free;
    }

    array = item;
    length = cJSON_GetArraySize(array);

    for (i = 0; i < length; i ++) {
        item = cJSON_GetArrayItem(array, i);
        if (item == NULL || item->type != cJSON_Object) {
            BaiduPCS_ThrowError(api, "json.list.item is not object");
            goto free;
        }

        file = PCSFile_New();
        PCSFileList_Prepend(list, file);
        _BaiduPCS_Json2File(api, file, item);
        if (BaiduPCS_GetError(api) != NULL) {
        }
    }

free:

    if (json != NULL) {
        cJSON_Delete(json);
    }

    return list;
}
//}}}


/*
* Local variables:
* tab-width: 4
* c-basic-offset: 4
* End:
* vim600: et sw=4 ts=4 fdm=marker
* vim<600: et sw=4 ts=4
*/
