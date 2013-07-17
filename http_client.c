/*
 * Copyright (c) 2013 emptyhua@gmail.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "http_client.h"

#define MIN(a, b) ((a) > (b) ? (b) : (a))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define DEFAULT_ALLOC_SIZE 4096

/* curl debug function from http://curl.haxx.se/libcurl/c/debug.html */

static
void dump(const char *text,
          FILE *stream, unsigned char *ptr, size_t size,
          char nohex)
{
  size_t i;
  size_t c;
 
  unsigned int width=0x10;
 
  if(nohex)
    /* without the hex output, we can fit more on screen */ 
    width = 0x40;
 
  fprintf(stream, "%s, %10.10ld bytes (0x%8.8lx)\n",
          text, (long)size, (long)size);
 
  for(i=0; i<size; i+= width) {
 
    fprintf(stream, "%4.4lx: ", (long)i);
 
    if(!nohex) {
      /* hex not disabled, show it */ 
      for(c = 0; c < width; c++)
        if(i+c < size)
          fprintf(stream, "%02x ", ptr[i+c]);
        else
          fputs("   ", stream);
    }
 
    for(c = 0; (c < width) && (i+c < size); c++) {
      /* check for 0D0A; if found, skip past and start a new line of output */ 
      if (nohex && (i+c+1 < size) && ptr[i+c]==0x0D && ptr[i+c+1]==0x0A) {
        i+=(c+2-width);
        break;
      }
      fprintf(stream, "%c",
              (ptr[i+c]>=0x20) && (ptr[i+c]<0x80)?ptr[i+c]:'.');
      /* check again for 0D0A, to avoid an extra \n if it's at width */ 
      if (nohex && (i+c+2 < size) && ptr[i+c+1]==0x0D && ptr[i+c+2]==0x0A) {
        i+=(c+3-width);
        break;
      }
    }
    fputc('\n', stream); /* newline */ 
  }
  fflush(stream);
}
 
static
int my_trace(CURL *handle, curl_infotype type,
             char *data, size_t size,
             void *userp)
{
  HttpClient *client = (HttpClient *)userp;
  const char *text;
  (void)handle; /* prevent compiler warning */ 
 
  switch (type) {
  case CURLINFO_TEXT:
    fprintf(stderr, "== Info: %s", data);
  default: /* in case a new one is introduced to shock us */ 
    return 0;
 
  case CURLINFO_HEADER_OUT:
    text = "=> Send header";
    break;
  case CURLINFO_DATA_OUT:
    text = "=> Send data";
    break;
  case CURLINFO_SSL_DATA_OUT:
    text = "=> Send SSL data";
    break;
  case CURLINFO_HEADER_IN:
    text = "<= Recv header";
    break;
  case CURLINFO_DATA_IN:
    text = "<= Recv data";
    break;
  case CURLINFO_SSL_DATA_IN:
    text = "<= Recv SSL data";
    break;
  }
 
  dump(text, stderr, (unsigned char *)data, size, client->trace_ascii);
  return 0;
}

/* 新建buffer */
HttpBuffer* HttpBuffer_New() {
//{{{
    HttpBuffer* buffer = calloc(sizeof(HttpBuffer), 1);
    return buffer;
}
//}}}

/* 向buffer追加字符串 */
void HttpBuffer_Append(HttpBuffer* buffer, const char *input, size_t size) {
//{{{
    if (!size) return;
    while (buffer->next != NULL && buffer->used !=0) buffer = buffer->next;

    if (buffer->content == NULL) {
        int buffer_size = MAX(size, DEFAULT_ALLOC_SIZE);
        buffer->content = malloc(sizeof(char) * buffer_size);
        buffer->size    = buffer_size;
    }

    int left_buffer_size = buffer->size - buffer->used;
    int left_input_size = size;
    if (left_buffer_size) {
        int write_size = MIN(size, left_buffer_size);
        memcpy(buffer->content + buffer->used, input, sizeof(char) * write_size);
        input += write_size;
        left_input_size -= write_size;
        buffer->used += write_size;
    } 

    if (left_input_size) {
        HttpBuffer *newBuffer;
        if (buffer->next) {
            newBuffer = buffer->next;
        } else {
            newBuffer = HttpBuffer_New();
            buffer->next = newBuffer;
        }
        int buffer_size = MAX(left_input_size, DEFAULT_ALLOC_SIZE);
        if (newBuffer->content == NULL) {
            newBuffer->content = malloc(sizeof(char) * buffer_size);
            newBuffer->size    = buffer_size;
        }
        memcpy(newBuffer->content, input, sizeof(char) * left_input_size);
        newBuffer->used = left_input_size;
    }
}
//}}}

