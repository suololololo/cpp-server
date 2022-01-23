#include "timer.h"
sort_timer_list::~sort_timer_list()
{
    timer_util *tmp = head;
    while (tmp)
    {
        head = tmp->next;
        delete tmp;
        tmp = head;
    }
}

void sort_timer_list::add_timer(timer_util *timer)
{
    if (!timer)
    {
        return;
    }
    if (head == NULL && tail == NULL)
    {
        head = timer;
        tail = timer;
        return;
    }
    if (timer->expire < head->expire)
    {
        head->prev = timer;
        timer->next = head;
        timer->prev = NULL;
        head = timer;
        return;
    }
    add_timer(timer, head);
}

void sort_timer_list::add_timer(timer_util *timer, timer_util *lst_head)
{
    timer_util *prev = lst_head;
    timer_util *tmp = lst_head->next;
    while (tmp)
    {
        if (timer->expire < tmp->expire)
        {
            tmp->prev->next = timer;
            timer->prev = tmp->prev;
            tmp->prev = timer;
            timer->next = tmp;
            break;
        }
        prev = tmp;
        tmp = tmp->next;
    }
    if (!tmp)
    {
        prev->next = timer;
        timer->prev = tail;
        timer->next = NULL;
        tail = timer;
    }
}

void sort_timer_list::adjust_timer(timer_util *timer)
{
    if (!timer)
    {
        return;
    }

    timer_util *tmp = timer->next;
    if (!tmp || timer->expire < tmp->expire)
    {
        return;
    }

    if (timer == head)
    {
        head = head->next;
        head->prev = NULL;
        timer->next = NULL;
        add_timer(timer, head);
    }
    else
    {
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(timer, timer->next);
    }
}

void sort_timer_list::del_timer(timer_util *timer)
{
    if (!timer)
    {
        return;
    }
    if (timer == head && timer == tail)
    {
        delete timer;
        head = NULL;
        tail = NULL;
        return;
    }
    if (timer == head)
    {
        head = head->next;
        head->prev = NULL;
        delete timer;
        return;
    }
    if (timer == tail)
    {
        tail = tail->prev;
        tail->next = NULL;
        delete timer;
        return;
    }
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}

void sort_timer_list::trick()
{
    if (!head)
    {
        return;
    }
    time_t cur = time(NULL);
    timer_util *tmp = head;
    while (tmp)
    {
        if (cur < tmp->expire)
        {
            break;
        }
        tmp->cb_function(tmp->user_data);
        head = head->next;
        if (head)
        {
            head->prev = NULL;
        }
        delete tmp;
        tmp = head;
    }
}