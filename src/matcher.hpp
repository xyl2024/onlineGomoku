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

#define BRONZE_SCORE 1000
#define SILVER_SCORE 3000
#define GOLD_SCORE 10000

    /*玩家对战匹配管理*/
    class Matcher
    {
    private:
        MatchQueue<uint64_t> _bronzeQueue; // 青铜
        std::thread _bronzeThread;
        MatchQueue<uint64_t> _silverQueue; // 白银
        std::thread _silverThread;
        MatchQueue<uint64_t> _goldQueue; // 黄金
        std::thread _goldThread;
        RoomManager *_rm;
        UserTable *_ut;
        OnlineUser *_ou;

    private:
        void ThreadRunHelper(MatchQueue<uint64_t> &mq)
        {
            // 匹配线程无限循环
            while(true)
            {
                // 1.队列人数小于2则阻塞等待
                while(mq.Size() < 2) //循环判断
                    mq.Wait();
                // 2.出队两个玩家
                uint64_t uid1, uid2;
                bool ret = mq.Pop(uid1);
                if(ret == false) 
                    continue;
                ret = mq.Pop(uid2);
                if(ret == false)
                {
                    Add(uid1);
                    continue;
                }
                // 3.判断两个玩家是否在游戏大厅在线
                wsserver_t::connection_ptr conn1 = _ou->GetConnFromHall(uid1);
                if(conn1.get()==nullptr)
                {
                    Add(uid2);
                    continue;
                }
                wsserver_t::connection_ptr conn2 = _ou->GetConnFromHall(uid2);
                if(conn2.get()==nullptr)
                {
                    Add(uid1);
                    continue;
                }
                // 4.创建房间
                room_ptr rp = _rm->CreateRoomForTwoUser(uid1, uid2);
                if(rp.get() == nullptr)
                {
                    Add(uid1);
                    Add(uid2);
                    continue;
                }
                // 5.发送响应给两个玩家
                Json::Value resp;
                resp["optype"] = "match_success";
                resp["result"] = true;
                std::string body;
                util::json::serialize(resp, body);
                conn1->send(body);
                conn2->send(body);
            }
        }
        /*线程入口函数*/
        void BronzeRun()
        {
            ThreadRunHelper(_bronzeQueue);
        }
        void SilverRun()
        {
            ThreadRunHelper(_silverQueue);
        }
        void GoldRun()
        {
            ThreadRunHelper(_goldQueue);
        }

    public:
        Matcher(RoomManager *rm, UserTable *ut, OnlineUser *ou)
            : _rm(rm), _ut(ut), _ou(ou), _bronzeThread(std::thread(&Matcher::BronzeRun, this)), _silverThread(std::thread(&Matcher::SilverRun, this)), _goldThread(std::thread(&Matcher::GoldRun, this))
        {
            mylog::INFO_LOG("玩家匹配管理模块初始化完成");
        }
        /*添加玩家到对应的阻塞队列中*/
        bool Add(uint64_t uid)
        {
            // 1.获取玩家信息
            Json::Value user;
            bool ret = _ut->SelectById(uid, user);
            if (ret == false)
            {
                mylog::INFO_LOG("获取玩家信息失败，uid: %d", uid);
                return false;
            }
            int score = user["score"].asInt();
            // 2.添加到对应队列
            if (score < BRONZE_SCORE)
                _bronzeQueue.Push(uid);
            else if (score >= BRONZE_SCORE && score < SILVER_SCORE)
                _silverQueue.Push(uid);
            else
                _goldQueue.Push(uid);
            return true;
        }
        /*在对应的阻塞队列中删除玩家*/
        bool Del(uint64_t uid)
        {
            // 1.获取玩家信息
            Json::Value user;
            bool ret = _ut->SelectById(uid, user);
            if (ret == false)
            {
                mylog::INFO_LOG("获取玩家信息失败，uid: %d", uid);
                return false;
            }
            int score = user["score"].asInt();
            // 2.删除对应队列中的数据
            if (score < BRONZE_SCORE)
                _bronzeQueue.Remove(uid);
            else if (score >= BRONZE_SCORE && score < SILVER_SCORE)
                _silverQueue.Remove(uid);
            else
                _goldQueue.Remove(uid);
            return true;
        }
    };
}
#endif