/* 获取buffer长度 */
size_t HttpBuffer_Length(HttpBuffer* buffer) {
//{{{
    size_t total = 0;
    while (buffer != NULL && buffer->used != 0) {
        total += buffer->used;
        buffer = buffer->next;
    }
    return total;
}
//}}}

/* 转换为char字符串 */
size_t HttpBuffer_ToChar(HttpBuffer* buffer, char *content) {
//{{{
    size_t total = 0;
    while (buffer != NULL && buffer->used != 0) {
        memcpy(content, buffer->content, sizeof(char) * buffer->used);
        content += buffer->used;
        buffer = buffer->next;
    }
    return total;
}
//}}}

/* 打印buffer */
void HttpBuffer_Dump(HttpBuffer* buffer) {
//{{{
    int id = 0;
    while (buffer != NULL) {
        printf("chuck %d\n", id);
        printf("size %d\n", (int)buffer->size);
        printf("used %d\n", (int)buffer->used);
        printf("content %.*s\n", (int)buffer->used, buffer->content);
        printf("\n");
        buffer = buffer->next;
        id ++;
    }
}
//}}}

/* 清空buffer,但是不释放已申请的内存 */
void HttpBuffer_Empty(HttpBuffer* buffer) {
//{{{
    while (buffer != NULL) {
        buffer->used = 0;
        buffer = buffer->next;
    }
}
//}}}

/* 释放buffer */
void HttpBuffer_Free(HttpBuffer* buffer) {
//{{{
    while (buffer != NULL) {
        if (buffer->content != NULL) {
            free(buffer->content);
            buffer->content = NULL;
        }
        HttpBuffer *old = buffer;
        buffer = buffer->next;
        free(old);
    }
}
//}}}


//*********************HttpClient***************************

/* CURLOPT_WRITEFUNCTION 回调 */
static size_t _HttpClient_WriteData(void *ptr, size_t size, size_t nmemb, void *data) {
//{{{
    HttpClient *client = (HttpClient *)data;
    HttpBuffer_Append(client->responseText, ptr, size * nmemb);
    return size * nmemb;
}
//}}}

/* CURLOPT_HEADERFUNCTION 回调 */
static size_t _HttpClient_WriteHeader(void *ptr, size_t size, size_t nmemb, void *data) {
//{{{
    HttpClient *client = (HttpClient *)data;
    HttpBuffer_Append(client->responseHeader, ptr, size * nmemb);
    return size * nmemb;
}
//}}}

/* 新建HttpClient */
HttpClient* HttpClient_New() {
//{{{
    HttpClient* client = calloc(sizeof(HttpClient), 1);
    return client;
}
//}}}


/* 释放HttpClient */
void HttpClient_Free(HttpClient* client) {
//{{{
    HttpBuffer_Free(client->responseText);
    HttpBuffer_Free(client->responseHeader);
    curl_easy_cleanup(client->curl);
    if (client->c_responseText != NULL) free(client->c_responseText);
    if (client->c_responseHeader != NULL) free(client->c_responseHeader);
    free(client);
}
//}}}


/* 初始化HttpClient,使用前务必调用 */
void HttpClient_Init(HttpClient *client) {
//{{{
    
    curl_version_info_data *version;

    if (client->curl == NULL) {
        client->curl = curl_easy_init();
        client->responseText = HttpBuffer_New();
        client->responseHeader = HttpBuffer_New();
    } else {
        curl_easy_reset(client->curl);
    }
    
    /* 重置失败次数 */ 
    client->fail_times        = 0;
    client->retry_times       = 5;
    client->retry_interval    = 1;

    /* 重置失败回调 */
    client->fail_reset_callback = NULL;
    client->fail_reset_context  = NULL;

    /* 清空body cstr */
    if (client->c_responseText != NULL) {
        free(client->c_responseText);
        client->c_responseText = NULL;
    }

    /* 清空header cstr */
    if (client->c_responseHeader != NULL) {
        free(client->c_responseText);
        client->c_responseText = NULL;
    }

    /* curl 初始化 */
    curl_easy_setopt(client->curl, CURLOPT_WRITEFUNCTION, _HttpClient_WriteData);
    curl_easy_setopt(client->curl, CURLOPT_WRITEDATA, client);

    curl_easy_setopt(client->curl, CURLOPT_SSL_VERIFYPEER, 0);
    curl_easy_setopt(client->curl, CURLOPT_SSL_VERIFYHOST, 0);
    /* 选个快点的加密算法 */
    version = curl_version_info(CURLVERSION_NOW);
    if (strstr(version->ssl_version, "OpenSSL") != NULL) {
        curl_easy_setopt(client->curl, CURLOPT_SSL_CIPHER_LIST, "RC4-SHA");
    } else if (strstr(version->ssl_version, "NSS") != NULL) {
        curl_easy_setopt(client->curl, CURLOPT_SSL_CIPHER_LIST, "rc4,rsa_rc4_128_sha");
    }

    /* 设置默认超时时间 */
    curl_easy_setopt(client->curl, CURLOPT_TIMEOUT, 20);
    curl_easy_setopt(client->curl, CURLOPT_CONNECTTIMEOUT, 5);
}
//}}}

