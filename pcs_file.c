/*
 * Copyright (c) 2013 emptyhua@gmail.com
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include "pcs.h"

PCSFileBlock *PCSFileBlock_New() {
//{{{
    PCSFileBlock *block = calloc(sizeof(PCSFileBlock), 1);
    return block;
}
//}}}

void PCSFileBlock_Free(PCSFileBlock *block) {
//{{{
    free(block);
}
//}}}

PCSFile *PCSFile_New() {
//{{{
    PCSFile *file = calloc(sizeof(PCSFile), 1);
    return file;
}
//}}}

void PCSFile_Free(PCSFile *file) {
//{{{
    PCSFileBlock  *old  = NULL;
    PCSFileBlock *block = NULL;
    if (file == NULL) return;
    if (file->path != NULL) {
        free(file->path);
    }

    if (file->block != NULL) {
        block = file->block;
        while (block) {
            old = block;
            block = block->next;
            PCSFileBlock_Free(old);
        }
    }

    free(file);
}
//}}}

void PCSFile_SetPath(PCSFile *file, const char *path) {
//{{{
    char *new_path = strdup(path);
    if (file->path) {
        free(file->path);
    }
    file->path = new_path;
}
//}}}

PCSFileList *PCSFileList_New() {
//{{{
    PCSFileList *list = calloc(sizeof(PCSFileList), 1);
    return list;
}
//}}}

void PCSFileList_Free(PCSFileList *list) {
//{{{
    if (list == NULL) return;
    PCSFile *old;
    PCSFile *file = list->first;
    while (file) {
        old = file;
        file = file->next;
        PCSFile_Free(old);
    }
    free(list);
}
//}}}

static PCSFileList *PCSFileList_Reverse(PCSFileList *list) {
//{{{
    PCSFile *head = list->first;
    if (head == NULL || head->next == NULL) return list;
    PCSFile *p;  
    PCSFile *q;  
    PCSFile *r;  
    p = head;    
    q = head->next;  
    head->next = NULL;  
    while(q){  
        r = q->next;
        q->next = p;      
        p = q;
        q = r;
    } 
    list->first= p;
    return list;
}
//}}}

void PCSFileList_Prepend(PCSFileList *list, PCSFile *file) {
//{{{
    file->next = list->first;
    list->first = file;
}
//}}}

PCSFile *PCSFileList_Find(PCSFileList *list, const char *path) {
//{{{
    PCSFile *file = list->first; 
    while (file != NULL) {
        if (strcmp(file->path, path) == 0) {
            return file;
        }
        file = file->next;
    }
    return NULL;
}
//}}}

PCSFile *PCSFileList_Shift(PCSFileList *list) {
//{{{
    PCSFile *file = list->first; 
    if (file != NULL) {
        list->first = file->next;
    } else {
        list->first = NULL;
    }
    return file;
}
//}}}

/* 构建本地文件对象 */
PCSFile *BaiduPCS_NewLocalFile(BaiduPCS *api, const char *path) {
//{{{
    PCSFile *file   = NULL;
    FILE *fp        = NULL;
   
    api->error[0] = '\0'; 
    
    
    //文件不存在
    if (lstat(path, &(api->file_st)) == -1) {
        sprintf(api->error, "%s dons't exsit", path);
        goto free;
    }
    
   
    //目录
    if (S_ISDIR(api->file_st.st_mode)) {
        file            = PCSFile_New();
        file->path      = strdup(path);
        file->is_dir    = 1;
    //链接
    } else if (S_ISLNK(api->file_st.st_mode)) {
        file            = PCSFile_New();
        file->path      = strdup(path);
        file->is_link   = 1;
    //普通文件
    } else if (S_ISREG(api->file_st.st_mode)) {
        file            = PCSFile_New();
        file->path      = strdup(path);
        file->size      = api->file_st.st_size;

        fp = fopen(path, "rb");
        if (fp == NULL) {
            sprintf(api->error, "%s is not readable", path);
            PCSFile_Free(file);
            file = NULL;
            goto free;
        } 
        fclose(fp);

    } else {
        sprintf(api->error, "%s unspported file type", path);
    }

free:
    return file;
}
//}}}

