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
#define WHITE_CHESS 'o'
#define BLACK_CHESS '*'
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
    uint64_t _rid;                         // 房间id
    int _playerCnt;                        // 玩家人数
    uint64_t _whiteUid;                    // 白棋玩家id
    uint64_t _blackUid;                    // 黑棋玩家id
    room_status _status;                   // 房间状态
    UserTable *_ut;                        // 用户表管理模块
    OnlineUser *_ou;                       // 在线用户管理模块
    std::vector<std::vector<char>> _board; // 棋盘

public:
    Room(uint64_t rid, UserTable *ut, OnlineUser *ou)
        : _rid(rid), _status(room_status::GAME_START), _playerCnt(0), _ut(ut), _ou(ou), _board(BOARD_ROW, std::vector<char>(BOARD_COL, 0))
    {
        mylog::INFO_LOG("创建房间成功，rid = %lu", _rid);
    }
    ~Room()
    {
        mylog::INFO_LOG("销毁房间成功，rid = %lu", _rid);
    }
    /*总地处理玩家的请求*/
    void HandleRequest(const Json::Value &req)
    {
        // 1.判断房间号是否一致
        Json::Value rsp;
        uint64_t rid = req["room_id"].asUInt64();
        if(rid != _rid)
        {
            rsp["optype"] = req["optype"].asString();
            rsp["result"] = false;
            rsp["reason"] = "房间号不一致";
            Broadcast(rsp);
            return;
        }
        mylog::INFO_LOG("房间号一致");
        // 2.根据不同请求调用不同的处理函数，最后广播出去
        if(req["optype"].asString() == "put_chess") //处理下棋
        {
            rsp = HandleChess(req);

            // 有winner，更新数据库数据
            if(rsp["winner"].asUInt64() != 0)
            {
                uint64_t winner = rsp["winner"].asUInt64();
                uint64_t loser = (winner == _whiteUid) ? _blackUid : _whiteUid;
                _ut->Win(winner);
                _ut->Lose(loser);
                _status = room_status::GAME_OVER;
            }
            mylog::INFO_LOG("下棋请求处理完毕");
        }
        else if(req["optype"].asString() == "chat")
        {
            rsp = HandleChat(req);
            mylog::INFO_LOG("聊天请求处理完毕");
        }
        else
        {
            rsp["optype"] = req["optype"].asString();
            rsp["result"] = false;
            rsp["reason"] = "未知的请求";
            return Broadcast(rsp);
        }
        // 3.把响应广播给所有玩家
        std::string rsp_str;
        util::json::serialize(rsp, rsp_str);
        // mylog::INFO_LOG("给房间中的用户广播信息：%s", rsp_str.c_str());
        Broadcast(rsp);
    }
    /*处理玩家退出房间动作*/
    void HandleExitRoom(uint64_t uid)
    {
        // 1.若下棋过程中玩家退出，则另一玩家获胜
        // 若下棋结束后退出，则正常退出
        Json::Value rsp;
        if(_status == room_status::GAME_START)
        {
            uint64_t winnerid = (uid == _whiteUid ? _blackUid : _whiteUid);
            uint64_t loserid = (winnerid == _whiteUid ? _blackUid : _whiteUid);
            rsp["optype"] = "put_chess";
            rsp["result"] = true;
            rsp["reason"] = "对方掉线，你胜利了";
            rsp["room_id"] = _rid;
            rsp["uid"] = uid;
            rsp["row"] = -1;
            rsp["col"] = -1;
            rsp["winner"] = winnerid;
            // 更新数据库用户信息
            _ut->Win(winnerid);
            _ut->Lose(loserid);
            _status = room_status::GAME_OVER; 

        }
        Broadcast(rsp);

        // 2.房间玩家数量-1
        _playerCnt -= 1;
    }
