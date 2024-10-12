#include "mprpcchannel.h"
#include "rpcheader.pb.h"
#include "mprpcapplication.h"
#include "mprpccontroller.h"

#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include "zookeeperutil.h"

// header_size + service_name method_name args_sze + args

void MprpcChannel::CallMethod(const google::protobuf::MethodDescriptor *method,
                              google::protobuf::RpcController *controller, const google::protobuf::Message *request,
                              google::protobuf::Message *response, google::protobuf::Closure *done)
{
    const google::protobuf::ServiceDescriptor *sd = method->service();
    std::string service_name = sd->name();
    std::string method_name = method->name();

    // 获取参数的序列化字符串长度 args_size
    uint32_t args_size = 0;
    std::string args_str;
    if (request->SerializeToString(&args_str))
    {
        args_size = args_str.size();
    }
    else
    {

        controller->SetFailed("serialize request error!");

        return;
    }

    // 定义rpc请求header
    mprpc::RpcHeader rpcHeader;
    rpcHeader.set_service_name(service_name);
    rpcHeader.set_method_name(method_name);
    rpcHeader.set_args_size(args_size);

    uint32_t header_size = 0;
    std::string rpc_header_str;
    if (rpcHeader.SerializeToString(&rpc_header_str))
    {
        header_size = rpc_header_str.size();
    }
    else
    {
        controller->SetFailed(" serialize rpc header error!");

        return;
    }

    // 组织将发送的rpc请求的字符串
    std::string send_rpc_str;
    send_rpc_str.insert(0, std::string((char *)&header_size, 4));
    send_rpc_str += rpc_header_str;
    send_rpc_str += args_str;

    // 打印信息
    std::cout << "------------------------" << std::endl;
    std::cout << "header-size : " << header_size << std::endl;
    std::cout << "rpc_header_str : " << rpc_header_str << std::endl;
    std::cout << "service_name : " << service_name << std::endl;
    std::cout << "method_name : " << method_name << std::endl;
    std::cout << "args_str : " << args_str << std::endl;
    std::cout << "------------------------" << std::endl;

    // 使用tcp编程，完成rpc方法远程调用
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == clientfd)
    {

        char errortxt[512] = {0};
        sprintf(errortxt, "create socket error! %d", errno);
        controller->SetFailed(errortxt);
        return;
    }

    // 读取配置文件rpcserver信息
    //  std::string ip = MprpcApplication::GetInstance().GetConfig().Load("rpcserverip");
    //  uint16_t port = atoi(MprpcApplication::GetInstance().GetConfig().Load("rpcserverport").c_str());

    ZKClient zkCli;
    zkCli.Start();
    std::string method_path = "/" + service_name + "/" + method_name;
    std::string host_data = zkCli.GetData(method_path.c_str());
    if (host_data == "")
    {
        controller->SetFailed(method_path + "is not exits!");
        return;
    }
    int idx = host_data.find(":");
    if (idx == -1)
    {
        controller->SetFailed(method_path + "address is invaild!");
        return;
    }
    std::string ip = host_data.substr(0, idx);
    uint16_t port = atoi(host_data.substr(idx + 1, host_data.size() - idx).c_str());

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip.c_str());

    // 链接rpc服务节点
    if (-1 == connect(clientfd, (struct sockaddr *)&server_addr, sizeof(server_addr)))
    {
        char errortxt[512] = {0};
        sprintf(errortxt, "connect error! %d", errno);
        controller->SetFailed(errortxt);

        close(clientfd);
        return;
    }

    // 发送rpc请求
    if (-1 == send(clientfd, send_rpc_str.c_str(), send_rpc_str.size(), 0))
    {
        char errortxt[512] = {0};
        sprintf(errortxt, " send error! %d", errno);
        controller->SetFailed(errortxt);
        close(clientfd);
        return;
    }
    // 接收rpc请求响应值
    char recv_buf[1024] = {0};
    int reecv_size = 0;
    if (-1 == (reecv_size = recv(clientfd, recv_buf, 1024, 0)))
    {
        char errortxt[512] = {0};
        sprintf(errortxt, " recv error! %d", errno);
        controller->SetFailed(errortxt);
        close(clientfd);
        return;
    }
    // 反序列化rpc调用的响应数据
    // recv_buf遇到\0就会停止存数据 从而导致字符串response_str不正确 std::string response_str(recv_buf, 0, reecv_size);
    // if (!response->ParseFromString(response_str))
    if (!response->ParseFromArray(recv_buf, reecv_size))
    {
        std::cout << "parse error : " << recv_buf << std::endl;
        char errortxt[512] = {0};
        sprintf(errortxt, "cparse error : %s", recv_buf);
        controller->SetFailed(errortxt);
        close(clientfd);
        return;
    }
    close(clientfd);
}