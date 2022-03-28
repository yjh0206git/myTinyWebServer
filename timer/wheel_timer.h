#ifndef _TIMEWHEEL_H_
#define _TIMEWHEEL_H_

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>

#include <time.h>
#include "../log/log.h"

const int BUFFER_SIZE = 64;

class TwTimer;

// 用户数据，绑定socket和定时器
struct client_data {
    sockaddr_in address;
    int sockfd;
    char buf[BUFFER_SIZE];
    TwTimer* timer;
};

// 定时器类，时间轮采用双向链表
class TwTimer {
public:
    int rotation;  // 定时器转多少圈后生效
    int time_slot;  // 记录定时器属于时间轮的哪个时间槽
    client_data* user_data;  // 客户数据
    TwTimer* next;  // 指向下一个定时器
    TwTimer* pre;  // 指向上一个定时器
    time_t expire;
    time_t start_time;
public:
    TwTimer( int rot, int ts ) : rotation(rot), time_slot(ts), next(NULL), pre(NULL) {}
    void (*cb_func)( client_data * );  // 回调函数
};

class TimeWheel {
private: 
    static const int N = 60;  // 槽的数目
    static const int SI = 1;  // 定时器槽之间时间间隔
    TwTimer* slot[ N ];  // 时间轮的槽，指向一个定时器链表，链表无序
    int cur_slot;  // 当前槽
public:
    TimeWheel() {
        cur_slot=0;
        for( int i = 0; i < N; i++ ) {
            slot[i] = NULL;
        }
    }

    ~TimeWheel() {
        for( int i = 0; i < N; i++ ) {
            TwTimer* tmp;
            while( tmp = slot[i], tmp ) {
                slot[i] = tmp->next;
                delete tmp;
            }
        }
    }

    TwTimer* add_timer( int timeout );  // 根据定时值创建定时器，并插入槽中
    void del_timer( TwTimer* timer );
    void tick();
};

class Utils
{
public:
    Utils() {}
    ~Utils() {}

    void init(int timeslot);

    //对文件描述符设置非阻塞
    int setnonblocking(int fd);

    //将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
    void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

    //信号处理函数
    static void sig_handler(int sig);

    //设置信号函数
    void addsig(int sig, void(handler)(int), bool restart = true);

    //定时处理任务，重新定时以不断触发SIGALRM信号
    void timer_handler();

    void show_error(int connfd, const char *info);

public:
    static int *u_pipefd;
    TimeWheel m_timer_lst;
    static int u_epollfd;
    int m_TIMESLOT;
};

void cb_func(client_data *user_data);

#endif