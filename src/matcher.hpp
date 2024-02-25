#ifndef __M_MATCHER_H__
#define __M_MATCHER_H__

#include "util.hpp"
#include "onlineUser.hpp"
#include "database.hpp"
#include "room.hpp"
#include <list>
#include <mutex>
#include <condition_variable>
namespace gomoku
{
/*玩家对战匹配使用的阻塞队列。*/
template <class T>
class MatchQueue
{
private:
    std::list<T> _list;
    std::mutex _mtx;
    std::condition_variable _cond; // 阻塞消费线程，当_list.size() < 2时阻塞
public:
    /*获取队列元素个数*/
    int Size()
    {
        std::unique_lock<std::mutex> lock(_mtx);
        return _list.size();
    }
    /*判断是否为空*/
    bool Empty()
    {
        std::unique_lock<std::mutex> lock(_mtx);
        return _list.empty();
    }
    /*阻塞线程*/
    void Wait()
    {
        std::unique_lock<std::mutex> lock(_mtx);
        _cond.wait(lock);
    }
    /*数据入队，唤醒线程*/
    bool Push(const T &data)
    {
        std::unique_lock<std::mutex> lock(_mtx);
        _list.push_back(data);
        // 唤醒线程
        _cond.notify_all();
        return true;
    }
    /*数据出队*/
    bool Pop(T &data)
    {
        std::unique_lock<std::mutex> lock(_mtx);
        if (_list.empty())
            return false;
        data = _list.front();
        _list.pop_front();
        return true;
    }
    /*删除指定数据*/
    bool Remove(T &data)
    {
        std::unique_lock<std::mutex> lock(_mtx);
        _list.remove(data);
        return true;
    }
};

class Matcher
{
private:
    MatchQueue<uint64_t> _bronzeQueue; // 青铜
    std::thread _bronzeThread;
    MatchQueue<uint64_t> _silverQueue; // 白银
    std::thread _silverThread;
    MatchQueue<uint64_t> _goldQueue;   // 黄金
    std::thread _goldThread;
    RoomManager* _rm;
    UserTable* _ut;
    OnlineUser* _ou;
private:
    /*线程入口函数*/
    void BronzeRun();
    void SilverRun();
    void GoldRun();
public:
    Matcher(RoomManager* rm, UserTable* ut, OnlineUser* ou);
    ~Matcher();
    bool Add(uint64_t uid);
    bool Del(uint64_t uid);
    
};
}
#endif