/* 清空buffer 并调用curl_easy_perform */
static CURLcode _HttpClient_ResetAndPerform(HttpClient *client) {
//{{{
    HttpBuffer_Empty(client->responseText);
    HttpBuffer_Empty(client->responseHeader);
    return  curl_easy_perform(client->curl);
}
//}}}

/* 错误重试逻辑 */
static CURLcode _HttpClient_Perform(HttpClient *client) {
//{{{
    void (*reset_func) (void *context);
    CURLcode ret;
    while ((ret = _HttpClient_ResetAndPerform(client)) != CURLE_OK) {

        /* 调用失败重置函数 */
        reset_func = client->fail_reset_callback;
        if (reset_func != NULL) {
            reset_func(client->fail_reset_context);
        }

        client->fail_times ++;
        if (client->fail_times >= client->retry_times) {
            break;
        }

#ifdef DEBUG
        fprintf(stderr, "curl error %s\n", curl_easy_strerror(ret));
        fprintf(stderr, "HttpClient retry after %d seconds\n", client->retry_interval);
#endif

        sleep(client->retry_interval);
    }
    return ret;
}
//}}}

/* 设置失败重试 */
void HttpClient_SetFailRetry(HttpClient *client, int retry_times, int retry_interval) {
//{{{
    client->retry_times        = retry_times;
    client->retry_interval     = retry_interval;
}
//}}}

/* 设置请求失败后的回调函数 */
void HttpClient_SetFailRetryCallback(HttpClient *client, void *callback, void *context) {
//{{{
    client->fail_reset_callback = callback;
    client->fail_reset_context  = context;
}
//}}}

/* 获取响应头 */
void HttpClient_SetWillGetHeader(HttpClient *client) {
//{{{
    curl_easy_setopt(client->curl, CURLOPT_HEADERFUNCTION, _HttpClient_WriteHeader);
    curl_easy_setopt(client->curl, CURLOPT_WRITEHEADER, client);
}
//}}}
/* 打印调试信息 */
void  HttpClient_SetDebug(HttpClient *client, int trace_ascii) {
    client->trace_ascii = trace_ascii;
    curl_easy_setopt(client->curl, CURLOPT_DEBUGFUNCTION, my_trace);
    curl_easy_setopt(client->curl, CURLOPT_DEBUGDATA, client);
    curl_easy_setopt(client->curl, CURLOPT_VERBOSE, 1L);
}

/* 发送Get请求 */
CURLcode HttpClient_Get(HttpClient *client, const char *url) {
//{{{
    curl_easy_setopt(client->curl, CURLOPT_URL, url);
    curl_easy_setopt(client->curl, CURLOPT_POST, 0);
    client->resultCode = _HttpClient_Perform(client);
    return client->resultCode;
}
//}}}

/* 发送POST请求 */
CURLcode HttpClient_PostHttpData(HttpClient *client, const char *url, struct curl_httppost *data) {
//{{{
    curl_easy_setopt(client->curl, CURLOPT_URL, url);
    curl_easy_setopt(client->curl, CURLOPT_HTTPPOST, data);
    client->resultCode = _HttpClient_Perform(client);
    return client->resultCode;
}
//}}}

/* 获取错误信息 */
const char *HttpClient_GetError(HttpClient *client) {
//{{{
   if (client->resultCode == CURLE_OK) return NULL;
   return curl_easy_strerror(client->resultCode);
}
//}}}

/* 获取相应body */
const char *HttpClient_ResponseText(HttpClient *client) {
//{{{
    size_t s;
    char *str;
    if (client->c_responseText == NULL) {
        s = HttpBuffer_Length(client->responseText);
        str = malloc(sizeof(char) * (s + 1));
        str[s] = '\0';
        HttpBuffer_ToChar(client->responseText, str);
        client->c_responseText = str;
    }
    return client->c_responseText;
}
//}}}

/* 获取相应header */
const char *HttpClient_ResponseHeader(HttpClient *client) {
//{{{
    size_t s;
    char *str;
    if (client->c_responseHeader == NULL) {
        s = HttpBuffer_Length(client->responseHeader);
        str = malloc(sizeof(char) * (s + 1));
        str[s] = '\0';
        HttpBuffer_ToChar(client->responseHeader, str);
        client->c_responseHeader = str;
    }
    return client->c_responseHeader;
}
//}}}

/* Http响应码 */
long HttpClient_ResponseCode(HttpClient *client) {
//{{{
    long httpCode;
    curl_easy_getinfo(client->curl, CURLINFO_RESPONSE_CODE, &httpCode);
    return httpCode;
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
