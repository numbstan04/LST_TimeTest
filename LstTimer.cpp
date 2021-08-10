#include <time.h>
#include <netinet/in.h>
#include <iostream>

#define BUFFER_SIZE 128
/*
 * 用户数据结构
 *
*/
class util_timer;

struct client_data{
    sockaddr_in address;
    int sockfd;
    char buf[BUFFER_SIZE];
    util_timer* timer;
};

/*定时器类*/
class util_timer{
public:
    util_timer() : prev(nullptr),next(nullptr){}

public:
    time_t expire;//超时时间
    util_timer* prev;   //指向前一个定时器
    util_timer* next;   //指向后一个定时器

    void (*cb_func)(client_data*);//任务回调函数
    client_data* user_data; //回调函数处理的客户数据，由定时器传递给回调函数
};

/*定时器升序双向链表*/
class sort_timer_list{
private:
    util_timer* head;
    util_timer* tail;

public:
    //链表构造
    sort_timer_list():head(nullptr),tail(nullptr){}

    //链表销毁，删除所有定时器
    ~sort_timer_list()
    {
        util_timer* tmp = head;
        while (tmp) {
            head = tmp->next;
            delete tmp;
            tmp = head;
        }
    }

    //将定时器添加到链表中
    void add_timer(util_timer *timer)
    {
        if (timer == nullptr) {
            return ;
        }
        if(!head) {
            head = tail = timer;
            return ;
        }
        //当添加的定时器超时时间小于当前链表所有定时器的超时时间，则将它放在定时器的头部，作为链表的新头节点
        if (timer->expire < head->expire) {
            timer->next = head;
            head->prev = timer;
            head = timer;
            return ;
        }
        //否则调用重载add_timer(util_timer *timer,util_timer *head),将它添加到合适的位置，以保证链表的升序特性
        add_timer(timer, head);

    }

    //重载add_timer
    void add_timer(util_timer* timer,util_timer* head)
    {
        util_timer* newHead = head;
        util_timer* tmp = newHead->next;

        while (tmp) {
            if (timer->expire < tmp->expire) {
                newHead->next = timer;
                timer->prev = newHead;
                timer->next = tmp;
                tmp->prev = timer;
                break;
            }
            newHead = tmp;
            tmp = tmp->next;
        }

        //若遍历完链表，仍未找到超时时间大于添加的定时器的超时时间，则将定时器添加到链表末尾
        if (!tmp) {
            newHead->next = timer;
            timer->prev = newHead;
            timer->next = nullptr;
            tail = timer;
        }
    }

    /*当某个定时任务发生变化时，调整对应定时器的位置，只考虑被调整的定时器超时时间被延长，则将该定时器向后移动
     * */
    void adjust_timer(util_timer *timer){
        if (timer == nullptr) {
            return;
        }
        util_timer* tmp = timer->next;

        //如果该定时器在链表的为节点，或者该定时器的超时时间小于下一个节点的定时器时间，则不需要调整
        if (!tmp || timer->expire < tmp->expire) {
            return;
        }

        //如果目标定时器是链表的头节点，则将该定时器从链表中取出并重新插入
        if (timer == head) {
            head = head->next;
            head->prev = nullptr;
            timer->next = nullptr;
            add_timer(timer, head);
        }else
        {
            //若该定时器不是链表的头节点，则将该节点从链表取出，然后再插入到之后的链表当中去
            timer->prev->next = timer->next;
            timer->next->prev = timer->prev;
            add_timer(timer, timer->next);
        }
    }

    //将定时器从timer链表中删除
    void del_timer(util_timer *timer){
        if (!timer) {
            return ;
        }

        //若该链表只有一个节点
        if ((timer == head) && (timer == tail)) {
            delete timer;
            head = nullptr;
            tail = nullptr;
            return;
        }
        //若该定时器为链表的头节点并且不止一个节点，重置链表
        if(timer == head) {
            head = head->next;
            head->prev = nullptr;
            delete timer;
            return ;
        }
        //若该定时器为链表的尾节点并且不止一个节点，重置链表
        if (timer == tail) {
            tail = tail->next;
            tail->next = nullptr;
            delete timer;
            return;
        }
        //若该节点在链表中间
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        delete timer;
    }

    //SIGALRM信号每次被触发就在其信号处理函数中执行tick函数，以处理链表上的任务
    void tick(){
        if (!head) {
            return;
        }
        std::cout << "timer tick"<<std::endl;
        time_t cur = time(nullptr);
        //从头节点依次处理每一个定时器
        util_timer* tmp = head;
        while (tmp) {
            //由于每个定时器都使用绝对时间作为超时值，因此将定时器的超时时间与当前系统时间进行对比，断定定时器是否到期
            if (cur < tmp->expire) {
                break;
            }
            //调用定时器的回调函数，执行回调任务
            tmp->cb_func(tmp->user_data);
            head = tmp->next;
            if (head) {
                head->prev = nullptr;
            }
            delete tmp;
            tmp = head;
        }
    }
};