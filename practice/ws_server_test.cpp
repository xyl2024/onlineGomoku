#include <iostream>
#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp> //不带SSL
// #include <functional>

using std::cout;
using std::endl;


/// 类型重命名
using WsServer = websocketpp::server<websocketpp::config::asio>;

/// 业务回调函数
// http请求
void http_handler(WsServer* svr, websocketpp::connection_hdl hdl)
{
    /// 1.从connection句柄hdl中获得一个connection对象(的指针)
    WsServer::connection_ptr conn = svr->get_con_from_hdl(hdl);

    /// 2.从conn对象中获取request对象，得到请求的信息！
    websocketpp::http::parser::request req = conn->get_request();
    cout << "body: " << conn->get_request_body() << endl;
    cout << "method: " << req.get_method() << endl;
    cout << "url: " << req.get_uri() << endl;

    /// 3.响应一个简单的html页面，直接写入conn对象中即可！
    std::string rspBody = "<html><body><h1>Hello Websocket http</h1></body></html>";
    conn->set_body(rspBody);
    conn->append_header("Content-Type", "text/html");
    conn->set_status(websocketpp::http::status_code::ok);
}
// Websocket相关回调函数
void wsOpen_handler(WsServer* svr, websocketpp::connection_hdl hdl)
{
    cout << "Websocket握手成功\n";
}
void wsClose_handler(WsServer* svr, websocketpp::connection_hdl hdl)
{
    cout << "Websocket断开连接\n";
}
void wsMessage_handler(WsServer* svr, websocketpp::connection_hdl hdl, WsServer::message_ptr msg)
{
    /// 1.来自客户端的消息，在message_ptr中
    cout << "收到一条Websocket消息: \n";
    cout << msg->get_payload() << endl;
    /// 2.发给客户端的消息，直接通过conn对象调用send即可！
    std::string rspBody = "Server Echo : " + msg->get_payload();
    svr->get_con_from_hdl(hdl)->send(rspBody, websocketpp::frame::opcode::text);
}
int main()
{
    /// 1.创建server对象
    WsServer svr;
    /// 2.设置日志等级
    svr.set_access_channels(websocketpp::log::alevel::none); // none 表示不打印任何等级日志
    /// 3.初始化asio框架中的调度器
    svr.init_asio();
    /// 4.设置业务回调函数
    svr.set_http_handler(std::bind(&http_handler, &svr, std::placeholders::_1)); //设置http请求的回调
    svr.set_open_handler(std::bind(&wsOpen_handler, &svr, std::placeholders::_1));
    svr.set_close_handler(std::bind(&wsClose_handler, &svr, std::placeholders::_1));
    svr.set_message_handler(std::bind(&wsMessage_handler, &svr, std::placeholders::_1, std::placeholders::_2));
    /// 5.监听绑定端口、开启获取连接、开启服务器
    svr.listen(8090);
    svr.start_accept();
    svr.run();
    return 0;
}