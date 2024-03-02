#ifndef _DATABASE_HPP_
#define _DATABASE_HPP_

#include "util.hpp"
#include <mutex>
namespace gomoku
{
    /// @brief 对于MySQL用户表的操作
    class UserTable
    {
    private:
        MYSQL *_mysql;
        std::mutex _mtx;

    public:
        // 主机、端口、MySQL用户名、MySQL密码、数据库名
        UserTable(const char *host, uint16_t port, const char *mysql_usr, const char *mysql_pwd, const char *db_name)
        {
            std::cout << "Connecting to mysql...\n";
            _mysql = util::mysql::create(host, port, mysql_usr, mysql_pwd, db_name);
            assert(_mysql != NULL);
            std::cout << "UserTable模块初始化完毕";
        }
        /// 用户注册，新增一个user
        /// 以Json::Value对象的方式传入
        bool AddtUser(const Json::Value &usr)
        {
            if (usr["password"].isNull() || usr["username"].isNull())
            {
                mylog::ERROR_LOG("INPUT PASSWORD OR USERNAME");
                return false;
            }
#define INSERT_USER "insert user values(null, '%s', password('%s'), 1000, 0, 0);"
            char sql[4096] = {0};
            sprintf(sql, INSERT_USER, usr["username"].asCString(), usr["password"].asCString());
            bool ret = util::mysql::exec(_mysql, sql);
            if (ret == false)
            {
                mylog::ERROR_LOG("insert user info failed!!\n");
                return false;
            }
            return true;
        }
        /// 注销用户，删除一个user
        // bool EraseUser(uint64_t id);

        /// user登陆时，
        /// 根据用户名+密码，获取user详细信息
        bool SelectByUsrPwd(Json::Value &usr)
        {
            if (usr["password"].isNull() || usr["username"].isNull())
            {
                mylog::ERROR_LOG("INPUT PASSWORD OR USERNAME");
                return false;
            }
#define LOGIN_USER "select id, score, pk_cnt, win_cnt from user where username='%s' and password=password('%s');"
            char sql[4096] = {0};
            sprintf(sql, LOGIN_USER, usr["username"].asCString(), usr["password"].asCString());
            MYSQL_RES *res = NULL;
            {
                std::unique_lock<std::mutex> lock(_mtx);
                bool ret = util::mysql::exec(_mysql, sql);
                if (ret == false)
                {
                    mylog::ERROR_LOG("user login failed!!\n");
                    return false;
                }
                // 按理说要么有数据，要么没有数据，就算有数据也只能有一条数据
                res = mysql_store_result(_mysql);
                if (res == NULL)
                {
                    mylog::ERROR_LOG("have no login user info!!");
                    return false;
                }
            }
            int row_num = mysql_num_rows(res);
            if (row_num != 1)
            {
                mylog::ERROR_LOG("the user information queried is not unique!!");
                return false;
            }
            MYSQL_ROW row = mysql_fetch_row(res);
            usr["id"] = (Json::UInt64)std::stol(row[0]);
            usr["score"] = (Json::UInt64)std::stol(row[1]);
            usr["pk_cnt"] = std::stoi(row[2]);
            usr["win_cnt"] = std::stoi(row[3]);
            mysql_free_result(res);
            return true;
        }
        /// 根据id，获取user详细信息
        bool SelectById(uint64_t id, Json::Value &user)
        {
#define USER_BY_ID "select username, score, pk_cnt, win_cnt from user where id=%ld;"
            char sql[4096] = {0};
            sprintf(sql, USER_BY_ID, id);
            MYSQL_RES *res = NULL;
            {
                std::unique_lock<std::mutex> lock(_mtx);
                bool ret = util::mysql::exec(_mysql, sql);
                if (ret == false)
                {
                    mylog::ERROR_LOG("get user by id failed!!\n");
                    return false;
                }
                // 按理说要么有数据，要么没有数据，就算有数据也只能有一条数据
                res = mysql_store_result(_mysql);
                if (res == NULL)
                {
                    mylog::ERROR_LOG("have no user info!!");
                    return false;
                }
            }
            int row_num = mysql_num_rows(res);
            if (row_num != 1)
            {
                mylog::ERROR_LOG("the user information queried is not unique!!");
                return false;
            }
            MYSQL_ROW row = mysql_fetch_row(res);
            user["id"] = (Json::UInt64)id;
            user["username"] = row[0];
            user["score"] = (Json::UInt64)std::stol(row[1]);
            user["pk_cnt"] = std::stoi(row[2]);
            user["win_cnt"] = std::stoi(row[3]);
            mysql_free_result(res);
            return true;
        }

        /// 根据用户名，获取user详细信息
        bool SelectByName(const std::string &name, Json::Value &user)
        {
#define USER_BY_NAME "select id, score, pk_cnt, win_cnt from user where username='%s';"
            char sql[4096] = {0};
            sprintf(sql, USER_BY_NAME, name.c_str());
            MYSQL_RES *res = NULL;
            {
                std::unique_lock<std::mutex> lock(_mtx);
                bool ret = util::mysql::exec(_mysql, sql);
                if (ret == false)
                {
                    mylog::ERROR_LOG("get user by name failed!!\n");
                    return false;
                }
                // 按理说要么有数据，要么没有数据，就算有数据也只能有一条数据
                res = mysql_store_result(_mysql);
                if (res == NULL)
                {
                    mylog::ERROR_LOG("have no user info!!");
                    return false;
                }
            }
            int row_num = mysql_num_rows(res);
            if (row_num != 1)
            {
                mylog::ERROR_LOG("the user information queried is not unique!!");
                return false;
            }
            MYSQL_ROW row = mysql_fetch_row(res);
            user["id"] = (Json::UInt64)std::stol(row[0]);
            user["username"] = name;
            user["score"] = (Json::UInt64)std::stol(row[1]);
            user["pk_cnt"] = std::stoi(row[2]);
            user["win_cnt"] = std::stoi(row[3]);
            mysql_free_result(res);
            return true;
        }
        /// 某个user赢了，修改他的分数和比赛场次
        bool Win(uint64_t id)
        {
#define USER_WIN "update user set score=score+30, pk_cnt=pk_cnt+1, win_cnt=win_cnt+1 where id=%ld;"
            char sql[4096] = {0};
            sprintf(sql, USER_WIN, id);
            bool ret = util::mysql::exec(_mysql, sql);
            if (ret == false)
            {
                mylog::ERROR_LOG("update win user info failed!!\n");
                return false;
            }
            return true;
        }
        /// 某个user输了，修改他的分数和比赛场次
        bool Lose(uint64_t id)
        {
#define USER_LOSE "update user set score=score-30, pk_cnt=pk_cnt+1 where id=%ld;"
            char sql[4096] = {0};
            sprintf(sql, USER_LOSE, id);
            bool ret = util::mysql::exec(_mysql, sql);
            if (ret == false)
            {
                mylog::ERROR_LOG("update lose user info failed!!\n");
                return false;
            }
            return true;
        }
    };
}

#endif