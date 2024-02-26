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

    private:
        /*服务器回调函数*/
        void HttpCallback(websocketpp::connection_hdl hdl)
        {
            wsserver_t::connection_ptr conn = _wssvr.get_con_from_hdl(hdl);
            auto req = conn->get_request();
            std::string method = req.get_method();
            std::string uri = req.get_uri();
            std::string pathname = _webRoot + uri;
            std::string body;
            util::file::read(pathname, body);
            conn->set_status(websocketpp::http::status_code::ok);
            conn->append_header("Content-Length", std::to_string(body.size()));

            conn->set_body(body);
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

    public:
        /*成员初始化*/
        GomokuServer(const char *host, uint16_t port, const char *mysql_usr, const char *mysql_pwd, const char *db_name, const std::string &wwwroot = WWWROOT)
            : _ut(host, port, mysql_usr, mysql_pwd, db_name), _rm(&_ut, &_ou), _sm(&_wssvr), _mch(&_rm, &_ut, &_ou), _webRoot(wwwroot)
        {
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
    };

}

#endif