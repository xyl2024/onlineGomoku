#ifndef _SERVER_HPP_
#define _SERVER_HPP_

#include "util.hpp"
#include "database.hpp"
#include "onlineUser.hpp"
#include "session.hpp"
#include "matcher.hpp"
#include "room.hpp"

namespace gomoku
{
#define WWWROOT "./wwwroot/"
    class GomokuServer
    {
    private:
        wsserver_t _wssvr;    // 服务器主体
        UserTable _ut;        // 用户信息表管理
        OnlineUser _ou;       // 在线用户管理
        RoomManager _rm;      // 房间管理
        SessionManager _sm;   // 会话管理
        Matcher _mch;         // 玩家匹配管理
        std::string _webRoot; // 静态资源根目录
    public:
        /*成员初始化*/
        GomokuServer(const char *host, uint16_t port, const char *mysql_usr, const char *mysql_pwd, const char *db_name, const std::string &wwwroot = WWWROOT)
            : _ut(host, port, mysql_usr, mysql_pwd, db_name), _rm(&_ut, &_ou), _sm(&_wssvr), _mch(&_rm, &_ut, &_ou), _webRoot(wwwroot)
        {
            // 初始化websocket服务器设置
            _wssvr.set_access_channels(websocketpp::log::alevel::none);
            _wssvr.init_asio();
            _wssvr.set_reuse_addr(true);
            _wssvr.set_http_handler(std::bind(&GomokuServer::HttpCallback, this, std::placeholders::_1));
            _wssvr.set_open_handler(std::bind(&GomokuServer::WsOpenCallback, this, std::placeholders::_1));
            _wssvr.set_close_handler(std::bind(&GomokuServer::WsCloseCallback, this, std::placeholders::_1));
            _wssvr.set_message_handler(std::bind(&GomokuServer::WsMsgCallback, this, std::placeholders::_1, std::placeholders::_2));
        }
        /*启动服务器*/
        void Start(int port)
        {
            _wssvr.listen(port);
            _wssvr.start_accept();
            _wssvr.run();
        }
    
    private:
        /*websocketpp服务器设置的回调函数*/
        void HttpCallback(websocketpp::connection_hdl hdl)
        {
            // 1.获取连接对象conn
            wsserver_t::connection_ptr conn = _wssvr.get_con_from_hdl(hdl);
            auto req = conn->get_request();
            std::string method = req.get_method();
            std::string uri = req.get_uri();

            // mylog::DEBUG_LOG("获取到新的http请求");
            
            // 2.根据不同请求，调用不同的业务处理函数
            if(method == "POST" && uri == "/reg")
            {
                mylog::DEBUG_LOG("正在处理用户注册请求");
                return RegisteHandler(conn);
            }
            else if(method == "POST" && uri == "/login")
            {
                mylog::DEBUG_LOG("正在处理用户登录请求");
                return LoginHandler(conn);
            }
            else if(method == "GET" && uri == "/info")
            {
                mylog::DEBUG_LOG("正在处理获取用户信息请求");
                return InfoHandler(conn);
            }
            else 
            {
                // mylog::DEBUG_LOG("正在处理静态资源请求");
                return FileHandler(conn); //静态资源请求
            }
        }
        void WsOpenCallback(websocketpp::connection_hdl hdl)
        {
        }
        void WsCloseCallback(websocketpp::connection_hdl hdl)
        {
        }
        void WsMsgCallback(websocketpp::connection_hdl hdl, wsserver_t::message_ptr msg)
        {
        }
    private:
    /*处理HTTP相关业务*/
        /*处理静态资源请求*/
        void FileHandler(wsserver_t::connection_ptr conn)
        {
            //1.组织文件所在的路径
            websocketpp::http::parser::request req = conn->get_request();
            std::string uri = req.get_uri();
            std::string filepath = _webRoot + uri;
            //2.如果该路径是一个目录，+ "login.html"
            if(filepath.back() == '/')
                filepath += "login.html";
            //3.读取文件内容，如果文件不存在则返回404页面
            Json::Value rsp;
            std::string body;
            bool ret = util::file::read(filepath, body);
            if(ret == false)
            {
                std::string not_found_page = _webRoot + "NotFound404.html";
                util::file::read(not_found_page, body);
                conn->set_status(websocketpp::http::status_code::not_found);
                conn->set_body(body);
                return;
            }
            //4.正常响应
            conn->set_status(websocketpp::http::status_code::ok);
            conn->set_body(body);
        }

