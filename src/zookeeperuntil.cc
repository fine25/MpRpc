#include "zookeeperutil.h"
#include "mprpcapplication.h"

#include <semaphore.h>
#include <iostream>

// 回调、全局watcher观察器，zkserver给zkclient的通知
void global_wathcher(zhandle_t *zh, int type, int state,
                     const char *path, void *WatcherCtx)
{
    if (type == ZOO_SESSION_EVENT) // 回调的消息类型是和会话相关的消息类型
    {
        if (state == ZOO_CONNECTED_STATE) // zkserver和zkclient连接成功
        {
            sem_t *sem = (sem_t *)zoo_get_context(zh);
            sem_post(sem);
        }
    }
}

ZKClient::ZKClient() : m_zhandle(nullptr)
{
}
ZKClient::~ZKClient()
{
    if (m_zhandle != nullptr)
    {
        // 关闭句柄，释放资源
        zookeeper_close(m_zhandle);
    }
}

// 连接zkserver
void ZKClient::Start()
{
    std::string host = MprpcApplication::GetInstance().GetConfig().Load("zookeeperip");
    std::string port = MprpcApplication::GetInstance().GetConfig().Load("zookeeperport");
    std::string connstr = host + ":" + port;

    // zookeeper的API客户端程序提供了三个线程：API调用线程、网络I/O线程 pthread_create poll 、watcher回调线程
    m_zhandle = zookeeper_init(connstr.c_str(), global_wathcher, 30000, nullptr, nullptr, 0);
    if (nullptr == m_zhandle)
    {
        std::cout << "zookeeper_init error!" << std::endl;
        exit(EXIT_FAILURE);
    }
    sem_t sem;
    sem_init(&sem, 0, 0);
    zoo_set_context(m_zhandle, &sem);

    sem_wait(&sem);
    std::cout << "zookeeper_init success!" << std::endl;
}

void ZKClient::Create(const char *path, const char *data, int datalen, int state)
{
    char path_buffer[128];
    int bufferlen = sizeof(path_buffer);
    int flag;
    // 判断znode节点是否存在
    flag = zoo_exists(m_zhandle, path, 0, nullptr);
    // zonde节点不存在
    if (ZNONODE == flag)
    {
        // 创建指定path的zonde节点
        flag = zoo_create(m_zhandle, path, data, datalen,
                          &ZOO_OPEN_ACL_UNSAFE, state, path_buffer, bufferlen);
        if (flag == ZOK)
        {
            std::cout << "znode create sucess..path:" << path << std::endl;
        }
        else
        {
            std::cout << "flag:" << flag << std::endl;
            std::cout << "znode create error...paht:" << path << std::endl;
            exit(EXIT_FAILURE);
        }
    }
}

// 根据指定path获取znode节点的值
std::string ZKClient::GetData(const char *path)
{
    char buffer[64];
    int bufferlen = sizeof(buffer);
    int flag = zoo_get(m_zhandle, path, 0, buffer, &bufferlen, nullptr);

    if (flag != ZOK)
    {
        std::cout << "get znode error ... path:" << path << std::endl;
        return "";
    }
    else
    {
        return buffer;
    }
}
