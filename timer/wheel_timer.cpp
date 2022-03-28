#include "wheel_timer.h"
#include "../http/http_conn.h"

TwTimer* TimeWheel::add_timer( int timeout ) {
    if( timeout < 0 ) {
        return NULL;
    }

    // 记录多少个tick后被触发，不足最小单位SI的记为1，其余为timeout/SI
    int ticks = 0;
    if( timeout < SI ) {
        ticks = 1;
    } else {
        ticks = timeout / SI;
    }

    int rotation = ticks / N;  // 被触发的圈数
    int ts = ( cur_slot + ticks % N ) % N;  // 被插入的槽
    TwTimer* timer = new TwTimer( rotation, ts );

    // 如果链表为空，则放到头，否则插入到第一个位置
    if( !slot[ts] ) {
        slot[ts] = timer;
    } else {
        timer->next = slot[ts];
        slot[ts]->pre = timer;
        slot[ts] = timer;
    }

    return timer;
}


// 删除定时器
void TimeWheel::del_timer( TwTimer* timer ) {
    if( !timer ) {
        return;
    }

    // 注意链表为双向的
    int ts = timer->time_slot;
    if( timer == slot[ts] ) {
        slot[ts] = slot[ts]->next;
        if( slot[ts] ) {
            slot[ts]->pre = NULL;
        }
    } else {
        timer->pre->next = timer->next;
        if( timer->next ) {
            timer->next->pre = timer->pre;
        }
    }
    delete timer;
}

// SI时间到后，条用该函数，时间轮向前滚动一个槽的间隔
void TimeWheel::tick() {
    TwTimer* tmp = slot[cur_slot];
    while( tmp ) {
        if( tmp->rotation > 0 ) {  // 定时时间未到
            tmp->rotation--;
            tmp = tmp->next;
        } else {  // 定时时间已到
            tmp->cb_func( tmp->user_data );
            if( tmp == slot[cur_slot] ) {  // tmp位于链表首
                slot[cur_slot] = tmp->next;
                if( slot[cur_slot] ) {
                    slot[cur_slot]->pre = NULL;
                }
                delete tmp;
                tmp = slot[cur_slot];
            } else {  // tmp位于链表中
                tmp->pre->next = tmp->next;
                if( tmp->next ) {
                    tmp->next->pre = tmp->pre;
                }
                TwTimer* tmp2 = tmp->next;
                delete tmp;
                tmp = tmp2;
            }
        }
    }
    cur_slot = ( cur_slot + 1 ) % N;
}

void Utils::init(int timeslot)
{
    m_TIMESLOT = timeslot;
}

//对文件描述符设置非阻塞
int Utils::setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

//将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;

    if (1 == TRIGMode)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

//信号处理函数
void Utils::sig_handler(int sig)
{
    //为保证函数的可重入性，保留原来的errno
    int save_errno = errno;
    int msg = sig;
    send(u_pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}

//设置信号函数
void Utils::addsig(int sig, void(handler)(int), bool restart)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart)
        sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

//定时处理任务，重新定时以不断触发SIGALRM信号
void Utils::timer_handler()
{
    m_timer_lst.tick();
    alarm(m_TIMESLOT);
}

void Utils::show_error(int connfd, const char *info)
{
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int *Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;

class Utils;
void cb_func(client_data *user_data)
{
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    http_conn::m_user_count--;
}