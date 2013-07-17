#说明
命令行工具基于libcurl+ [百度PCS的rest API](http://developer.baidu.com/wiki/index.php?title=docs/pcs/rest/file_data_apis_list) ，适用于linux,Mac OS
#安装
编译前请修改`baidu_pcs.c`中的两个全局变量为自己的API Key和Secret Key
~~~
/* pcs api key */
static const char *option_api_key     = "";
/* pcs api secret */
static const char *option_api_secret  = "";
~~~
然后
~~~
make
make install #这一步可以省略咯，就一个可行行文件，可以自己随便copy到哪里。。
~~~
#使用方法
~~~
使用方法: baidu_pcs 命令 [选项]

命令列表:

info      查看云盘信息

ls        列出远程文件或目录
          选项:
            -l 显示详细信息
            -r 递归子目录

upload   [选项] [本地路径] [远程路径] 上传文件或目录
          选项:
            覆盖策略
            默认:略过已存在同名远程文件
            -o 覆盖远程同名文件
            -n 如果存在同名文件，创建以日期结尾的新文件

            -l 跟随软链

download [选项] [远程路径] [本地路径] 下载文件或目录
          选项:
            -o 覆盖本地同名文件
            -n 如果存在同名文件，创建以日期结尾的新文件
cp       [远程路径] [目的远程路径] 复制远程文件或目录
mv       [远程路径] [目的远程路径] 移动远程文件或目录
rm       [远程路径] 删除远程文件或目录
~~~
#你可能需要知道
1. 大于10M的文件会进行分片上传
2. API使用https协议，curl初始化时设置了速度较快的rc4加密方式
3. 所有请求失败会重试5次
4. 非上传请求，5秒连接超时，20秒请求超时
5. 上传请求，5秒连接超时，文件尺寸/(10K/s)的上传超时
6. 上面你都可以自己改了重新编译。。。
