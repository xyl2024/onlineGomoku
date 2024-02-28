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
        /*处理http短连接的回调*/
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
                mylog::DEBUG_LOG("正在处理用户注册的HTTP请求");
                return RegisteHandler(conn);
            }
            else if(method == "POST" && uri == "/login")
            {
                mylog::DEBUG_LOG("正在处理用户登录的HTTP请求");
                return LoginHandler(conn);
            }
            else if(method == "GET" && uri == "/info")
            {
                mylog::DEBUG_LOG("正在处理获取用户信息的HTTP请求");
                return InfoHandler(conn);
            }
            else 
            {
                // mylog::DEBUG_LOG("正在处理静态资源请求");
                return FileHandler(conn); //静态资源请求
            }
        }

        /*建立游戏大厅的长连接*/
        void __WsOpenHall(wsserver_t::connection_ptr conn)
        {
            // 1.判断客户端是否登录
            // 1.1根据请求中的Cookie，获取对应的会话(sid)
            Session::ptr sp = __GetSessionByCookie(conn);
            if(sp.get() == nullptr) return;
            // 至此，表示当前用户已登录

            // 2.判断客户端是否重复登录
            if(_ou.InHall(sp->GetUid()) || _ou.InRoom(sp->GetUid()))
            {
                mylog::INFO_LOG("用户重复登录！");
                __OrganizeWebSocketResponseJson(conn, "hall_ready", false, "用户重复登录！");
            }
            // 3.将当前客户加入游戏大厅
            _ou.EnterHall(sp->GetUid(), conn);
            // 4.响应给客户端
            __OrganizeWebSocketResponseJson(conn, "hall_ready", true, "建立游戏大厅长连接成功！");
            // 5.设置Session生效时间为永久
            _sm.SetSessionTime(sp->GetSid(), SESSION_FOREVER);
        }

        /*处理websocket长连接开启的回调*/
        void WsOpenCallback(websocketpp::connection_hdl hdl)
        {
            // 1.根据http资源路径，判断是什么长连接请求
            wsserver_t::connection_ptr conn = _wssvr.get_con_from_hdl(hdl);
            auto req = conn->get_request();
            std::string uri = req.get_uri();
            if(uri == "/hall") //建立游戏大厅的长连接
            {
                __WsOpenHall(conn);
            }
            else if(uri == "/room") //建立游戏房间的长连接
            {

            }

        }

        /*从Cookie中获取Session对象，如果获取不到则意味着用户没有登录(减少重复代码)*/
        Session::ptr __GetSessionByCookie(wsserver_t::connection_ptr conn)
        {
            std::string cookie_str = conn->get_request_header("Cookie");
            if(cookie_str.empty())
            {
                mylog::INFO_LOG("请求中没有Cookie字段，请重新登录");
                __OrganizeWebSocketResponseJson(conn, "hall_ready", false, "请求中没有Cookie字段，请重新登录");
                return Session::ptr();
            }
            std::string sid_str;
            bool ret = __GetCookieValueByKey(cookie_str, "SID", sid_str);
            if(ret == false)
            {
                mylog::INFO_LOG("Cookie字段中没有sid字段");
                __OrganizeWebSocketResponseJson(conn, "hall_ready", false, "Cookie字段中没有sid字段");
                return Session::ptr();
            }
            Session::ptr sp = _sm.GetSessionBySid(std::stol(sid_str));
            if(sp.get() == nullptr)
            {
                mylog::INFO_LOG("无法找到Session对象，请重新登录");
                __OrganizeWebSocketResponseJson(conn, "hall_ready", false, "无法找到Session对象，请重新登录");
                return Session::ptr();
            }
            return sp; //成功返回，此时意味着当前用户正常登录
        }

        /*关闭游戏大厅的长连接*/
        void __WsCloseHall(wsserver_t::connection_ptr conn)
        {
            // 1.将用户移出游戏大厅
            // 1.1判断客户端是否登录
            Session::ptr sp = __GetSessionByCookie(conn);
            if(sp.get() == nullptr) return;

            // 1.2移除玩家
            _ou.ExitHall(sp->GetUid());
            // 2.设置session失效时间
            _sm.SetSessionTime(sp->GetSid(), SESSION_TIMEOUT);
        }

        /*处理websocket长连接断开的回调*/
        void WsCloseCallback(websocketpp::connection_hdl hdl)
        {
            // 1.根据http资源路径，判断是什么长连接请求
            wsserver_t::connection_ptr conn = _wssvr.get_con_from_hdl(hdl);
            auto req = conn->get_request();
            std::string uri = req.get_uri();
            if(uri == "/hall") //建立游戏大厅的长连接
            {
                __WsCloseHall(conn);
            }
            else if(uri == "/room") //建立游戏房间的长连接
            {

            }
        }

        void __WsMsgHall(wsserver_t::connection_ptr conn, wsserver_t::message_ptr msg)
        {
            std::string rsp_str;
            Json::Value rsp_json;
            // 1.身份验证，判断当前用户是否在线，获取用户id
            Session::ptr sp = __GetSessionByCookie(conn);
            if(sp.get() == nullptr) return;
            // 2.获取通信请求信息
            std::string req_str = msg->get_payload();
            Json::Value req_json;
            bool ret = util::json::unserialize(req_str, req_json);
            if(ret == false)
            {
                return __OrganizeWebSocketResponseJson(conn, "json_unserialize_failed", false, "客户端发送的请求Json解析失败");
            }
            // 3.处理通信请求：开始匹配对战、停止匹配对战
            if(!req_json["optype"].isNull() && req_json["optype"].asString() == "match_start")
            {
                _mch.Add(sp->GetUid());
                return __OrganizeWebSocketResponseJson(conn, "match_start", true, "成功添加到匹配队列");
            }
            else if(!req_json["optype"].isNull() && req_json["optype"].asString() == "match_stop")
            {
                _mch.Del(sp->GetUid());
                return __OrganizeWebSocketResponseJson(conn, "match_stop", true, "从匹配队列中移除");
            }
            else
                return __OrganizeWebSocketResponseJson(conn, "unkonw", false, "未知的请求");
        }

        /*处理websocket长连接通信消息的回调*/
        void WsMsgCallback(websocketpp::connection_hdl hdl, wsserver_t::message_ptr msg)
        {
            // 1.根据http资源路径，判断是什么长连接请求
            wsserver_t::connection_ptr conn = _wssvr.get_con_from_hdl(hdl);
            auto req = conn->get_request();
            std::string uri = req.get_uri();
            if(uri == "/hall") //建立游戏大厅的长连接
            {
                __WsMsgHall(conn, msg);
            }
            else if(uri == "/room") //建立游戏房间的长连接
            {

            }
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

        /*组织一个json格式的websocket响应(减少重复代码)*/
        void __OrganizeWebSocketResponseJson(wsserver_t::connection_ptr conn, const std::string& optype, bool result ,const std::string& reason)
        {
            Json::Value rsp;
            rsp["optype"] = optype;
            rsp["result"] = result;
            rsp["reason"] = reason;
            std::string body;
            util::json::serialize(rsp, body);
            conn->send(body);
        }
        /*组织一个json格式的http响应(减少重复代码)*/
        void __OrganizeHttpResponseJson(wsserver_t::connection_ptr& conn, bool result, websocketpp::http::status_code::value status, const std::string& reason)
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
                return __OrganizeHttpResponseJson(conn, false, websocketpp::http::status_code::bad_request, "请求的Json格式错误");
            }
            // 3.处理用户名/密码未输入的情况
            if(reg_info["username"].isNull() || reg_info["password"].isNull())
            {
                mylog::DEBUG_LOG("未输入用户名/密码");
                return __OrganizeHttpResponseJson(conn, false, websocketpp::http::status_code::bad_request, "未输入用户名/密码");
            }
            // 4.将用户名&密码录入数据库
            ret = _ut.AddtUser(reg_info); 
            if(false == ret)
            {
                mylog::DEBUG_LOG("用户名已被占用");
                return __OrganizeHttpResponseJson(conn, false, websocketpp::http::status_code::bad_request, "用户名已被占用");
            }
            return __OrganizeHttpResponseJson(conn, true, websocketpp::http::status_code::ok, "注册成功");
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
                return __OrganizeHttpResponseJson(conn, false, websocketpp::http::status_code::bad_request, "请求的Json格式错误");
            }
            //2.与数据库中数据校验
            if(login_info["username"].isNull() || login_info["password"].isNull())
            {
                mylog::DEBUG_LOG("未输入用户名/密码");
                return __OrganizeHttpResponseJson(conn, false, websocketpp::http::status_code::bad_request, "未输入用户名/密码");
            }
            ret = _ut.SelectByUsrPwd(login_info);
            if(false == ret)
            {
                mylog::DEBUG_LOG("用户名/密码错误");
                return __OrganizeHttpResponseJson(conn, false, websocketpp::http::status_code::bad_request, "用户名/密码错误");
            }
            //3.验证成功，给客户端创建session
            uint64_t uid = login_info["id"].asInt64();
            Session::ptr sp = _sm.CreateSession(uid, LOGIN);
            if(sp.get() == nullptr)
            {
                mylog::DEBUG_LOG("创建会话失败");
                return __OrganizeHttpResponseJson(conn, false, websocketpp::http::status_code::internal_server_error, "创建会话失败");
            }
            //设置session过期时间
            _sm.SetSessionTime(sp->GetSid(), SESSION_TIMEOUT);

            //4.设置响应头部Set-Cookie:sid
            std::string sid = "SID=" + std::to_string(sp->GetSid());
            conn->append_header("Set-Cookie", sid);
            return __OrganizeHttpResponseJson(conn, true, websocketpp::http::status_code::ok, "登录成功");
        }

        /*获取HttpCookie中指定key的value值*/
        bool __GetCookieValueByKey(const std::string& cookie, const std::string& key, std::string& value)
        {
            /*
                GET /sample_page.html HTTP/1.1
                Host: www.example.org
                Cookie: yummy_cookie=choco; tasty_cookie=strawberry
            */
            //1.以; 为间隔分割字符串
            std::string sep = "; ";
            std::vector<std::string> kv;
            util::string::split(cookie, sep, kv);
            //2.对单个字符串，以=分割为key和val
            for(std::string s : kv)
            {
                std::vector<std::string> kv_2;
                util::string::split(s, "=", kv_2);
                if(kv_2.size() != 2) continue;
                if(kv_2[0] == key)
                {
                    value = kv_2[1]; //3.找到key为SID的val值
                    return true;
                }    
            }
            return false;
        }
        
        /*处理获取用户信息请求*/
        void InfoHandler(wsserver_t::connection_ptr conn)
        {
            Json::Value err_info;

            // 1.根据请求中的Cookie，获取对应的会话(sid)
            std::string cookie_str = conn->get_request_header("Cookie");
            if(cookie_str.empty())
            {
                mylog::INFO_LOG("请求中没有Cookie字段");
                return __OrganizeHttpResponseJson(conn, false, websocketpp::http::status_code::bad_request, "请求中没有Cookie字段");
            }
            std::string sid_str;
            bool ret = __GetCookieValueByKey(cookie_str, "SID", sid_str);
            if(ret == false)
            {
                mylog::INFO_LOG("Cookie字段中没有sid字段");
                return __OrganizeHttpResponseJson(conn, false, websocketpp::http::status_code::bad_request, "Cookie字段中没有sid字段");
            }
            Session::ptr sp = _sm.GetSessionBySid(std::stol(sid_str));
            if(sp.get() == nullptr)
            {
                mylog::INFO_LOG("无法找到Session对象，请重新登录");
                return __OrganizeHttpResponseJson(conn, false, websocketpp::http::status_code::bad_request, "无法找到Session对象，请重新登录");
            }
            // 2.会话中有uid，根据uid从数据库中提取用户信息并响应
            uint64_t uid = sp->GetUid();
            Json::Value user_info;
            ret = _ut.SelectById(uid, user_info);
            if(false == ret)
            {
                mylog::INFO_LOG("无法找到用户信息");
                return __OrganizeHttpResponseJson(conn, false, websocketpp::http::status_code::bad_request, "无法找到用户信息");
            }
            // 获取信息成功，组织响应
            std::string body;
            util::json::serialize(user_info, body);
            conn->set_body(body);
            conn->append_header("Content-Type", "application/json");
            conn->set_status(websocketpp::http::status_code::ok);

            // 3.刷新会话过期时间
            _sm.SetSessionTime(sp->GetSid(), SESSION_TIMEOUT);
        }
    };

}

#endif