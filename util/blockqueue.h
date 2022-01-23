#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H
#include <iostream>
#include <pthread.h>
#include <sys/time.h>
#include <stdlib.h>
#include "../lock/locker.h"
// 循环数组实现阻塞队列
template <typename T>
class block_queue
{
public:
    block_queue(int max_size)
    {
        if (max_size <= 0)
        {
            exit(-1);
        }
        m_max_size = max_size;
        m_array = new T[m_max_size];
        m_size = 0;
        m_front = -1;
        m_back = -1;
    }
    ~block_queue()
    {
        m_lock.lock();
        if (m_array != NULL)
        {
            delete[] m_array;
        }
        m_lock.unlock();
    }
    void clear()
    {
        m_lock.lock();
        m_size = 0;
        m_front = -1;
        m_back = -1;
        m_lock.unlock();
    }
    bool full()
    {
        bool is_full = false;
        m_lock.lock();
        if (m_size >+ m_max_size)
        {
            is_full == true;
        }
        m_lock.unlock();
        return is_full;
    }
    bool empty()
    {
        bool is_empty = false;
        m_lock.lock();
        if (m_size <= 0)
        {
            is_empty = true;
        }
        m_lock.unlock();
        return is_empty;
    }

    bool front(T &value)
    {
        m_lock.lock();
        if (m_size == 0)
        {
            m_lock.unlock();
            return false;
        }
        value = m_array[m_front];
        m_lock.unlock();
        return true;
    }
    bool back(T &value)
    {
        m_lock.lock();
        if (m_size == 0)
        {
            m_lock.unlock();
            return false;
        }
        value = m_array[m_back];
        m_lock.unlock();
        return true;
    }
    int size()
    {
        int size = 0;
        m_lock.lock();
        size = m_size;
        m_lock.unlock();
        return size;
    }
    int max_size()
    {
        int max_size = 0;
        m_lock.lock();
        max_size = m_max_size;
        m_lock.unlock();
        return max_size;
    }
    // 插入队列 并唤醒线程
    bool push(const T &item)
    {
        m_lock.lock();
        if (m_size == m_max_size)
        {
            m_lock.unlock();
            return false;
        }
        m_back = (m_back + 1) % m_max_size;
        m_array[m_back] = item;
        m_size++;
        m_cond.broadcast();
        m_lock.unlock();
        return true;
    }
    // 取出任务， 如果队列为空则循环等待
    bool pop(T &item)
    {
        m_lock.lock();
        while (m_size <= 0)
        {
            if (!m_cond.wait(m_lock.get()))
            {
                m_lock.unlock();
                return false;
            }
        }

        m_front = (m_front + 1) % m_max_size;
        item = m_array[m_front];
        m_size--;
        m_lock.unlock();
        return true;
    }
    bool pop(T &item, int ms_timeout)
    {
        struct timespec t = {0, 0};
        struct timeval now = {0, 0};
        gettimeofday(&now, NULL);
        m_lock.lock();
        if (m_size <= 0)
        {
            t.tv_sec = now.tv_sec + ms_timeout / 1000;
            t.tv_nsec = (ms_timeout % 1000) * 1000;
            if (!m_cond.timewait(m_lock.get(), t))
            {
                m_lock.unlock();
                return false;
            }
        }
        if (m_size <= 0)
        {
            m_lock.unlock();
            return false;
        }

        m_front = (m_front + 1) % m_max_size;
        item = m_array[m_front];
        m_size--;
        m_lock.unlock();
        return true;
    }

private:
    int m_max_size;
    T *m_array;
    int m_front;
    int m_back;
    int m_size;
    locker m_lock;
    cond m_cond;
};

#endif