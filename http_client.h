/*
 * Copyright (c) 2013 emptyhua@gmail.com
 */

#ifndef _HTTP_CLIENT_H
#define _HTTP_CLIENT_H

#include <curl/curl.h>

typedef struct HttpBuffer_s HttpBuffer;

struct HttpBuffer_s {
    char *content;
    size_t size;            /*buffer尺寸*/
    size_t used;            /*已使用字节数*/
    HttpBuffer *next;       /*链表*/
};

typedef struct HttpClient_s {
    CURL *curl;                 /*CURL          */
    
    int retry_times;            /*失败重试次数  */
    int retry_interval;         /*重试频率,秒   */
    int fail_times;             /*失败次数      */

    HttpBuffer *responseText;   /*body缓冲区    */
    HttpBuffer *responseHeader; /*header缓冲区  */
    char *c_responseText;
    char *c_responseHeader;

    void *fail_reset_callback;  /*失败重置回调  */
    void *fail_reset_context;   /*失败重置上下文*/

    CURLcode resultCode;

    int trace_ascii;
} HttpClient;


HttpBuffer* HttpBuffer_New();
void   HttpBuffer_Free(HttpBuffer *buffer);
void   HttpBuffer_Append(HttpBuffer *buffer, const char *input, size_t size); 
size_t HttpBuffer_Length(HttpBuffer *buffer);
void   HttpBuffer_Empty(HttpBuffer *buffer);
size_t HttpBuffer_ToChar(HttpBuffer *buffer, char *content);
void   HttpBuffer_Dump(HttpBuffer *buffer);

HttpClient* HttpClient_New();
void HttpClient_Free(HttpClient *client);

void  HttpClient_Init(HttpClient *client);
void  HttpClient_SetFailRetryCallback(HttpClient *client, void *callback, void *context);
void  HttpClient_SetFailRetry(HttpClient *client, int retry_times, int retry_interval);
void  HttpClient_SetWillGetHeader(HttpClient *client);
void  HttpClient_SetDebug(HttpClient *client, int trace_ascii);

CURLcode HttpClient_Get(HttpClient *client, const char *url);
CURLcode HttpClient_PostHttpData(HttpClient *client, const char *url, struct curl_httppost *data);

const char *HttpClient_GetError(HttpClient *client);
const char *HttpClient_ResponseText(HttpClient *client);
const char *HttpClient_ResponseHeader(HttpClient *client);
long HttpClient_ResponseCode(HttpClient *client);

#endif
