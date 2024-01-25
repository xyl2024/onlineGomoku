#ifndef __M_UTIL_H__
#define __M_UTIL_H__
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <memory>
#include <vector>
#include <cstdint>
#include <jsoncpp/json/json.h>
#include <mysql/mysql.h>
#include "../mylog/mylog.h"

#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>

typedef websocketpp::server<websocketpp::config::asio> wsserver_t;
namespace gomoku
{
    namespace util
    {

        class mysql
        {
        public:
            /// 创建一个mysql句柄，并做一些初始化工作
            static MYSQL *create(const std::string &host,
                                 uint16_t port,
                                 const std::string &mysql_usr,
                                 const std::string &mysql_pwd,
                                 const std::string &db_name)
            {
                MYSQL *mysql = mysql_init(NULL);
                if (mysql == NULL)
                {
                    mylog::ERROR_LOG("mysql init failed!");
                    return NULL;
                }
                // 2. 连接服务器
                if (mysql_real_connect(mysql,
                                       host.c_str(),
                                       mysql_usr.c_str(),
                                       mysql_pwd.c_str(),
                                       db_name.c_str(), port, NULL, 0) == NULL)
                {
                    mylog::ERROR_LOG("connect mysql server failed : %s", mysql_error(mysql));
                    mysql_close(mysql);
                    return NULL;
                }
                // 3. 设置客户端字符集
                if (mysql_set_character_set(mysql, "utf8") != 0)
                {
                    mylog::ERROR_LOG("set client character failed : %s", mysql_error(mysql));
                    mysql_close(mysql);
                    return NULL;
                }
                return mysql;
            }
            /// 执行sql语句
            static bool exec(MYSQL *mysql, const std::string &sql)
            {
                int ret = mysql_query(mysql, sql.c_str());
                if (ret != 0)
                {
                    mylog::ERROR_LOG("%s\n", sql.c_str());
                    mylog::ERROR_LOG("mysql query failed : %s\n", mysql_error(mysql));
                    return false;
                }
                return true;
            }
            /// 销毁mysql句柄
            static void destroy(MYSQL *mysql)
            {
                if (mysql != NULL)
                    mysql_close(mysql);
                return;
            }
        };

        class json
        {
        public:
            static bool serialize(const Json::Value &root, std::string &str)
            {
                Json::StreamWriterBuilder swb;
                std::unique_ptr<Json::StreamWriter> sw(swb.newStreamWriter());
                std::stringstream ss;
                int ret = sw->write(root, &ss);
                if (ret != 0)
                {
                    mylog::ERROR_LOG("json serialize failed!!");
                    return false;
                }
                str = ss.str();
                return true;
            }
            static bool unserialize(const std::string &str, Json::Value &root)
            {
                Json::CharReaderBuilder crb;
                std::unique_ptr<Json::CharReader> cr(crb.newCharReader());
                std::string err;
                bool ret = cr->parse(str.c_str(), str.c_str() + str.size(), &root, &err);
                if (ret == false)
                {
                    mylog::ERROR_LOG("json unserialize failed: %s", err.c_str());
                    return false;
                }
                return true;
            }
        };
        class string
        {
        public:
            static int split(const std::string &src, const std::string &sep, std::vector<std::string> &res)
            {
                // 123,234,,,,345
                size_t pos, idx = 0;
                while (idx < src.size())
                {
                    pos = src.find(sep, idx);
                    if (pos == std::string::npos)
                    {
                        // 没有找到,字符串中没有间隔字符了，则跳出循环
                        res.push_back(src.substr(idx));
                        break;
                    }
                    if (pos == idx)
                    {
                        idx += sep.size();
                        continue;
                    }
                    res.push_back(src.substr(idx, pos - idx));
                    idx = pos + sep.size();
                }
                return res.size();
            }
        };
        class file
        {
        public:
            static bool read(const std::string &filename, std::string &body)
            {
                // 打开文件
                std::ifstream ifs(filename, std::ios::binary);
                if (ifs.is_open() == false)
                {
                    mylog::ERROR_LOG("%s file open failed!!", filename.c_str());
                    return false;
                }
                // 获取文件大小
                size_t fsize = 0;
                ifs.seekg(0, std::ios::end);
                fsize = ifs.tellg();
                ifs.seekg(0, std::ios::beg);
                body.resize(fsize);
                // 将文件所有数据读取出来
                ifs.read(&body[0], fsize);
                if (ifs.good() == false)
                {
                    mylog::ERROR_LOG("read %s file content failed!", filename.c_str());
                    ifs.close();
                    return false;
                }
                // 关闭文件
                ifs.close();
                return true;
            }
        };
    }
}
#endif