/*
正则语法:
如果你想查找元字符本身的话，比如你查找.,或者*,就出现了问题：你没办法指定它们，因为它们会被解释成别的意思。这时你就得使用\来取消这些字符的特殊意义。因此，你应该使用\.和\*。当然，要查找\本身，你也得用\\.


.     匹配除换行符以外的任意字符
\w     匹配字母或数字或下划线或汉字
\s     匹配任意的空白符 任意的空白符，包括空格，制表符(Tab)，换行符，中文全角空格等。
\d     匹配数字      如:\d+匹配1个或更多连续的数字
\b     匹配单词的开始或结束
^     匹配字符串的开始
$     匹配字符串的结束

*     重复零次或更多次    如:然后是任意数量的字母或数字(\w*)   +是和*类似的元字符，不同的是*匹配重复任意次(可能是0次)，而+则匹配重复1次或更多次。

+     重复一次或更多次    如:\d+匹配1个或更多连续的数字  +是和*类似的元字符，不同的是*匹配重复任意次(可能是0次)，而+则匹配重复1次或更多次。

?     重复零次或一次
{n}     重复n次
{n,}     重复n次或更多次
{n,m}     重复n到m次
*?     重复任意次，但尽可能少重复
+?     重复1次或更多次，但尽可能少重复
??     重复0次或1次，但尽可能少重复
{n,m}?     重复n到m次，但尽可能少重复
{n,}?     重复n次以上，但尽可能少重复

\W     匹配任意不是字母，数字，下划线，汉字的字符
\S     匹配任意不是空白符的字符
\D     匹配任意非数字的字符
\B     匹配不是单词开头或结束的位置
[^x]     匹配除了x以外的任意字符
[^aeiou]     匹配除了aeiou这几个字母以外的任意字符

捕获     (exp)     匹配exp,并捕获文本到自动命名的组里
(?<name>exp)     匹配exp,并捕获文本到名称为name的组里，也可以写成(?'name'exp)
(?:exp)     匹配exp,不捕获匹配的文本，也不给此分组分配组号
零宽断言     (?=exp)     匹配exp前面的位置
(?<=exp)     匹配exp后面的位置
(?!exp)     匹配后面跟的不是exp的位置
(?<!exp)     匹配前面不是exp的位置
注释     (?#comment)     这种类型的分组不对正则表达式的处理产生任何影响，用于提供注释让人阅读










PCRE函数简介和使用示例 .
分类： C/C++ 2011-03-13 23:56 15411人阅读 评论(6) 收藏 举报 
正则表达式listbuffercompilationnullperlPCRE是一个NFA正则引擎，不然不能提供完全与Perl一致的正则语法功能。
但它同时也实现了DFA，只是满足数学意义上的正则。

 

PCRE提供了19个接口函数，为了简单介绍，使用PCRE内带的测试程序(pcretest.c)示例用法。

1. pcre_compile

       原型：

         #include <pcre.h> 

pcre *pcre_compile(const char *pattern, int options, const char **errptr, int *erroffset, const unsigned char *tableptr); 

功能：将一个正则表达式编译成一个内部表示，在匹配多个字符串时，可以加速匹配。其同pcre_compile2功能一样只是缺少一个参数errorcodeptr。

参数：

pattern    正则表达式

options     为0，或者其他参数选项

       errptr       出错消息

        erroffset  出错位置

tableptr   指向一个字符数组的指针，可以设置为空NULL

示例：

L1720     re = pcre_compile((char *)p, options, &error, &erroroffset, tables);

 

2. pcre_compile2

       原型：

#include <pcre.h> 

pcre *pcre_compile2(const char *pattern, int options, int *errorcodeptr, const char **errptr, int *erroffset, const unsigned char *tableptr); 

功能：将一个正则表达式编译成一个内部表示，在匹配多个字符串时，可以加速匹配。其同pcre_compile功能一样只是多一个参数errorcodeptr。

参数：

pattern    正则表达式

options     为0，或者其他参数选项

errorcodeptr    存放出错码

       errptr       出错消息

        erroffset  出错位置

tableptr   指向一个字符数组的指针，可以设置为空NULL

 

3. pcre_config

       原型：

#include <pcre.h> 

int pcre_config(int what, void *where);

功能：查询当前PCRE版本中使用的选项信息。

参数：

what         选项名

where       存储结果的位置

示例：

Line1312 (void)pcre_config(PCRE_CONFIG_POSIX_MALLOC_THRESHOLD, &rc);

 

4. pcre_copy_named_substring

       原型：

#include <pcre.h> 

int pcre_copy_named_substring(const pcre *code, const char *subject, int *ovector, int stringcount, const char *stringname, 
char *buffer, int buffersize); 

功能：根据名字获取捕获的字串。

参数：

code                            成功匹配的模式

subject               匹配的串

ovector              pcre_exec() 使用的偏移向量

stringcount   pcre_exec()的返回值

stringname       捕获字串的名字

buffer                 用来存储的缓冲区

buffersize                   缓冲区大小

示例：

Line2730 int rc = pcre_copy_named_substring(re, (char *)bptr, use_offsets,

            count, (char *)copynamesptr, copybuffer, sizeof(copybuffer));

 

5. pcre_copy_substring
       原型：

#include <pcre.h> 

int pcre_copy_substring(const char *subject, int *ovector, int stringcount, int stringnumber, char *buffer, int buffersize); 

功能：根据编号获取捕获的字串。

参数：

code                            成功匹配的模式

subject               匹配的串

ovector              pcre_exec() 使用的偏移向量

stringcount   pcre_exec()的返回值

stringnumber   捕获字串编号

buffer                 用来存储的缓冲区

buffersize                   缓冲区大小

示例：

Line2730 int rc = pcre_copy_substring((char *)bptr, use_offsets, count,

              i, copybuffer, sizeof(copybuffer));

 

6. pcre_dfa_exec

       原型：

#include <pcre.h> 

int pcre_dfa_exec(const pcre *code, const pcre_extra *extra, const char *subject, int length, int startoffset, int options, 
int *ovector, int ovecsize, int *workspace, int wscount);

功能：使用编译好的模式进行匹配，采用的是一种非传统的方法DFA，只是对匹配串扫描一次（与Perl不兼容）。

参数：

code                   编译好的模式

extra         指向一个pcre_extra结构体，可以为NULL

subject    需要匹配的字符串

length       匹配的字符串长度（Byte）

startoffset        匹配的开始位置

options     选项位

ovector    指向一个结果的整型数组

ovecsize   数组大小

workspace        一个工作区数组

wscount   数组大小

示例：

Line2730 count = pcre_dfa_exec(re, extra, (char *)bptr, len, start_offset,

              options | g_notempty, use_offsets, use_size_offsets, workspace,

              sizeof(workspace)/sizeof(int));

 

7. pcre_copy_substring
       原型：

#include <pcre.h> 

int pcre_exec(const pcre *code, const pcre_extra *extra, const char *subject, int length, int startoffset, int options, 
int *ovector, int ovecsize);

功能：使用编译好的模式进行匹配，采用与Perl相似的算法，返回匹配串的偏移位置。。

参数：

code                   编译好的模式

extra         指向一个pcre_extra结构体，可以为NULL

subject    需要匹配的字符串

length       匹配的字符串长度（Byte）

startoffset        匹配的开始位置

options     选项位

ovector    指向一个结果的整型数组

ovecsize   数组大小

 

8. pcre_free_substring
       原型：

#include <pcre.h> 

void pcre_free_substring(const char *stringptr);

功能：释放pcre_get_substring()和pcre_get_named_substring()申请的内存空间。

参数：

stringptr            指向字符串的指针

示例：

Line2730        const char *substring;

int rc = pcre_get_substring((char *)bptr, use_offsets, count,

              i, &substring);

……

pcre_free_substring(substring);

 

9. pcre_free_substring_list
       原型：

#include <pcre.h> 

void pcre_free_substring_list(const char **stringptr);

功能：释放由pcre_get_substring_list申请的内存空间。

参数：

stringptr            指向字符串数组的指针

示例：

Line2773        const char **stringlist;

int rc = pcre_get_substring_list((char *)bptr, use_offsets, count,

……

pcre_free_substring_list(stringlist);

 

10. pcre_fullinfo
       原型：

#include <pcre.h> 

int pcre_fullinfo(const pcre *code, const pcre_extra *extra, int what, void *where);

功能：返回编译出来的模式的信息。

参数：

code          编译好的模式

extra         pcre_study()的返回值，或者NULL
what         什么信息
where       存储位置
示例：

Line997          if ((rc = pcre_fullinfo(re, study, option, ptr)) < 0)

fprintf(outfile, "Error %d from pcre_fullinfo(%d)/n", rc, option);

}

 

11. pcre_get_named_substring
       原型：

#include <pcre.h> 

int pcre_get_named_substring(const pcre *code, const char *subject, int *ovector, int stringcount, const char *stringname, const char **stringptr);

功能：根据编号获取捕获的字串。

参数：

code                            成功匹配的模式

subject               匹配的串

ovector              pcre_exec() 使用的偏移向量

stringcount   pcre_exec()的返回值

stringname       捕获字串的名字

stringptr     存放结果的字符串指针
示例：

Line2759        const char *substring;

int rc = pcre_get_named_substring(re, (char *)bptr, use_offsets,

            count, (char *)getnamesptr, &substring);

 

12. pcre_get_stringnumber
       原型：

#include <pcre.h> 

int pcre_get_stringnumber(const pcre *code, const char *name);

功能：根据命名捕获的名字获取对应的编号。

参数：

code                            成功匹配的模式

name                 捕获名字

 

13. pcre_get_substring
       原型：

#include <pcre.h> 

int pcre_get_substring(const char *subject, int *ovector, int stringcount, int stringnumber, const char **stringptr);

功能：获取匹配的子串。

参数：

subject       成功匹配的串
ovector       pcre_exec() 使用的偏移向量

stringcount    pcre_exec()的返回值
stringnumber  获取的字符串编号
stringptr      字符串指针
 

14. pcre_get_substring_list
       原型：

#include <pcre.h> 

int pcre_get_substring_list(const char *subject, int *ovector, int stringcount, const char ***listptr);

功能：获取匹配的所有子串。

参数：

subject       成功匹配的串
ovector       pcre_exec() 使用的偏移向量

stringcount    pcre_exec()的返回值
listptr             字符串列表的指针
 

15. pcre_info

       原型：

#include <pcre.h> 

int pcre_info(const pcre *code, int *optptr, int *firstcharptr);

已过时，使用pcre_fullinfo替代。
 

16. pcre_maketables
       原型：

#include <pcre.h> 

const unsigned char *pcre_maketables(void);

功能：生成一个字符表，表中每一个元素的值不大于256，可以用它传给pcre_compile()替换掉内建的字符表。

参数：

示例：

Line2759 tables = pcre_maketables();

 

17. pcre_refcount
       原型：

#include <pcre.h> 

int pcre_refcount(pcre *code, int adjust);

功能：编译模式的引用计数。

参数：

code       已编译的模式
adjust      调整的引用计数值

 

18. pcre_study
       原型：

#include <pcre.h> 

pcre_extra *pcre_study(const pcre *code, int options, const char **errptr);

功能：对编译的模式进行学习，提取可以加速匹配过程的信息。

参数：

code      已编译的模式
options    选项
errptr     出错消息
示例：

Line1797 extra = pcre_study(re, study_options, &error);

 

19. pcre_version
       原型：

#include <pcre.h> 

char *pcre_version(void);

功能：返回PCRE的版本信息。

参数：

示例：

Line1384 if (!quiet) fprintf(outfile, "PCRE version %s/n/n", pcre_version());

 

程序实例：

[cpp] view plaincopyprint?
01.#define PCRE_STATIC // 静态库编译选项   
02.#include <stdio.h>   
03.#include <string.h>   
04.#include <pcre.h>   
05.#define OVECCOUNT 30 /* should be a multiple of 3 * /   
06.#define EBUFLEN 128   
07.#define BUFLEN 1024   
08.  
09.int main()  
10.{  
11.    pcre  *re;  
12.    const char *error;  
13.    int  erroffset;  
14.    int  ovector[OVECCOUNT];  
15.    int  rc, i;  
16.    char  src [] = "111 <title>Hello World</title> 222";   // 要被用来匹配的字符串   
17.    char  pattern [] = "<title>(.*)</(tit)le>";              // 将要被编译的字符串形式的正则表达式   
18.    printf("String : %s/n", src);  
19.    printf("Pattern: /"%s/"/n", pattern);  
20.    re = pcre_compile(pattern,       // pattern, 输入参数，将要被编译的字符串形式的正则表达式   
21.                      0,            // options, 输入参数，用来指定编译时的一些选项   
22.                      &error,       // errptr, 输出参数，用来输出错误信息   
23.                      &erroffset,   // erroffset, 输出参数，pattern中出错位置的偏移量   
24.                      NULL);        // tableptr, 输入参数，用来指定字符表，一般情况用NULL   
25.    // 返回值：被编译好的正则表达式的pcre内部表示结构   
26.    if (re == NULL) {                 //如果编译失败，返回错误信息   
27.        printf("PCRE compilation failed at offset %d: %s/n", erroffset, error);  
28.        return 1;  
29.    }  
30.    rc = pcre_exec(re,            // code, 输入参数，用pcre_compile编译好的正则表达结构的指针   
31.                   NULL,          // extra, 输入参数，用来向pcre_exec传一些额外的数据信息的结构的指针   
32.                   src,           // subject, 输入参数，要被用来匹配的字符串   
33.                   strlen(src),  // length, 输入参数， 要被用来匹配的字符串的指针   
34.                   0,             // startoffset, 输入参数，用来指定subject从什么位置开始被匹配的偏移量   
35.                   0,             // options, 输入参数， 用来指定匹配过程中的一些选项   
36.                   ovector,       // ovector, 输出参数，用来返回匹配位置偏移量的数组   
37.                   OVECCOUNT);    // ovecsize, 输入参数， 用来返回匹配位置偏移量的数组的最大大小   
38.    // 返回值：匹配成功返回非负数，没有匹配返回负数   
39.    if (rc < 0) {                     //如果没有匹配，返回错误信息   
40.        if (rc == PCRE_ERROR_NOMATCH) printf("Sorry, no match .../n");  
41.        else printf("Matching error %d/n", rc);  
42.        pcre_free(re);  
43.        return 1;  
44.    }  
45.    printf("/nOK, has matched .../n/n");   //没有出错，已经匹配   
46.    for (i = 0; i < rc; i++) {             //分别取出捕获分组 $0整个正则公式 $1第一个()   
47.        char *substring_start = src + ovector[2*i];  
48.        int substring_length = ovector[2*i+1] - ovector[2*i];  
49.        printf("$%2d: %.*s/n", i, substring_length, substring_start);  
50.    }  
51.    pcre_free(re);                     // 编译正则表达式re 释放内存   
52.    return 0;  
53.}  
#define PCRE_STATIC // 静态库编译选项
#include <stdio.h>
#include <string.h>
#include <pcre.h>
#define OVECCOUNT 30 /* should be a multiple of 3 * /
#define EBUFLEN 128
#define BUFLEN 1024

int main()
{
    pcre  *re;
    const char *error;
    int  erroffset;
    int  ovector[OVECCOUNT];
    int  rc, i;
    char  src [] = "111 <title>Hello World</title> 222";   // 要被用来匹配的字符串
    char  pattern [] = "<title>(.*)</(tit)le>";              // 将要被编译的字符串形式的正则表达式
    printf("String : %s/n", src);
    printf("Pattern: /"%s/"/n", pattern);
    re = pcre_compile(pattern,       // pattern, 输入参数，将要被编译的字符串形式的正则表达式
                      0,            // options, 输入参数，用来指定编译时的一些选项
                      &error,       // errptr, 输出参数，用来输出错误信息
                      &erroffset,   // erroffset, 输出参数，pattern中出错位置的偏移量
                      NULL);        // tableptr, 输入参数，用来指定字符表，一般情况用NULL
    // 返回值：被编译好的正则表达式的pcre内部表示结构
    if (re == NULL) {                 //如果编译失败，返回错误信息
        printf("PCRE compilation failed at offset %d: %s/n", erroffset, error);
        return 1;
    }
    rc = pcre_exec(re,            // code, 输入参数，用pcre_compile编译好的正则表达结构的指针
                   NULL,          // extra, 输入参数，用来向pcre_exec传一些额外的数据信息的结构的指针
                   src,           // subject, 输入参数，要被用来匹配的字符串
                   strlen(src),  // length, 输入参数， 要被用来匹配的字符串的指针
                   0,             // startoffset, 输入参数，用来指定subject从什么位置开始被匹配的偏移量
                   0,             // options, 输入参数， 用来指定匹配过程中的一些选项
                   ovector,       // ovector, 输出参数，用来返回匹配位置偏移量的数组
                   OVECCOUNT);    // ovecsize, 输入参数， 用来返回匹配位置偏移量的数组的最大大小
    // 返回值：匹配成功返回非负数，没有匹配返回负数
    if (rc < 0) {                     //如果没有匹配，返回错误信息
        if (rc == PCRE_ERROR_NOMATCH) printf("Sorry, no match .../n");
        else printf("Matching error %d/n", rc);
        pcre_free(re);
        return 1;
    }
    printf("/nOK, has matched .../n/n");   //没有出错，已经匹配
    for (i = 0; i < rc; i++) {             //分别取出捕获分组 $0整个正则公式 $1第一个()
        char *substring_start = src + ovector[2*i];
        int substring_length = ovector[2*i+1] - ovector[2*i];
        printf("$%2d: %.*s/n", i, substring_length, substring_start);
    }
    pcre_free(re);                     // 编译正则表达式re 释放内存
    return 0;
}
 


程序来自网上，看到有人不理解最后一个for循环的含义，ovector返回的是匹配字符串的偏移，包括起始偏移和结束偏移，所以就有循环内部的2*i处理。
*/
