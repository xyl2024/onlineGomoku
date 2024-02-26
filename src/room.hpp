#ifndef _ROOM_HPP_
#define _ROOM_HPP_
#include "database.hpp"
#include "onlineUser.hpp"
#include <mutex>
#include <unordered_map>
namespace gomoku
{
#define BOARD_ROW 15
#define BOARD_COL 15
    /*
        房间类，负责玩家对战胜负的记录、聊天动作的处理等，
        总之就是房间内任何动作都需要广播给所有在房间内的用户。
    */
    class Room
    {
        enum class room_status
        {
            GAME_START,
            GAME_OVER
        };

    private:
        uint64_t _rid;
        room_status _status;
        int _playerCnt;
        uint64_t _whiteUid;
        uint64_t _blackUid;
        UserTable *_userTable;
        OnlineUser *_onlineUser;
        std::vector<std::vector<char>> _board;

    public:
        Room(uint64_t rid, UserTable *ut, OnlineUser *olu)
            : _rid(rid), _status(room_status::GAME_START), _playerCnt(0), _userTable(ut), _onlineUser(olu), _board(BOARD_ROW, std::vector<char>(BOARD_COL, 0))
        {
        }
        uint64_t GetRid()
        {
            return _rid;
        }
        room_status GetStatus()
        {
            return _status;
        }
        int GetPlayerCnt()
        {
            return _playerCnt;
        }
        uint64_t GetWhiteUid()
        {
            return _whiteUid;
        }
        uint64_t GetBlackUid()
        {
            return _blackUid;
        }
        void SetWhiteUid(uint64_t uid)
        {
            _whiteUid = uid;
            _playerCnt++;
        }
        void SetBlackUid(uint64_t uid)
        {
            _blackUid = uid;
            _playerCnt++;
        }
        /// 处理玩家请求
        void HandleRequest(const Json::Value &req)
        {
            // 1.校验房间号是否匹配
            Json::Value rsp;
            // ...
        }

        /// 处理聊天动作
        Json::Value HandleChat(const Json::Value &req)
        {
        }

        /// 处理下棋动作
        Json::Value HandleChess(const Json::Value &req)
        {
        }

        /// 处理玩家退出房间动作
        void HandleExitRoom(uint64_t uid)
        {
        }
        /// 广播给房间内所有玩家
        void Broadcast(const Json::Value &rsp)
        {
        }
    };

    using room_ptr = std::shared_ptr<Room>;
    /*
        房间管理类：负责创建房间、查找房间、销毁房间等与房间相关的工作。
        - 当两个玩家匹配成功，将为他们[创建房间]
        - 通过rid[查找房间]，通过uid[查找房间]
        - 通过rid[销毁房间]
        1. 用户数据管理模块的句柄
        2. 在线用户管理模块的句柄
        3. rid分配器
        4. 保护rid分配器的互斥锁
        5. rid和房间对象句柄的映射关系 --- 通过rid找具体的房间对象
        6. uid和rid之间的映射关系 --- 通过uid找rid再找具体的房间对象
    */
    class RoomManager
    {
    private:
        UserTable *_userTable;
        OnlineUser *_onlineUser;
        uint64_t _nextRid = 1;
        std::mutex _mtx;
        std::unordered_map<uint64_t, room_ptr> _rooms;
        std::unordered_map<uint64_t, uint64_t> _users;

    public:
        RoomManager(UserTable *ut, OnlineUser *olu)
            : _userTable(ut), _onlineUser(olu)
        {
        }
        /// 通过rid找房间对象
        room_ptr GetRoomByRid(uint64_t rid)
        {
            std::unique_lock<std::mutex> lock(_mtx);
            auto it = _rooms.find(rid);
            if (it == _rooms.end())
                return room_ptr();
            return it->second;
        }
        /// 通过uid找房间对象
        room_ptr GetRoomByUid(uint64_t uid)
        {
            std::unique_lock<std::mutex> lock(_mtx);
            // 先找rid
            auto it = _users.find(uid);
            if (it == _users.end())
                return room_ptr();
            uint64_t rid = it->second;
            // 通过rid找房间对象
            auto it2 = _rooms.find(rid);
            if (it2 == _rooms.end())
                return room_ptr();
            return it2->second;
        }
        /// 通过rid销毁房间

        /// 删除房间中指定用户

        /// 给两个匹配成功的用户创建房间
        room_ptr CreateRoomForTwoUser(uint64_t uid1, uint64_t uid2)
        {
            // 1. 判断两个玩家是否都在游戏大厅中
            if (_onlineUser->InHall(uid1) == false)
            {
                mylog::INFO_LOG("用户不在游戏大厅中，uid: %lu", uid1);
                return room_ptr();
            }
            if (_onlineUser->InHall(uid2) == false)
            {
                mylog::INFO_LOG("用户不在游戏大厅中，uid: %lu", uid2);
                return room_ptr();
            }

            // 2. 创建房间并将用户信息添加到房间中
            std::unique_lock<std::mutex> lock(_mtx);
            room_ptr rp(new Room(_nextRid, _userTable, _onlineUser));
            rp->SetWhiteUid(uid1);
            rp->SetBlackUid(uid2);

            // 3. 将房间信息用哈希表管理起来
            _rooms.insert({_nextRid, rp});
            _users.insert({uid1, _nextRid});
            _users.insert({uid2, _nextRid});
            _nextRid++;
            return rp;
        }
    };
}
#endif