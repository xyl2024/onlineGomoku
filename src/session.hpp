#ifndef _SESSION_HPP_
#define _SESSION_HPP_
/**
 * 使用cookie和session进行 http短链接用户登录状态管理。
 * 服务器上管理的session都有时限，过期了则对应的session会被删除。
 * 每次用户登录，都会延长session的过期时间。
 */
#include "util.hpp"
#include <unordered_map>
#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>

namespace gomoku
{
#define SESSION_FOREVER -1

    /*会话状态*/
    typedef enum
    {
        LOGOUT,
        LOGIN
    } SessionStatus;

    /*会话*/
    class Session
    {
    private:
        uint64_t _sid;             // session id
        uint64_t _uid;             // user id
        SessionStatus _sstatus;    // session status
        wsserver_t::timer_ptr _tp; // 定时器

    public:
        using ptr = std::shared_ptr<Session>;

        Session(uint64_t sid)
            : _sid(sid)
        {
            mylog::INFO_LOG("创建Session: %p", this);
        }
        ~Session()
        {
            mylog::INFO_LOG("释放Session: %p", this);
        }
        uint64_t GetSid() { return _sid; }
        void SetStatus(SessionStatus status) { _sstatus = status; }
        void SetUid(uint64_t uid) { _uid = uid; }
        uint64_t GetUid() { return _uid; }
        bool IsLogin() { return (_sstatus == LOGIN); }
        void SetTimerPtr(const wsserver_t::timer_ptr &tp) { _tp = tp; }
        wsserver_t::timer_ptr &GetTimerPtr() { return _tp; }
    };

    /*会话管理*/
    class SessionManager
    {
    private:
        uint64_t _nextSid; // sid分配器
        std::mutex _mtx;
        std::unordered_map<uint64_t, Session::ptr> _session; // 通过sid找session对象
        wsserver_t *_server;

    public:
        SessionManager(wsserver_t *server)
            : _nextSid(1), _server(server)
        {
            mylog::INFO_LOG("SessionManager创建成功");
        }
        ~SessionManager()
        {
            mylog::INFO_LOG("SessionManager销毁成功");
        }
        /*创建一个会话对象*/
        Session::ptr CreateSession(uint64_t uid, SessionStatus status)
        {
            std::unique_lock<std::mutex> lock(_mtx);
            Session::ptr s(new Session(_nextSid));
            s->SetStatus(status);
            s->SetUid(uid);
            _session.insert({_nextSid, s});
            _nextSid += 1;
            return s;
        }
        /*添加会话对象到哈希表中*/
        void AppendSession(Session::ptr s)
        {
            std::unique_lock<std::mutex> lock(_mtx);
            _session.insert({s->GetSid(), s});
        }
        /*获取会话对象*/
        Session::ptr GetSessionBySid(uint64_t sid)
        {
            std::unique_lock<std::mutex> lock(_mtx);
            auto it = _session.find(sid);
            if (it == _session.end())
                return Session::ptr();
            return it->second;
        }
        /*销毁会话对象*/
        void RemoveSession(uint64_t sid)
        {
            std::unique_lock<std::mutex> lock(_mtx);
            _session.erase(sid);
        }
        /*设置会话对象存活时间*/
        void SetSessionTime(uint64_t sid, int ms)
        {
            // 使用websocketpp的定时器来管理session的生命周期，
            // 登录之后创建session对象，session需要在指定时间无通信后删除
            // 在大厅或房间，session应该永久存在
            // 退出大厅或房间，session应该被重新设置为临时，在长时间无通信后被删除

            Session::ptr s = GetSessionBySid(sid);
            if (s.get() == nullptr)
                return;
            wsserver_t::timer_ptr tp = s->GetTimerPtr();
            if (tp.get() == nullptr && ms == SESSION_FOREVER)
            {
                // 1. session对象没有设置timer且设置永久存在
                return;
            }
            else if (tp.get() == nullptr && ms != SESSION_FOREVER)
            {
                // 2. session对象没有设置timer且设置指定时间之后被删除的定时任务
                wsserver_t::timer_ptr tmp_tp = _server->set_timer(ms, std::bind(&SessionManager::RemoveSession, this, sid));
                s->SetTimerPtr(tmp_tp);
            }
            else if (tp.get() != nullptr && ms == SESSION_FOREVER)
            {
                // 3. session对象没有设置timer且设置永久存在

                // 删除定时任务--- stready_timer删除定时任务会导致任务直接被执行
                tp->cancel(); // 因为这个取消定时任务并不是立即取消的
                
                // 因此重新给session管理器中，添加一个session信息, 
                // 且添加的时候需要使用定时器，而不是立即添加
                s->SetTimerPtr(wsserver_t::timer_ptr()); // 将session关联的定时器设置为空
                _server->set_timer(0, std::bind(&SessionManager::AppendSession, this, s));
            }
            else if (tp.get() != nullptr && ms != SESSION_FOREVER)
            {
                // 4. ession对象没有设置timer且设置指定时间之后被删除的定时任务
                tp->cancel(); // 这个取消定时任务并不是立即取消的
                s->SetTimerPtr(wsserver_t::timer_ptr());
                _server->set_timer(0, std::bind(&SessionManager::AppendSession, this, s));

                // 重新给session添加定时销毁任务
                wsserver_t::timer_ptr tmp_tp = _server->set_timer(ms, std::bind(&SessionManager::RemoveSession, this, s->GetSid()));
                // 重新设置session关联的定时器
                s->SetTimerPtr(tmp_tp);
            }
        }
    };
}
#endif