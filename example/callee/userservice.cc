#include <iostream>
#include <string>

#include "user.pb.h"
#include "mprpcapplication.h"
#include "rpcprovider.h"


class UserService : public bsk::UserServiceRpc
{

    bool Login(std::string name, std::string pwd)
    {
        std::cout << "doing local service:login" << std::endl;
        std::cout << "name:" << name << "pwd:" << pwd << std::endl;
        return true;
    }
    bool Register(uint32_t id, std::string name, std::string pwd)
    {
        std::cout << "doing local service:Register" << std::endl;
        std::cout << "id :" << id << "name:" << name << "pwd:" << pwd << std::endl;
        return true;
    }

    // 重写RPC虚函数callee调用
    void Login(::google::protobuf::RpcController *controller,
               const ::bsk::LoginRequest *request,
               ::bsk::LoginResponse *response,
               ::google::protobuf::Closure *done)
    {
        // 接收框架上报传递的请求参数
        std::string name = request->name();
        std::string pwd = request->pwd();

        // 执行本地业务
        bool login_result = Login(name, pwd);

        // 响应写入·
        bsk::ResultCode *code = response->mutable_result();
        code->set_errcode(0);
        code->set_errmsg("");
        response->set_sucess(login_result);

        // 执行回调（响应对象数据的序列化和网络发送）
        done->Run();
    }

    void Register(::google::protobuf::RpcController *controller,
                  const ::bsk::RegisterRequest *request,
                  ::bsk::RegisterResponse *response,
                  ::google::protobuf::Closure *done)
    {
        // 接收框架上报传递的请求参数
        std::string name = request->name();
        std::string pwd = request->pwd();
        std::uint32_t id = request->id();

        // 执行本地业务
        bool register_result = Register(id, name, pwd);

        response->mutable_result()->set_errcode(0);
        response->mutable_result()->set_errmsg("");

        response->set_sucess(register_result);

        // 执行回调（响应对象数据的序列化和网络发送）
        done->Run();
    }
};

int main(int argc, char **argv)
{

    // 调用框架初始化操作
    MprpcApplication::Init(argc, argv);

    // 将Userservice对象发布到rpc节点上
    RpcProvider provider;
    provider.NotifyService(new UserService());

    // 启动一个rpc服务节点 RUN后进入阻塞状态，等待远程rpc调用
    provider.Run();
    return 0;
}