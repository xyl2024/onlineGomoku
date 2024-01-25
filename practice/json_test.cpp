#include <iostream>
#include <jsoncpp/json/json.h>

using std::cout;
using std::endl;

/// 测试序列化
/// 将Json::Value对象转换成字符串
std::string test_seri()
{
    /// 1.定义Json::Value对象
    Json::Value root;
    root["姓名"] = "小源";
    root["年龄"] = 20;
    root["成绩"].append(90);
    root["成绩"].append(90.9);
    root["成绩"].append(99.9);
    /// 2.使用StreamWriterBuilder工厂类对象创建StreamWriter对象
    Json::StreamWriterBuilder swb;
    Json::StreamWriter* sw = swb.newStreamWriter();

    /// 3.使用StreamWriter对象实现序列化
    std::stringstream ss;
    if(sw->write(root, &ss) != 0)
    {
        std::cout << "StreamWriter write failed.\n";
        delete sw;
        return "";
    }
    std::cout << ss.str() << endl;  //序列化出的字符串，中文默认为Unicode编码！！！
    delete sw; // 释放sw对象！！！
    return ss.str();
}

/// @brief 测试反序列化
/// 将字符串转换成Json::Value对象
void test_unseri(const std::string& s)
{
    /// 1.使用CharReaderBuilder工厂类对象创建CharReader对象
    Json::CharReaderBuilder crb;
    /// 2.使用CharReader对象将字符串反序列化
    Json::CharReader* cr = crb.newCharReader();
    Json::Value root;
    std::string errs;
    if(cr->parse(s.c_str(), s.c_str()+s.size(), &root, &errs) == false)
    {
        cout << "CharReader parse failed.\n";
        delete cr;
        return;
    }
    /// 3.遍历Json::Value对象中的内容
    cout << "姓名：" << root["姓名"].asString() << endl;
    cout << "年龄：" << root["年龄"].asString() << endl;
    int sz = root["成绩"].size();
    for(int i = 0; i < sz; ++i)
    {
        cout << "成绩：" << root["成绩"][i].asDouble() << "\n";
    }
    delete cr;
}

int main()
{
    std::string s = test_seri();
    test_unseri(s);
    return 0;
}