/* 构建远程目录的文件列表 */
PCSFileList *BaiduPCS_NewRemoteFileList(BaiduPCS *api, const char *path) {
//{{{
    PCSFileList *result     = NULL;
    PCSFileList *stack      = NULL;
    PCSFileList *list       = NULL;
    PCSFile *c_file         = NULL;
    PCSFile *t_file         = NULL;
    
    //结果列表
    result          = PCSFileList_New();
    //遍历栈
    stack           = PCSFileList_New();

    c_file = BaiduPCS_NewRemoteFile(api, path);

    while(c_file != NULL) {
        //如果是普通文件
        if (!c_file->is_dir) {
            PCSFileList_Prepend(result, c_file);
            c_file = PCSFileList_Shift(stack);
        //如果是目录
        } else {
            PCSFileList_Prepend(result, c_file);
            list = BaiduPCS_ListRemoteDir(api, c_file->path);
            //列出目录
            if (list != NULL) {
                t_file  = PCSFileList_Shift(list);
                while(t_file != NULL) {
                    if (t_file->is_dir) {
                        PCSFileList_Prepend(stack, t_file);
                    } else {
                        PCSFileList_Prepend(result, t_file);
                    }
                    t_file = PCSFileList_Shift(list);
                }
                PCSFileList_Free(list);
            }
            c_file = PCSFileList_Shift(stack);
        }
    }
    PCSFileList_Free(stack);
    return PCSFileList_Reverse(result);
}
//}}}

/* 构建本地目录的文件列表 */
PCSFileList *BaiduPCS_NewLocalFileList(BaiduPCS *api, const char *path) {
//{{{
    PCSFileList *result     = NULL;
    PCSFileList *stack      = NULL;
    PCSFile *c_file         = NULL;
    PCSFile *new_file       = NULL;
    DIR *dir                = NULL;
    struct dirent *item     = NULL;
    char *file_path         = NULL;
    
    //结果列表
    result          = PCSFileList_New();
    //遍历栈
    stack           = PCSFileList_New();
    c_file          = BaiduPCS_NewLocalFile(api, path);

    while(c_file != NULL) {
        //普通文件
        if (!c_file->is_dir && !c_file->is_link) {
            PCSFileList_Prepend(result, c_file);
            c_file = PCSFileList_Shift(stack);
        //如果是目录
        } else if (c_file->is_dir) {
            //遍历目录
            dir = opendir(c_file->path);  
            //读取目录失败
            if (dir == NULL) {
                PCSFile_Free(c_file);
                c_file = PCSFileList_Shift(stack);
                continue;
            }

            while ((item = readdir(dir)) != NULL) {
                if (strcmp(item->d_name, ".") == 0
                    || strcmp(item->d_name, "..") == 0) {
                    continue;
                }
                //构建文件完整路径
                file_path = malloc(sizeof(char) * (strlen(c_file->path) + strlen(item->d_name) + 2));
                file_path[0] = '\0';
                strcat(file_path, c_file->path);
                strcat(file_path, "/");
                strcat(file_path, item->d_name);

                new_file = BaiduPCS_NewLocalFile(api, file_path);
                if (new_file != NULL) { 
                    //放入栈中
                    PCSFileList_Prepend(stack, new_file);
                }

                free(file_path);
                file_path = NULL;
            }
            closedir(dir);

            PCSFileList_Prepend(result, c_file);
            c_file = PCSFileList_Shift(stack);
        //忽略其他类型
        } else {
            PCSFile_Free(c_file);
            c_file = PCSFileList_Shift(stack);
        }
    }

    PCSFileList_Free(stack);

    return PCSFileList_Reverse(result);
}
//}}}

void PCSFile_Dump(PCSFile *file) {
//{{{
    if (file == NULL) return;
    PCSFileBlock *block = NULL;
    int i               = 0;

    printf("路径:%s\n", file->path);
    printf("MD5:%s\n", file->md5);
    printf("大小:%lld\n", (unsigned long long)file->size);
    printf("创建时间:%u\n", file->ctime);
    printf("创建时间:%u\n", file->mtime);
    printf("是否目录:%d\n", (int)file->is_dir);
    printf("是否链接:%d\n", (int)file->is_link);
    printf("切片大小:%lld\n", (unsigned long long)file->block_size);

    if (file->block != NULL) {
        block = file->block;
        for (; block != NULL; i ++) {
            printf("-块:%d\n", i); 
            printf("-偏移:%lld\n", (unsigned long long)block->offset); 
            printf("-尺寸:%lld\n", (unsigned long long)block->size);
            block = block->next;
        }
    }
}
//}}}

void PCSFileList_Dump(PCSFileList *list) {
//{{{
    PCSFile *file = list->first;
    while (file != NULL) {
        printf("------------------------------\n");
        PCSFile_Dump(file);
        file = file->next;
    }
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
