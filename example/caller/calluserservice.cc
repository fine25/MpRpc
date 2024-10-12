#include <iostream>

#include "user.pb.h"
#include "mprpcapplication.h"


int main(int argc,char** argv)
{
    //初始化函数
    MprpcApplication::Init(argc,argv);


    //调用发布rpcLogin方法
    bsk::UserServiceRpc_Stub stub(new MprpcChannel());
    //rpc方法请求参数
    bsk::LoginRequest request;
    request.set_name("zhang san");
    request.set_pwd("123456");
    //rpc方法响应
    bsk::LoginResponse response;
    //以同步的方式发起rpc方法调用 RpcChannel--callMethod方法处理
   MprpcController contorller;

    stub.Login(&contorller, &request, &response, nullptr);
    
    if(contorller.Failed())
    {
        std::cout<<contorller.ErrorText()<<std::endl;
    }
    else
    {
 //一次rpc调用完成，读取结果
    if(0 == response.result().errcode())
    {
        std::cout<<"rpc login response sucess:"<<response.sucess() <<std::endl;
    }
    else
    {
        std::cout<<"rpc login response error:"<<response.result().errmsg() <<std::endl;
    }
    }
   

    //调用发布rpcRegister
    bsk::RegisterRequest request_reg;
    request_reg.set_id(123);
    request_reg.set_name("jie jie");
    request_reg.set_pwd("2538");
    bsk::RegisterResponse response_reg;
    stub.Register(nullptr,&request_reg,&response_reg,nullptr);
    //一次rpc调用完成，读取结果
    if(0 == response_reg.result().errcode())
    {
        std::cout<<"rpc register response sucess:"<<response_reg.sucess() <<std::endl;
    }
    else
    {
        std::cout<<"rpc register response error:"<<response_reg.result().errmsg() <<std::endl;
    }


    return 0;
}