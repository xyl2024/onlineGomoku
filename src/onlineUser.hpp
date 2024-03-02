#ifndef _ONLINE_USER_HPP_
#define _ONLINE_USER_HPP_
#include "util.hpp"
#include <mutex>
#include <unordered_map>
namespace gomoku
{
class OnlineUser
{
private:
    std::mutex _mtx;
    // 通过用户id，找到大厅里/房间里的用户对应的连接对象
    // key-value = uid-conn
    std::unordered_map<uint64_t, wsserver_t::connection_ptr> _hallUser;
    std::unordered_map<uint64_t, wsserver_t::connection_ptr> _roomUser;

public:
    OnlineUser()
    {
        std::cout <<"OnlineUser()\n";
    }
    // 用户进入大厅
    void EnterHall(uint64_t uid, wsserver_t::connection_ptr &conn)
    {
        std::unique_lock<std::mutex> lock(_mtx);
        _hallUser.insert({uid, conn});
    }
    // 用户进入房间
    void EnterRoom(uint64_t uid, wsserver_t::connection_ptr &conn)
    {
        std::unique_lock<std::mutex> lock(_mtx);
        _roomUser.insert({uid, conn});
    }
    // 用户退出大厅
    void ExitHall(uint64_t uid)
    {
        std::unique_lock<std::mutex> lock(_mtx);
        _hallUser.erase(uid);
    }
    // 用户推出房间
    void ExitRoom(uint64_t uid)
    {
        std::unique_lock<std::mutex> lock(_mtx);
        _roomUser.erase(uid);
    }
    // 用户是否在大厅
    bool InHall(uint64_t uid)
    {
        std::unique_lock<std::mutex> lock(_mtx);
        auto it = _hallUser.find(uid);
        if (it == _hallUser.end())
            return false;
        return true;
    }
    // 用户是否在房间
    bool InRoom(uint64_t uid)
    {
        std::unique_lock<std::mutex> lock(_mtx);
        auto it = _roomUser.find(uid);
        if (it == _roomUser.end())
            return false;
        return true;
    }
    // 获取大厅中用户的连接
    wsserver_t::connection_ptr GetConnFromHall(uint64_t uid)
    {
        std::unique_lock<std::mutex> lock(_mtx);
        auto it = _hallUser.find(uid);
        if (it == _hallUser.end())
            return wsserver_t::connection_ptr();
        return it->second;
    }
    // 获取房间中用户的连接
    wsserver_t::connection_ptr GetConnFromRoom(uint64_t uid)
    {
        std::unique_lock<std::mutex> lock(_mtx);
        auto it = _roomUser.find(uid);
        if (it == _roomUser.end())
            return wsserver_t::connection_ptr();
        return it->second;
    }
};
}
#endif