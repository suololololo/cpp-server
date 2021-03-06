**服务器编程基本框架**

![1](D:\Hexo\source\_posts\服务器编程基本框架\1.png)
服务器基本模块分为 IO处理单元，用于处理客户端连接，IO读写网络数据。逻辑处理单元，用于处理业务。网络存储单元，用于处理本地数据库，文件。各个单元之间用过请求队列通信，从而实现协同通信。
**IO方式**
**阻塞IO**:发起调用，调用过程中，进程被阻塞，期间什么事情都不做，不断去问数据是否准备好。必须等函数返回，才能进行下一步工作
**非阻塞IO**:函数调用后，直接返回结果，不论事件有没有发生。如果事件没有发生，则返回-1，根据errno结果区分，errno被设置为EAGIN时，希望再试一次。
**信号驱动IO**：linux注册一个信号处理函数，进程并不阻塞，继续运行，当有IO信号来时，再去处理。
**IO复用**：linux中一个进程同时监听多个IO事件，通过系统调用监听多个IO事件，IO事件是非阻塞的，但这个IO复用函数的阻塞的。
**异步IO**：linux发起调用时，由内核负责完成IO，不论有没有完成都立即返回。等内核完成时，会通知进程

**同步IO和异步IO的区别**：同步IO内核通知的就绪事件，而异步IO通知的是事件完成事件。因此同步IO负责执行IO操作的是IO操作发起者。而异步IO则是又内核线程来完成。
**阻塞IO，非阻塞IO,信号驱动IO,IO复用都是同步IO，因为内核通知的都是就绪事件。**

**事件处理模式**
Reactor模式：主线程（IO单元）只负责监听文件描述符上是否有事件发生，有的话就立即通知工作线程（逻辑处理单元），让工作线程去处理。除此之外，主线程不做任何其他实质性工作，读写数据，处理新连接，业务处理全在工作线程
Proactor模式：所有IO操作（读写数据，处理新连接）都交给主线程完成，工作线程只负责业务处理。

**同步IO模拟Reactor模式**
由于Proactor模式需要异步IO支持，而异步IO需要内核支持，但现在不太成熟，使用较少。这里使用同步IO模拟Reactor模式。
总体工作流程：
1.主线程往epoll内核事件表注册socket读就绪事件
2.主线程调用epoll_wait 监听内核事件表，等待socket有数据可读
3.当socket上有数据可读时，epoll_wait通知主线程，主线程将socket可读事件，放入请求队列中。
4.睡眠在请求队列的工作线程被唤醒，读取socket数据，处理客户请求，往epoll内核事件表，注册该socket的写就绪事件。
5.主线程调用epoll_wait 等待socket可写。
6.当socket可写时，主线程将socket可写事件，放入请求队列中
7.睡眠在请求队列的工作线程被唤醒，往socket上写处理结果

**高效并发模式**
并发编程的方式主要有多进程和多线程两种方式，但这里讨论的是IO单元和逻辑处理单元高效协调完成任务的方法。
1.半同步/半异步模式
2.领导者/追随者模式
这里的同步/异步指的是程序完全按照代码顺序来执行，或者需要由系统事件来驱动
**半同步/半异步模式**
主线程为异步线程，负责处理IO事件，然后将客户请求封装成对象插入请求队列中。请求队列通知工作线程来处理。
半同步/半反应堆模式 是半同步/半异步模式的变体。将异步线程具体化为处理客户端连接，此时插入请求队列中的就是就绪连接。
LT与ET
LT模式下，当epoll_wait 检测到事件并通知应用程序后，应用程序可以不立即处理。在下一次调用epoll_wait时，还会再次向应用程序通知此事件。
ET模式下，当epoll_wait检测到事件并通知应用程序后，必须处理此事件，因为后续的epoll_wait不会再通知此事件。必须一次性读完数据，使用非阻塞IO，直到出现EAGIN。
**EPOLLONESHOT**
当一个线程读完某个socket上的数据后开始处理数据，这个socket上又有数据可读，这时唤醒了另一个线程，开始读数据。出现了两个线程处理一个socket的数据，我们希望一个socket的数据始终由一个线程处理（因为数据可能会有前后关联）。
解决办法，给socket注册EPOLLONESHOT事件，一个线程处理socket时，其他线程无法处理，处理完后需要重置EPOLLNESHOT事件






