#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>
#include <list>
#include <exception>
#include "../lock/locker.h"
#include "../mysqlconnect/connect_pool.h"
template <typename T>
class threadpool
{
public:
    threadpool(int actor_model, connection_pool *conn, int thread_num = 8, int max_request = 10000);
    ~threadpool();

    bool append(T *request, int state);
    bool append_p(T *request);

private:
    static void *work(void *);
    void run();

private:
    int m_thread_num;
    int m_max_request;
    pthread_t *m_thread;
    std::list<T *> m_request_queue;
    locker m_lock;
    sem m_sem;
    int m_actor_mode;
    connection_pool *m_connect_pool;
};
template <typename T>
threadpool<T>::threadpool(int actor_model, connection_pool *conn, int thread_num, int max_request) : 
m_thread_num(thread_num),m_max_request(max_request), m_thread(NULL), m_actor_mode(actor_model), m_connect_pool(conn)
{
    if (thread_num <= 0 || max_request <= 0)
    {
        throw std::exception();
    }
    m_thread = new pthread_t[thread_num];
    if (!m_thread)
    {
        throw std::exception();
    }
    for (int i = 0; i < thread_num; i++)
    {
        if (pthread_create(m_thread + i, NULL, work, this) != 0)
        {
            delete[] m_thread;
            throw std::exception();
        }
        if (pthread_detach(m_thread[i]))
        {
            delete[] m_thread;
            throw std::exception();
        }
    }
}

template <typename T>
threadpool<T>::~threadpool()
{
    if (m_thread)
    {
        for (int i = 0; i < m_thread_num; i++)
        {
            pthread_exit(m_thread + i);
        }
    }
    delete[] m_thread;
}
template <typename T>
bool threadpool<T>::append(T *request, int state)
{
    m_lock.lock();
    if (m_request_queue.size() >= m_max_request)
    {
        m_lock.unlock();
        return false;
    }
    if (request == nullptr)
    {
        m_lock.unlock();
        return false;
    }
    request->m_state = state;
    m_request_queue.push_back(request);
    m_lock.unlock();
    m_sem.post();
    return true;
}

template <typename T>
bool threadpool<T>::append_p(T *request)
{
    m_lock.lock();
    if (m_request_queue.size() >= m_max_request)
    {
        m_lock.unlock();
        return false;
    }

    if (request == nullptr)
    {
        m_lock.unlock();
        return false;
    }
    m_request_queue.push_back(request);
    m_lock.unlock();
    m_sem.post();
    return true;
}

template <typename T>
void *threadpool<T>::work(void *arg)
{
    threadpool *pool = (threadpool *)arg;
    pool->run();
    return pool;
}

template <typename T>
void threadpool<T>::run()
{
    while (true)
    {
        m_sem.wait();
        m_lock.lock();
        if (m_request_queue.empty())
        {
            m_lock.unlock();
            continue;
        }
        T *request = m_request_queue.front();
        m_request_queue.pop_front();
        m_lock.unlock();
        if (!request)
        {
            continue;
        }
        if (1 == m_actor_mode)
        {
            // 读
            if (request->m_state == 0)
            {
                if (request->read_once())
                {
                    request->m_improv = 1;
                    connectionRAII mysqlconnect(&request->mysql, m_connect_pool);
                    request->process();
                }
                else
                {
                    request->m_improv = 1;
                    request->m_timeflag = 1;
                }
            }
            else
            {
                if (request->write())
                {
                    request->m_improv = 1;
                }
                else 
                {
                    request->m_improv = 1;
                    request->m_timeflag = 1;
                }
            }
        }
        else
        {
            connectionRAII mysqlconnect(&request->mysql, m_connect_pool);
            request->process();
        }

    }
}

#endif