        void OrganizeHttpResponseJson(wsserver_t::connection_ptr& conn, bool result, websocketpp::http::status_code::value status, const std::string& reason)
        {
            Json::Value rsp;
            rsp["result"] = result;
            rsp["reason"] = reason;
            std::string body;
            util::json::serialize(rsp, body);
            conn->set_status(status);
            conn->set_body(body);
            conn->append_header("Content-Type", "application/json");
        }
        
        /*处理用户注册请求*/
        void RegisteHandler(wsserver_t::connection_ptr conn)
        {
            // 1.获取请求正文
            websocketpp::http::parser::request req = conn->get_request();
            std::string req_body = conn->get_request_body();
            // 2.对json格式的正文进行反序列化，获取用户名&密码
            Json::Value reg_info;
            bool ret = util::json::unserialize(req_body, reg_info);
            if(false == ret)
            {
                mylog::DEBUG_LOG("Json反序列化失败");
                return OrganizeHttpResponseJson(conn, false, websocketpp::http::status_code::bad_request, "请求的Json格式错误");
            }
            // 3.处理用户名/密码未输入的情况
            if(reg_info["username"].isNull() || reg_info["password"].isNull())
            {
                mylog::DEBUG_LOG("未输入用户名/密码");
                return OrganizeHttpResponseJson(conn, false, websocketpp::http::status_code::bad_request, "未输入用户名/密码");
            }
            // 4.将用户名&密码录入数据库
            ret = _ut.AddtUser(reg_info); 
            if(false == ret)
            {
                mylog::DEBUG_LOG("用户名已被占用");
                return OrganizeHttpResponseJson(conn, false, websocketpp::http::status_code::bad_request, "用户名已被占用");
            }
            return OrganizeHttpResponseJson(conn, true, websocketpp::http::status_code::ok, "注册成功");
        }
        /*处理用户登录请求*/
        void LoginHandler(wsserver_t::connection_ptr conn)
        {
            //1.获取请求正文并反序列化
            websocketpp::http::parser::request req = conn->get_request();
            std::string req_body = conn->get_request_body();
            Json::Value login_info;
            bool ret = util::json::unserialize(req_body, login_info);
            if(false == ret)
            {
                mylog::DEBUG_LOG("Json反序列化失败");
                return OrganizeHttpResponseJson(conn, false, websocketpp::http::status_code::bad_request, "请求的Json格式错误");
            }
            //2.与数据库中数据校验
            if(login_info["username"].isNull() || login_info["password"].isNull())
            {
                mylog::DEBUG_LOG("未输入用户名/密码");
                return OrganizeHttpResponseJson(conn, false, websocketpp::http::status_code::bad_request, "未输入用户名/密码");
            }
            ret = _ut.SelectByUsrPwd(login_info);
            if(false == ret)
            {
                mylog::DEBUG_LOG("用户名/密码错误");
                return OrganizeHttpResponseJson(conn, false, websocketpp::http::status_code::bad_request, "用户名/密码错误");
            }
            //3.验证成功，给客户端创建session
            uint64_t uid = login_info["id"].asInt64();
            Session::ptr sp = _sm.CreateSession(uid, LOGIN);
            if(sp.get() == nullptr)
            {
                mylog::DEBUG_LOG("创建会话失败");
                return OrganizeHttpResponseJson(conn, false, websocketpp::http::status_code::internal_server_error, "创建会话失败");
            }
            //设置session过期时间
            _sm.SetSessionTime(sp->GetSid(), SESSION_TIMEOUT);

            //4.设置响应头部Set-Cookie:sid
            std::string sid = "SID=" + std::to_string(sp->GetSid());
            conn->append_header("Set-Cookie", sid);
            return OrganizeHttpResponseJson(conn, true, websocketpp::http::status_code::ok, "登录成功");
        }
        /*处理获取用户信息请求*/
        void InfoHandler(wsserver_t::connection_ptr conn)
        {}
    };

}

#endif