private:
    /*处理下棋动作*/
    Json::Value HandleChess(const Json::Value &req)
    {
        /*
            待优化，减少重复代码。。
        */
        Json::Value rsp = req;
        
        // 2.判断玩家是否在线，有人不在线，则另一个人获胜
        int req_uid = req["uid"].asInt64();
        int row = req["row"].asInt();
        int col = req["col"].asInt();
        if(_ou->InRoom(_whiteUid) == false)
        {
            rsp["result"] = true;
            rsp["reason"] = "对方掉线，你胜利了";
            rsp["winner"] = _blackUid;
            return rsp;
        }
        if(_ou->InRoom(_blackUid) == false)
        {
            rsp["result"] = true;
            rsp["reason"] = "对方掉线，你胜利了";
            rsp["winner"] = _whiteUid;
            return rsp;
        }
        // 3.判断下棋位置是否合理，合理则下棋
        if(_board[row][col] != 0)
        {
            rsp["result"] = false;
            rsp["reason"] = "当前位置已经有棋子了";
            return rsp;
        }
        // 下棋
        char req_color = (req_uid == _whiteUid) ? WHITE_CHESS : BLACK_CHESS;
        _board[row][col] = req_color;

        // 4.判断下完棋后，是否有人胜利(五子连珠)
        uint64_t winner = __win(row, col, req_color);

        rsp["result"] = true;
        if(winner)
            rsp["reason"] = "五子连珠，你赢啦！";
        else
            rsp["reason"] = "继续下棋";
        rsp["winner"] = winner;
        return rsp;
    }
    /*处理聊天动作*/
    Json::Value HandleChat(const Json::Value &req)
    {
        Json::Value rsp = req;
        rsp["result"] = true;
        return rsp;
    }
    
    /*广播给房间内所有玩家*/
    void Broadcast(const Json::Value &rsp)
    {
        // 1.序列化json格式的rsp为字符串
        std::string body;
        util::json::serialize(rsp, body);
        // 2.获取房间所有用户的连接并发送
        wsserver_t::connection_ptr conn1 = _ou->GetConnFromRoom(_whiteUid);
        wsserver_t::connection_ptr conn2 =_ou->GetConnFromRoom(_blackUid);
        if(conn1.get())
            conn1->send(body);
        else
            mylog::INFO_LOG("白棋玩家获取连接失败");
        if(conn2.get())
            conn2->send(body);
        else
            mylog::INFO_LOG("黑棋玩家获取连接失败");
    }
private:
    /*判断是否有五子连珠*/
    bool __fiveChess(int row, int col, int row_offset, int col_offset, char color)
    {
        int cnt = 1;
        for(int i = 0; i < 2; ++i) //判断两个方向
        {
            int r = row + row_offset;
            int c = col + col_offset;
            while(r >= 0 && r < BOARD_ROW && c >= 0 && c < BOARD_COL
                    && _board[r][c] == color)
            {
                ++cnt;
                r += row_offset;
                c += col_offset;
            }
            row_offset *= -1;
            col_offset *= -1;
        }
        if(cnt >= 5) return true;
        else return false;
    }
    
    /*从(row,col)位置开始往8个方向扫描，判断是否有胜利者*/
    uint64_t __win(int row, int col, char color)
    {
        if(
            __fiveChess(row, col, 0, 1, color) || //行不变，列++/--，判断行
            __fiveChess(row, col, 1, 0, color) || //列不变，行++/--，判断列
            __fiveChess(row, col, -1, 1, color) || //判断两个斜线
            __fiveChess(row, col, -1, -1, color) 
        )
        {
            return color == WHITE_CHESS ? _whiteUid : _blackUid;
        }
        return 0; //返回0表示没有人获胜
    }

public:
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
        std::cout << "RoomManager模块初始化完成\n";
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
    void DestroyRoom(uint64_t rid)
    {
        // 移除用户管理和房间管理哈希表中的数据
        room_ptr rp = GetRoomByRid(rid);
        if(rp.get() == nullptr)
            return;
        uint64_t uid1 = rp->GetBlackUid();
        uint64_t uid2 = rp->GetWhiteUid();
        std::unique_lock<std::mutex> lock(_mtx);
        _users.erase(uid1);
        _users.erase(uid2);
        _rooms.erase(rid);
        
    }

    /// 删除房间中指定用户
    void RemoveUser(uint64_t uid)
    {
        room_ptr rp = GetRoomByUid(uid);
        if (rp.get() == nullptr)
            return;
        rp->HandleExitRoom(uid);
        if (rp->GetPlayerCnt() == 0)
        {
            DestroyRoom(rp->GetRid());
        }
    }

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