#include "rpcprovider.h"
#include "mprpcapplication.h"
#include "rpcheader.pb.h"
#include "logger.h"
#include "zookeeperutil.h"


// 发布rpc方法,框架为外部提供
void RpcProvider::NotifyService(google::protobuf::Service *service)
{
    ServiceInfo service_info;

    // 获取服务对象描述信息
    const google::protobuf::ServiceDescriptor *pserviceDesc = service->GetDescriptor();
    // 获取服务名
    std::string service_name = pserviceDesc->name();
    // 获取服务对象service的方法数量
    int methodCnt = pserviceDesc->method_count();
   
    for (int i = 0; i < methodCnt; i++)
    {
        // 获取服务对象指定下标的服务方法的描述
        const google::protobuf::MethodDescriptor *pmethodDesc = pserviceDesc->method(i);
        std::string method_name = pmethodDesc->name();
        service_info.m_methodMap.insert({method_name, pmethodDesc});
    }

    service_info.m_service = service;
    m_serviceMap.insert({service_name, service_info});
}
// 启动rpc服务节点，提供rpc远程网络调用服务
void RpcProvider::Run()
{
    std::string ip = MprpcApplication::GetInstance().GetConfig().Load("rpcserverip");
    uint16_t port = atoi(MprpcApplication::GetInstance().GetConfig().Load("rpcserverport").c_str());
    muduo::net::InetAddress address(ip, port);

    // 创建Tcpserver对象
    muduo::net::TcpServer server(&m_eventLoop, address, "RpcProvider");

    // 绑定连接回调和消息读写回调方法、分离网络业务代码
    server.setConnectionCallback(std::bind(&RpcProvider::onConnection, this, std::placeholders::_1));
    server.setMessageCallback(std::bind(&RpcProvider::onMessage, this, std::placeholders::_1,
                                        std::placeholders::_2, std::placeholders::_3));

    server.setThreadNum(4);

    //把当前rpc节点上要发部的服务全部注册到zk上面
    ZKClient zkCli;
    zkCli.Start();

    //service_name为永久性节点 method_name为临时性节点
    for(auto &sp : m_serviceMap)
    {
        std::string service_path ="/" + sp.first;
        zkCli.Create(service_path.c_str(),nullptr,0);
        for(auto &tmp : sp.second.m_methodMap)
        {
            std::string method_path = service_path  +"/"+ tmp.first;
            char method_path_data[128]={0};
            sprintf(method_path_data,"%s:%d",ip.c_str(),port);
            zkCli.Create(method_path.c_str(),method_path_data,strlen(method_path_data),ZOO_EPHEMERAL);
        }
    }




    std::cout << "RpcProvider start service at ip:" << ip << "port:" << port << std::endl;

    // 启动网络服务
    server.start();
    m_eventLoop.loop();
}

void RpcProvider::onConnection(const muduo::net::TcpConnectionPtr &conn)
{
    if (!conn->connected())
    {
        conn->shutdown();
    }
}

// 一个远程rpc服务调用请求，此方法就会响应
void RpcProvider::onMessage(const muduo::net::TcpConnectionPtr &conn, muduo::net::Buffer *buffer, muduo::Timestamp)
{
    // 网络上接收远程rpc调用请求的字符流
    std::string recv_buf = buffer->retrieveAllAsString();

    // 从字节流中读取4个字节
    uint32_t header_size = 0;
    recv_buf.copy((char *)&header_size, 4, 0);

    // 读取原始字符流，反序列化数据 得到详细信息
    std::string rpc_header_str = recv_buf.substr(4, header_size);

    mprpc::RpcHeader rpcHeader;
    std::string service_name;
    std::string method_name;
    uint32_t args_size;

    if (rpcHeader.ParseFromString(rpc_header_str))
    {
        // 数据反序列化成功
        service_name = rpcHeader.service_name();
        method_name = rpcHeader.method_name();
        args_size = rpcHeader.args_size();
    }
    else
    {
        std::cout << "rpc_header_str:" << rpc_header_str << "error parse" << std::endl;
        return;
    }

    // 获取rpc方法参数字符流数据
    std::string args_str = recv_buf.substr(4 + header_size, args_size);

    // 打印信息
    std::cout << "------------------------" << std::endl;
    std::cout << "header-size : " << header_size << std::endl;
    std::cout << "rpc_header_str : " << rpc_header_str << std::endl;
    std::cout << "service_name : " << service_name << std::endl;
    std::cout << "method_name : " << method_name << std::endl;
    std::cout << "args_str : " << args_str << std::endl;
    std::cout << "------------------------" << std::endl;

    //s获取service对象和method对象
    auto it = m_serviceMap.find(service_name);
    if(it == m_serviceMap.end())
    {
        std::cout<< service_name <<" is not exist"<< std::endl;
        return ;
    }
    auto mit = it->second.m_methodMap.find(method_name);
    if(mit == it->second.m_methodMap.end())
    {
        std::cout<<service_name <<" : "<<method_name <<" is not exist"<<std::endl;
        return ;
    }
    //获取service对象 method对象
    google::protobuf::Service* service = it->second.m_service;
    const google::protobuf::MethodDescriptor *method = mit->second;

    //生成rpc方法调用请求request和响应response参数
    google::protobuf::Message* request = service->GetRequestPrototype(method).New();
    if(!request->ParseFromString(args_str))
    {
        std::cout<<"request parse error ,content: "<<args_str<<std::endl;
        return ;
    }
    google::protobuf::Message* response = service->GetResponsePrototype(method).New(0);

    //给method方法调用，绑定Clouser回调函数
    
    google::protobuf::Closure* done = google::protobuf::NewCallback<RpcProvider,
                                const muduo::net::TcpConnectionPtr&,
                                google::protobuf::Message*>(this,&RpcProvider::SendRpcResponse,conn,response);
  

    //在框架上根据rpc请求，调用当前rpc节点上发布的方法
    service->CallMethod(method,nullptr,request,response,done);

}

void RpcProvider::SendRpcResponse(const muduo::net::TcpConnectionPtr &conn, google::protobuf::Message *response)
{
    std::string response_str;
    if(response->SerializeToString(&response_str))
    {
        //成功序列化，通过网络把rpc方法执行的结果发送到rpc调用方
        conn->send(response_str);
    }
    else
    {
        std::cout<<"seralize response_str error"<<std::endl;
    }
    //模拟http的短链接服务，由rpcprovider主动断开连接
    conn->shutdown();
}