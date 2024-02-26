#include "server.hpp"

namespace gomoku
{
#define HOST "39.108.170.136"
#define PORT 30306
#define USER "root"
#define PASS "xiaoyuanli123@"
#define DBNAME "gomoku"

    using std::cout;
    using std::endl;

    void test_mysql_util()
    {
        MYSQL *mysql = util::mysql::create(HOST, PORT, USER, PASS, DBNAME);
        const char *sql = "insert stu values(null, 18, '李瑟');";
        if (!util::mysql::exec(mysql, sql))
        {
            mylog::ERROR_LOG("出错了");
        }
        util::mysql::destroy(mysql);
    }

    void test_json_util()
    {
        Json::Value root;
        std::string body;
        root["姓名"] = "小明";
        root["年龄"] = 18;
        root["成绩"].append(98);
        root["成绩"].append(88.5);
        root["成绩"].append(78.5);
        util::json::serialize(root, body);
        // std::cout << body.c_str() << std::endl;
        mylog::INFO_LOG("%s", body.c_str());

        Json::Value val;
        util::json::unserialize(body, val);
        std::cout << "姓名:" << val["姓名"].asString() << std::endl;
        std::cout << "年龄:" << val["年龄"].asInt() << std::endl;
        int sz = val["成绩"].size();
        for (int i = 0; i < sz; i++)
        {
            std::cout << "成绩: " << val["成绩"][i].asFloat() << std::endl;
        }
    }

    void test_database_usertable()
    {
        UserTable utb(HOST, PORT, USER, PASS, DBNAME);
        Json::Value usr;
        // /// 1.创建用户stephen
        // usr["username"] = "stephen";
        // usr["password"] = "asfagas";
        // if (utb.AddtUser(usr) == false)
        // {
        //     std::cout << "出错了\n";
        // }
        /// 2.根据用户名获取信息
        utb.SelectByName("stephen", usr);
        std::string s;
        util::json::serialize(usr, s);
        std::cout << s << std::endl;
        /// 3.赢了
        // utb.Win(1);
        /// 4.输了
        utb.Lose(1);
        utb.Lose(1);
        utb.Lose(1);
    }

    void test_onlineUser()
    {
        OnlineUser ol;
        wsserver_t::connection_ptr conn;
        uint64_t uid = 2;
        ol.EnterRoom(uid, conn);
        if (ol.InRoom(uid))
        {
            cout << "uid=2 is in hall.\n";
        }
        else
        {
            cout << "uid=2 is not in hall.\n";
        }
        ol.ExitRoom(uid);
        if (ol.InRoom(uid))
        {
            cout << "uid=2 is in hall.\n";
        }
        else
        {
            cout << "uid=2 is not in hall.\n";
        }
    }

}
int main()
{
    gomoku::GomokuServer svr(HOST, PORT, USER, PASS, DBNAME);
    svr.Start(8888);
    return 0;
}