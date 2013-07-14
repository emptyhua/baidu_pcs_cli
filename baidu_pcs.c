/*
 * Copyright (c) 2013 emptyhua@gmail.com
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <time.h>
#include <dirent.h>
#include <libgen.h>

#include "pcs.h"

/* pcs api key */
static const char *option_api_key     = "EeONqeIfLl7k9aSVAdGEXald";
/* pcs api secret */
static const char *option_api_secret  = "wtVBuItWtIgt0gs62DKiRx710eSvcVPF";

static BaiduPCS *api = NULL;

void readable_timestamp(time_t t, char *buf) {
//{{{
    struct tm * timeinfo;
    timeinfo = localtime(&t);
    strftime(buf, 80, "%H:%M/%Y-%m-%d", timeinfo);
}
//}}}

void readable_size(double size, char *buf) {
//{{{
    int i = 0;
    static const char* units[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"};
    while (size > 1024) {
        size /= 1024;
        i++;
    }
    sprintf(buf, "%.*f %s", i, size, units[i]);
}
//}}}

void print_file(PCSFile *file, int show_detail) {
//{{{
    char buf2[100]; 
    char buf3[100]; 
    if (show_detail) {
        readable_timestamp(file->mtime, buf2);
        if (file->is_dir) {
            printf("目录      -      %s %s\n", buf2, file->path);
        } else {
            readable_size(file->size, buf3);
            printf("文件 %11s %s %s\n", buf3, buf2, file->path);
        } 
    } else {
        printf("%s\n", file->path);
    }
}
//}}}

/* 打印帮助 */
void usage() {
//{{{
    fprintf(stderr,
"使用方法: baidu_pcs 命令 [选项]\n"
"\n"
"命令列表:\n"
"\n"
"info      查看云盘信息\n"
"\n"
"ls        列出远程文件或目录\n"
"          选项:\n"
"            -l 显示详细信息\n"
"            -r 递归子目录\n"
"\n"
"upload   [选项] [本地路径] [远程路径] 上传文件或目录\n"
"          选项:\n"
"            覆盖策略\n"
"            默认:略过已存在同名远程文件\n"
"            -o 覆盖远程同名文件\n"
"            -n 如果存在同名文件，创建以日期结尾的新文件\n"
"\n"
"            -l 跟随软链\n"
"\n"
"download [选项] [远程路径] [本地路径] 下载文件或目录\n"
"          选项:\n"
"            -o 覆盖本地同名文件\n"
"            -n 如果存在同名文件，创建以日期结尾的新文件\n"
"cp       [选项] [远程路径] [目的远程路径] 复制远程文件或目录\n"
"mv       [选项] [远程路径] [目的远程路径] 移动远程文件或目录\n"
"rm       [选项] [远程路径] 删除远程文件或目录\n"
    );
}
//}}}

/* 初始化API */
int init_api() {
//{{{
    if (api != NULL) return 1;

    int ret = 1;
    struct passwd *pw;
    const char *homedir;
    const char *token;
    const char *error;
    char config[1024];
    char config_mode[] = "0600";
    char buf[1024];
    int i = 0;

    /* 需要释放 */
    FILE *fp;

    api = BaiduPCS_New();    
    sprintf(api->key, "%s", option_api_key);
    sprintf(api->secret, "%s", option_api_secret);

    /* 读取已存的token */
    pw = getpwuid(getuid());
    homedir = pw->pw_dir;
    snprintf(config, 1023, "%s/.baidu_pcs_token",  homedir);

    fp = fopen(config, "rb");
    if (fp != NULL) {
        i = fread(buf, 1, 1024, fp);
        snprintf(api->token, i + 1, "%s", buf);
#ifdef DEBUG
        fprintf(stderr, "已存token: %s\n", api->token);
#endif
        fclose(fp);
        fp = NULL;
        /* 确保只有自己读写 */
        i = strtol(config_mode, 0, 8);
        chmod(config, i);
    } else {
#ifdef DEBUG
        fprintf(stderr, "没有已存token %s\n", config);
#endif
        token = BaiduPCS_Auth(api);    
        error = BaiduPCS_GetError(api);
        if (error != NULL) {
            fprintf(stderr, "认证失败 %s\n", error);        
            ret = 0;
        } else {
            fp = fopen(config, "wb");
            if (fp != NULL) {
                fwrite(token, 1, strlen(token), fp);
                fclose(fp);
                fp = NULL;
                /* 确保只有自己读写 */
                i = strtol(config_mode, 0, 8);
                chmod(config, i);
            } else {
                fprintf(stderr, "%s 无法写入\n", config);        
                ret = 0;
            }
        }
    }

    return ret;
}
//}}}

/* 显示云盘信息 */
int command_info(int argc, char **argv) {
//{{{
    if (!init_api()) return 1;
    int ret = 0;
    const char *error   = NULL;
    char buf[1024];
    BaiduPCSInfo info;

    BaiduPCS_Info(api, &info);
    error = BaiduPCS_GetError(api);
    if (error != NULL) {
        fprintf(stderr, "获取信息失败%s\n", error);
        ret = 1;
        goto free;
    }

    readable_size(info.quota, buf);
    printf("总容量:%s\n", buf);
    readable_size(info.used, buf);
    printf("已使用:%s\n", buf);
free:
    return ret;
}
//}}}

/* 列出远程文件 */
int command_ls(int argc, char **argv) {
//{{{
    int option_show_detail = 0; //是否显示详情
    int option_recursion   = 0; //是否递归
    int ret = 0;
    char c;
    char *remote_path;
    const char *error;
    PCSFile *file;
    PCSFileList *list;
    PCSFile *tmp;


    opterr = 0;
    while ((c = getopt (argc, argv, "lr")) != -1) {
        switch (c) {
            case 'l':
                option_show_detail = 1;
                break;
            case 'r':
                option_recursion = 1;
                break;
        }
    }
 
    if (optind < argc - 1) {
        fprintf(stderr, "请指定路径\n");
        usage();
        ret = 1;
        goto free;
    }
    
    remote_path = argv[argc - 1];
#ifdef DEBUG
    fprintf(stderr, "ls %s\n", remote_path);
#endif
    
    if (!init_api()) {
        ret = 1;
        goto free;
    }
    
    file = BaiduPCS_NewRemoteFile(api, remote_path);
    error = BaiduPCS_GetError(api);
    if (error != NULL || file == NULL) {
        fprintf(stderr, "获取文件信息失败 %s\n", error);
        ret = 1;
        goto free;
    }

    if (!file->is_dir) {
        print_file(file, option_show_detail);
    } else {
        if (option_recursion) {
            list = BaiduPCS_NewRemoteFileList(api, file->path);
        } else {
            list = BaiduPCS_ListRemoteDir(api, file->path);
        }
        error = BaiduPCS_GetError(api);
        if (error != NULL || list == NULL) {
            fprintf(stderr, "获取文件信息失败 %s\n", error);
            ret = 1;
            goto free;
        }
        tmp = list->first;
        while (tmp) {
            print_file(tmp, option_show_detail);
            tmp = tmp->next;
        }
    }
free:

    if (file != NULL) {
        PCSFile_Free(file); 
    }

    if (list != NULL) {
        PCSFileList_Free(list);
    }
    return ret;
}
//}}}

int do_upload(const char *local_path,
const char *remote_path,  
int overwrite,              /* 是否覆盖 */ 
int create_new,             /* 是否新建 */ 
int follow_link,            /* 跟随软链 */
int split_size              /* 分片大小 */
) { 
//{{{
    int ret                 = 0;
    int is_first            = 1;
    PCSFile *new_file       = NULL;
    PCSFile *t_file         = NULL;
    struct dirent *item     = NULL;
    const char *error       = NULL;
    const char *ondup       = NULL;

    char t_path[PATH_MAX + 1];         /* 临时路径       */
    char t_remote_path[PATH_MAX + 1];  /* 临时路径       */

    /* 需要释放 */
    PCSFile *c_file         = NULL;
    PCSFileList *stack      = NULL;
    PCSFile *remote_file    = NULL;
    PCSFileList *r_list     = NULL;
    DIR *dir                = NULL;
    
    /* 遍历栈 */
    stack           = PCSFileList_New();
    c_file          = BaiduPCS_NewLocalFile(api, local_path);

    error = BaiduPCS_GetError(api);
    if (error != NULL || c_file == NULL) {
        fprintf(stderr, "错误:创建文件对象失败 %s\n", error);
        ret = 1;
        goto free;
    }

    /* 当前要上传的远程路径 */
    c_file->userdata = strdup(remote_path);
    
    if (create_new) {
        ondup = "newcopy";
    } else {
        ondup = "overwrite";
    }

    while(c_file != NULL) {
        remote_path = (char *)c_file->userdata;
        /* 普通文件 */
        if (!c_file->is_dir && !c_file->is_link) {

            if (is_first) {
                remote_file = BaiduPCS_NewRemoteFile(api, remote_path); 
                if (remote_file != NULL) {
                    /*  upload xx.txt /apps/xx/ 
                     *  对于这种情况,远端路径修正为/apps/xx/xx.txt
                     */
                    if (remote_file->is_dir) {
                        t_remote_path[0] = '\0';
                        strcat(t_remote_path, remote_path);
                        strcat(t_remote_path, "/");
                        strcat(t_remote_path, basename(c_file->path));
#ifdef DEBUG
                        fprintf(stderr, "上传路径 %s 修正为 %s\n", remote_path, t_remote_path);
#endif

                        free(c_file->userdata);
                        c_file->userdata = strdup(t_remote_path);
                        remote_path = c_file->userdata;
                        PCSFile_Free(remote_file);
                        remote_file = NULL;
                        continue;
                    /* upload xx.txt /apps/xx/xx.txt */
                    } else if (!overwrite && !create_new) {
                        fprintf(stderr, "IGNORE: %s -> %s 远端已存在\n", c_file->path, remote_file->path);
                        goto free; 
                    }
                    PCSFile_Free(remote_file);
                    remote_file = NULL;
                }
            }

            remote_file = BaiduPCS_Upload(api, c_file, remote_path, 0, ondup);
            error = BaiduPCS_GetError(api);
            /* 上传失败 */
            if (error != NULL || remote_file == NULL) {
                fprintf(stderr, "ERROR: %s -> %s %s\n", c_file->path, remote_path, error);
                /* 如果上传单个文件，失败后会返回error code */
                if (is_first) {
                    ret = 1;
                    goto free;
                }
            /* 上传成功 */
            } else {
                printf("OK: %s -> %s\n", c_file->path, remote_file->path);
                PCSFile_Free(remote_file);
                remote_file = NULL;

            }

            free(c_file->userdata);
            PCSFile_Free(c_file);

            c_file = PCSFileList_Shift(stack);
            is_first = 0;
        //如果是目录
        } else if (c_file->is_dir) {
            /* 遍历目录 */
            dir = opendir(c_file->path);  
            /* 读取目录失败 */
            if (dir == NULL) {
                fprintf(stderr, "ERROR: %s -> %s 目录读取失败\n", c_file->path, remote_path);
                /* 如果当前为用户指定的根目录，直接返回错误 */
                if (is_first) {
                    ret = 1;
                    goto free;
                }
            } else {
                /* 进行目录修正 */
                if (is_first) {
                    remote_file = BaiduPCS_NewRemoteFile(api, remote_path); 
                    if (remote_file != NULL) {
                        /*  upload ./dir/ /apps/xx/ 
                         *  对于这种情况,远端路径修正为/apps/xx/dir
                         */
                        if (remote_file->is_dir) {
                            t_remote_path[0] = '\0';
                            strcat(t_remote_path, remote_file->path);
                            strcat(t_remote_path, "/");
                            strcat(t_remote_path, basename(c_file->path));
#ifdef DEBUG
                            fprintf(stderr, "上传目录 %s 修正为 %s\n", remote_path, t_remote_path);
#endif
                            free(c_file->userdata);
                            c_file->userdata = strdup(t_remote_path);
                            remote_path = c_file->userdata;

                        } else if (!remote_file->is_dir) {
                            fprintf(stderr, "ERROR: %s -> %s 远端路径不是目录\n", c_file->path, remote_file->path);
                            ret = 1;
                            goto free;
                        }

                        PCSFile_Free(remote_file);
                        remote_file = NULL;
                    }
                }

                /* 获取远程目录列表,用于排除远端已存在的文件 */
                remote_file = BaiduPCS_NewRemoteFile(api, remote_path);
                if (remote_file != NULL) {
                    /* 远端已存在同名文件 */
                    if (!remote_file->is_dir) {
                        fprintf(stderr, "ERROR: %s -> %s 远端已存与目录同名的文件\n", c_file->path, remote_file->path);
                        if (is_first) {
                            ret = 1;
                            goto free;
                        }
                    } else {
                        /* 获取远端目录文件列表,用于对比本地目录列表 */
                        r_list = BaiduPCS_ListRemoteDir(api, remote_path);
                    }
                    PCSFile_Free(remote_file);
                    remote_file = NULL;
                }
                
                while ((item = readdir(dir)) != NULL) {
                    if (strcmp(item->d_name, ".") == 0
                        || strcmp(item->d_name, "..") == 0) {
                        continue;
                    }

                    /* 构建本地文件完整路径 */
                    t_path[0] = '\0';
                    strcat(t_path, c_file->path);
                    strcat(t_path, "/");
                    strcat(t_path, item->d_name);

                    /* 构建上传路径 */
                    t_remote_path[0] = '\0';
                    strcat(t_remote_path, remote_path);
                    strcat(t_remote_path, "/");
                    strcat(t_remote_path, item->d_name);

                    new_file = BaiduPCS_NewLocalFile(api, t_path);
                    error = BaiduPCS_GetError(api);
                    if (error != NULL || new_file == NULL) {
                        fprintf(stderr, "ERROR: %s -> %s %s\n", t_path, t_remote_path, error);
                    } else {
                        if (r_list != NULL) {
                            t_file = PCSFileList_Find(r_list, t_remote_path);
                            /* 远程已存在同名文件 */
                            if (t_file != NULL) {
                                if (new_file->is_dir && !t_file->is_dir) {
                                    fprintf(stderr, "ERROR: %s -> %s 远程已存在与目录同名的文件\n", t_path, t_remote_path);
                                    PCSFile_Free(new_file);
                                    new_file = NULL;
                                    continue;
                                } else if (!new_file->is_dir && t_file->is_dir) {
                                    fprintf(stderr, "ERROR: %s -> %s 远程已存在与文件同名的目录\n", t_path, t_remote_path);
                                    PCSFile_Free(new_file);
                                    new_file = NULL;
                                    continue;
                                } else if (!new_file->is_dir && !new_file->is_link && !overwrite && !create_new) {
                                    fprintf(stderr, "IGNORE: %s -> %s 远程已存在同名文件\n", t_path, t_remote_path);
                                    PCSFile_Free(new_file);
                                    new_file = NULL;
                                    continue;
                                }
                            }
                        }
                       
                        new_file->userdata = strdup(t_remote_path);
                        //放入栈中
                        PCSFileList_Prepend(stack, new_file);
                        new_file = NULL;
                    }
                }
                if (r_list != NULL) {
                    PCSFileList_Free(r_list);
                    r_list = NULL;
                }

                closedir(dir);
                dir = NULL;
            }

            free(c_file->userdata);
            PCSFile_Free(c_file);
            c_file = PCSFileList_Shift(stack);
            is_first = 0;
        /* 软链接 */
        } else if (c_file->is_link) { 
            if (follow_link) {
                realpath(c_file->path, t_path);
#ifdef DEBUG
                fprintf(stderr, "跟随软链 %s -> %s\n", c_file->path, t_path);
#endif
                new_file = BaiduPCS_NewLocalFile(api, t_path);
                new_file->userdata = c_file->userdata;
                PCSFile_Free(c_file);
                c_file = new_file;
            } else {
                fprintf(stderr, "IGNORE: 忽略软链接 %s\n", c_file->path);
                free(c_file->userdata);
                PCSFile_Free(c_file);
                c_file = PCSFileList_Shift(stack);
                is_first = 0;
            }
        /* 忽略其他类型 */
        } else {
            fprintf(stderr, "ERROR: 不支持的文件 %s\n", c_file->path);
            free(c_file->userdata);
            PCSFile_Free(c_file);
            c_file = PCSFileList_Shift(stack);
            is_first = 0;
        }
    }

free:
    PCSFileList_Free(stack);

    if (dir != NULL) {
        closedir(dir);
    }

    if (c_file != NULL) {
        free(c_file->userdata);
        PCSFile_Free(c_file);
    }

    if (remote_file != NULL) {
        PCSFile_Free(remote_file);
    }

    if (r_list != NULL) {
        PCSFileList_Free(r_list);
    }
    return ret;
}
//}}}

/* 上传文件或目录 */
int command_upload(int argc, char **argv) {
//{{{
    int option_overwrite    = 0; /* 覆盖同名文件        */
    int option_new          = 0; /* 创建新文件          */
    int option_follow_link  = 0; /* 复制链接源文件      */
    int option_split_size   = 0; /* 分片大小            */

    int ret = 0;
    char c;

    char *split_size;
    char *remote_path;
    char *local_path;
    

    opterr = 0;
    while ((c = getopt(argc, argv, "onlp:")) != -1) {
        switch (c) {
            case 'o':
                option_overwrite = 1;
                break;
            case 'n':
                option_new = 1;
                break;
            case 'l':
                option_follow_link = 1;
                break;
            case 'p':
                split_size = optarg;
                break;
            case '?':
                if (optopt == 'p') {
                    fprintf(stderr, "-p 请指定分片大小\n");
                    ret = 1;
                    goto free;
                }
                break;
        }
    }
 
    if (option_overwrite && option_new) {
        fprintf(stderr, "请不要同时指定-n -o\n");
        ret = 1;
        goto free;
    }

    if (optind < argc - 2) {
        fprintf(stderr, "请指定路径\n");
        usage();
        ret = 1;
        goto free;
    }

    local_path = argv[argc - 2];
    if (local_path[strlen(local_path) - 1] == '/') {
        local_path[strlen(local_path) - 1] = '\0';
    }

    remote_path = argv[argc - 1];
    if (remote_path[strlen(remote_path) - 1] == '/') {
        remote_path[strlen(remote_path) - 1] = '\0';
    }

#ifdef DEBUG
    fprintf(stderr, "Upload %s to %s\n", local_path, remote_path);
#endif

    if (!init_api()) {
        ret = 1;
        goto free;
    }
    
    if (stat(local_path, &(api->file_st)) == -1) {
        fprintf(stderr, "%s 不存在\n", local_path);
        ret = 1;
        goto free;
    }

    ret = do_upload(local_path,
                    remote_path,
                    option_overwrite, 
                    option_new, 
                    option_follow_link,
                    option_split_size);
free:

    return ret;
} 
//}}}

int do_download(const char *remote_path,
const char *local_path,  
int overwrite,             /* 是否覆盖 */ 
int create_new             /* 是否新建 */ 
) { 
//{{{
    int ret                 = 0;
    int single_download     = 0;
    int local_exist         = 0;
    int is_first            = 1;
    int remote_root_offset  = 0;
    const char *error       = NULL;
    const char *tmp         = NULL;
    PCSFileList *list       = NULL; /* 需要释放 */
    PCSFile *file           = NULL; 
    char *buf               = api->util_buffer0;
    FILE *fp                = NULL; /* 需要释放 */

    time_t rawtime;
    struct tm * timeinfo;

    char t_local_root[PATH_MAX + 1];
    char t_local_file[PATH_MAX + 1];


    list = BaiduPCS_NewRemoteFileList(api, remote_path);
    error = BaiduPCS_GetError(api);
    if (error != NULL || list == NULL) {
        fprintf(stderr, "ERROR: 获取文件列表失败 %s\n", error);
        goto free;
    }

#ifdef DEBUG
    PCSFileList_Dump(list);
#endif

    single_download = list->first->next == NULL && !list->first->is_dir;

    /* 输出到标准输出 */
    if (strcmp(local_path, "-") == 0) {
        file = list->first;
        while (file != NULL) {
            if (file->is_dir) {
                file = file->next;
                continue;
            }
            BaiduPCS_Download(api, file->path, stdout);
            error = BaiduPCS_GetError(api);
            if (error != NULL) {
                fprintf(stderr, "ERROR: %s %s\n", file->path, error);
                if (single_download) {
                    ret = 1;
                }
            }
            file = file->next;
        }
    /* 下载到文件系统 */
    } else {
        /* 下载目录修正
         * download /app/remote_dir/  ./local_dir
         * 如果本地已存在local_dir,则本地目录修正为 ./local_dir/remote_dir
         */

        file = list->first;

        /* 远端为目录 */
        if (file->is_dir) {
            sprintf(t_local_root, "%s", local_path);
            remote_root_offset = strlen(list->first->path);
            if (stat(local_path, &(api->file_st)) != -1) {
                if (S_ISDIR(api->file_st.st_mode)){
                    t_local_root[0] = '\0';
                    strcat(t_local_root, local_path);
                    strcat(t_local_root, "/");
                    strcat(t_local_root, basename(remote_path));
#ifdef DEBUG
                    fprintf(stderr, "本地根目录修正为:%s\n", t_local_root);
#endif
                } else {
                    fprintf(stderr, "ERROR: %s -> %s 本地已存与目录同名的文件\n", file->path, local_path);
                    ret = 1;
                    goto free;
                }
            }
        } else {
            sprintf(t_local_root, "%s", dirname(local_path));
            remote_root_offset = strlen(dirname(list->first->path));
            if (stat(local_path, &(api->file_st)) != -1) {
                if (S_ISDIR(api->file_st.st_mode)){
                    sprintf(t_local_root, "%s", local_path);
                }
            }
        }
    

        /* 遍历下载 */
        while (file != NULL) {
            /* 确定本地存储路径 */
            tmp = file->path + remote_root_offset;
            sprintf(t_local_file, "%s%s", t_local_root, tmp);
            /* 本地是否已存在 */
            local_exist = (stat(t_local_file, &(api->file_st)) != -1);
            /* 目录 */
            if (file->is_dir) {
                /* 本地已存在 */
                if (local_exist) {
                    if (!S_ISDIR(api->file_st.st_mode)){
                        fprintf(stderr, "ERROR: %s -> %s 本地已存与目录同名的文件\n", file->path, t_local_file);
                        ret = 1;
                        goto free;
                    }
                } else {
                    if (0 != mkdir(t_local_file, 0755)) {
                        fprintf(stderr, "ERROR: %s -> %s 本地目录创建失败\n", file->path, t_local_file);
                        ret = 1;
                        goto free;
                    }
                }
            /* 普通文件 */
            } else {
                if (local_exist) {
                    if (S_ISDIR(api->file_st.st_mode)) {
                        fprintf(stderr, "ERROR: %s -> %s 本地已存与文件同名的目录\n", file->path, t_local_file);
                        if (single_download) {
                            ret = 1;
                            goto free;
                        }
                    } else if (!overwrite && !create_new) {
                        fprintf(stderr, "IGNORE: %s -> %s 本地已存同名文件\n", file->path, t_local_file);
                    }
                }

                if (local_exist && create_new) {
                    time(&rawtime);
                    timeinfo = localtime(&rawtime);
                    strftime(buf, 80, ".%Y%m%d%H%M%S", timeinfo);
                    strcat(t_local_file, buf);
                }

                if (overwrite || create_new || !local_exist) {
                    fp = fopen(t_local_file, "wb");
                    if (fp == NULL) {
                        fprintf(stderr, "ERROR: %s -> %s 本地文件无法写入\n", file->path, t_local_file);
                        if (single_download) {
                            ret = 1;
                            goto free;
                        }
                    } else {
                        BaiduPCS_Download(api, file->path, fp);
                        error = BaiduPCS_GetError(api);
                        if (error != NULL) {
                            fprintf(stderr, "ERROR: %s -> %s %s\n", file->path, t_local_file, error);
                        } else {
                            printf("OK: %s -> %s\n", file->path, t_local_file);
                        }
                        fclose(fp);
                        fp = NULL;
                    }
                }
            }
            is_first = 0;
            file = file->next;
        }
    }

free:
    if (list != NULL) {
        PCSFileList_Free(list);
    }

    if (fp != NULL) {
        fclose(fp);
    }
    return ret;
}
//}}}


/* 下载文件或目录 */
int command_download(int argc, char **argv) {
//{{{
    int option_overwrite    = 0; /* 覆盖同名文件        */
    int option_new          = 0; /* 创建新文件          */

    int ret = 0;
    char c;

    char *remote_path;
    char *local_path;
    
    opterr = 0;
    while ((c = getopt(argc, argv, "on")) != -1) {
        switch (c) {
            case 'o':
                option_overwrite = 1;
                break;
            case 'n':
                option_new = 1;
                break;
        }
    }
 
    if (option_overwrite && option_new) {
        fprintf(stderr, "请不要同时指定-n -o\n");
        ret = 1;
        goto free;
    }

    if (optind < argc - 2) {
        fprintf(stderr, "请指定路径\n");
        usage();
        ret = 1;
        goto free;
    }

    local_path = argv[argc - 1];
    if (local_path[strlen(local_path) - 1] == '/') {
        local_path[strlen(local_path) - 1] = '\0';
    }

    remote_path = argv[argc - 2];
    if (remote_path[strlen(remote_path) - 1] == '/') {
        remote_path[strlen(remote_path) - 1] = '\0';
    }

#ifdef DEBUG
    fprintf(stderr, "Download %s to %s\n", local_path, remote_path);
#endif

    if (!init_api()) {
        ret = 1;
        goto free;
    }
    
    ret = do_download(remote_path,
                    local_path,
                    option_overwrite, 
                    option_new);
free:
    return ret;
} 
//}}}


/* 移动，复制 */
int command_move_or_copy(int argc, char **argv, const char *type) {
//{{{
    int ret = 0;
    const char *error   = NULL;
    const char *from    = NULL;
    const char *to      = NULL;

    if (argc < 3) {
        fprintf(stderr, "ERROR: %s 缺少参数\n", type);
        ret = 1;
        goto free;
    }

    from = argv[1];
    to   = argv[2];

    if (!init_api()) {
        ret = 1;
        goto free;
    }

    if (strcmp(type, "cp") == 0) {
        BaiduPCS_Copy(api, from, to);
    } else {
        BaiduPCS_Move(api, from, to);
    }

    error = BaiduPCS_GetError(api);
    if (error != NULL) {
        fprintf(stderr, "ERROR: %s %s -> %s %s\n", type, from, to, error);
        ret = 1;
        goto free;
    }

free:
    return ret;

}
//}}}

/* 删除 */
int command_remove(int argc, char **argv) {
//{{{
    int ret = 0;
    const char *error   = NULL;
    const char *path    = NULL;

    if (argc < 2) {
        fprintf(stderr, "ERROR: rm 缺少参数\n");
        ret = 1;
        goto free;
    }

    path = argv[1];

    if (!init_api()) {
        ret = 1;
        goto free;
    }

    BaiduPCS_Remove(api, path);

    error = BaiduPCS_GetError(api);
    if (error != NULL) {
        fprintf(stderr, "ERROR: rm %s %s\n", path, error);
        ret = 1;
        goto free;
    }

free:
    return ret;
}
//}}}

int main(int argc, char **argv) {
//{{{
    int ret = 0;

    if (argc < 2) {
        usage();
        return 1;
    }
    
    char *command = argv[1];

    argc --;
    argv ++;
    
    curl_global_init(CURL_GLOBAL_ALL);

    if (strcmp(command, "info") == 0) {
        ret = command_info(argc, argv);
    } else if (strcmp(command, "ls") == 0) {
        ret = command_ls(argc, argv);
    } else if (strcmp(command, "upload") == 0 || strcmp(command, "up") == 0) {
        ret = command_upload(argc, argv);
    } else if (strcmp(command, "download") == 0 || strcmp(command, "down") == 0) {
        ret = command_download(argc, argv);
    } else if (strcmp(command, "mv") == 0) {
        ret = command_move_or_copy(argc, argv, "mv");
    } else if (strcmp(command, "cp") == 0) {
        ret = command_move_or_copy(argc, argv, "cp");
    } else if (strcmp(command, "rm") == 0) {
        ret = command_remove(argc, argv);
    } else {
        fprintf(stderr, "未知命令!\n");
        usage();
        ret = 1;
    }

    if (api != NULL) {
        BaiduPCS_Free(api);
    }
    return ret;
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
