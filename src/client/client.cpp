#include <string>
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <ctime>
#include "json.hpp"
using json = nlohmann::json;

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <semaphore.h>
#include <atomic>

#include "group.hpp"
#include "user.hpp"
#include "public.hpp"

using namespace std;

// 当前登录的用户信息
User g_currentUser;
// 当前登录用户的好友列表
vector<User> g_currentUserFriendList;
// 当前登录用户的群组列表
vector<Group> g_currentUserGroupList;

// 显示登陆成功的用户信息
void showCurrentUserData();
// 接收线程
void readTaskHandler(int clientfd);
// 获取系统时间
string getCurrentTime();
// 主聊天页面
void mainMenu(int clientfd);
// 控制聊天页面
bool mainMenuRunning = false;

// 读写线程的通信
sem_t rwsem;
atomic_bool g_isLoginSuccess{false}; 

// 主线程=发送线程 子线程=接收线程
int main(int argc, char ** argv)
{
    if(argc < 3)
    {
        cerr << "command invalid. example: ./cilent 127.0.0.1 6000" << endl;
        exit(-1);
    }

    char* ip = argv[1];
    uint16_t port = atoi(argv[2]);

    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if(-1 == clientfd)
    {
        cerr << "socket create failed" <<endl;
    }

    sockaddr_in server;
    memset(&server, 0, sizeof(sockaddr_in));
    server.sin_addr.s_addr = inet_addr(ip);
    server.sin_family = AF_INET;
    server.sin_port = htons(port);

    if(-1 == connect(clientfd, (sockaddr*)&server, sizeof(sockaddr_in)))
    {
        cerr << "connect server error" << endl;
        close(clientfd);
        exit(-1);
    }

    sem_init(&rwsem, 0, 0);

    // 创建子线程专门用于接收消息
    std::thread readTask(readTaskHandler, clientfd);
    readTask.detach();

    while(1)
    {
        cout << "=================================================" << endl;
        cout << "1. login" << endl;
        cout << "2. register" << endl;
        cout << "3. quit" << endl;
        cout << "=================================================" << endl;
        cout << "choice: ";
        int choice = 0;
        cin >> choice;
        cin.get(); // 读掉回车

        switch(choice)
        {
        case 1: // login
        {
            int id = 0;
            char pwd[50] = {0};
            cout << "userid: ";
            cin >> id;
            cin.get();
            cout <<  "password: ";
            cin.getline(pwd, 50);

            // send login msg
            json js;
            js["msgid"] = LOGIN_MSG;
            js["id"] = id;
            js["password"] = pwd;
            string request = js.dump();
            int len = send(clientfd, request.c_str(), strlen(request.c_str())+1, 0);

            g_isLoginSuccess = false;
            if(-1 == len)
            {
                cerr << "send login message failed" << endl;
            }
            else
            {
                // 等子线程，登陆的响应消息处理完后会通知这里
                sem_wait(&rwsem);
                if(g_isLoginSuccess)
                {
                    // 进入聊天界面
                    mainMenuRunning = true;
                    mainMenu(clientfd);
                }
                else
                {
                    // 登录失败时子线程会打印错误信息的，这里不要了
                    // cout << "login failed" << endl;
                }
            }
        }
        break;
        case 2:
        {
            char name[50] = {0};
            char pwd[50] = {0};
            cout << "username: ";
            cin.getline(name, 50);
            cout << "password: ";
            cin.getline(pwd, 50);

            json js;
            js["msgid"] = REG_MSG;
            js["name"] = name;
            js["password"] = pwd;
            string request = js.dump();

            int len = send(clientfd, request.c_str(), strlen(request.c_str())+1, 0);
            if(-1 == len)
            {
                cerr << "send reg message failed" << endl;
            }
            else
            {
                sem_wait(&rwsem); // 等待子线程处理完注册业务         
            }
        }
        break;
        case 3:
            close(clientfd);
            sem_destroy(&rwsem);
            exit(0);
        default: 
            cerr << "invalid input, press enter to continue" <<endl;
            cin.clear();
            char dummy[1024] = {0};
            cin.getline(dummy, 1024);
            // cin.ignore(30,'\n');
            break;
        }
    }

}

void showCurrentUserData()
{
    cout << "====================user info====================" << endl;
    cout << "curret login user: " << g_currentUser.getId() << 
        " -- " << g_currentUser.getName() << endl;

    cout << "===================friend list===================" << endl;
    for(User& user : g_currentUserFriendList)
    {
        cout << "[" << user.getId() << "] " << user.getName() <<
            "\t-- state: " << user.getState() << endl;
    }

    cout << "===================group list====================" << endl;
    for(Group& group : g_currentUserGroupList)
    {
        cout << "[" << group.getId() << "] " << group.getName() << 
            "\t-- " << group.getDesc() << endl;
        cout << "\t-------------------------------------->" << endl;

        for(GroupUser& gpu : group.getUsers())
        {
            cout << "\t[" << gpu.getId() << "] " << gpu.getName() <<
                "\t-- state: " << gpu.getState() << "\trole: "<< gpu.getRole() << endl;
        }
    }
    cout << "=================================================" << endl;
}

// 处理登陆业务逻辑
void doLoginResponse(json& response)
{
    if(0 != response["errno"].get<int>())
    {
        // repeated login
        cerr << response["errmsg"] << endl;
        g_isLoginSuccess = false;
    }
    else
    {
        // cout << response <<endl;
        cout << "login success, welcome " << response["name"].get<string>() << endl;
        g_currentUser.setId(response["id"].get<int>());
        g_currentUser.setName(response["name"]);

        // if have friends, record friens info
        if(response.contains("friends"))
        {
            vector<string> vec = response["friends"];
            for(string &fri : vec)
            {
                json js = json::parse(fri);
                User user;
                user.setId(js["id"].get<int>());
                user.setName(js["name"]);
                user.setState(js["state"]);
                g_currentUserFriendList.push_back(user);
            }
        }

        // if have group, record group info
        if(response.contains("groups"))
        {
            vector<string> vec1 = response["groups"];
            for(string& groupstr : vec1)
            {
                json groupjs = json::parse(groupstr);
                Group group;
                group.setId(groupjs["id"].get<int>());
                group.setName(groupjs["groupname"]);
                group.setDesc(groupjs["groupdesc"]);
                vector<string> vec2 = groupjs["groupuser"];
                for(string& groupuser : vec2)
                {
                    json groupuserjs = json::parse(groupuser);
                    GroupUser gpu;
                    gpu.setId(groupuserjs["id"].get<int>());
                    gpu.setName(groupuserjs["name"]);
                    gpu.setState(groupuserjs["state"]);
                    gpu.setRole(groupuserjs["role"]);
                    group.getUsers().push_back(gpu);
                }
                g_currentUserGroupList.push_back(group);
            }
        }

        // 显示用户所有的信息
        showCurrentUserData();

        // 显示offlinemsg
        if(response.contains("offlinemsg"))
        {
            vector<string> vec = response["offlinemsg"];
            cout << "These are offline message: " << endl;
            for(string & s : vec)
            {
                json js = json::parse(s);
                if(js.contains("groupid"))
                {
                    cout << js["time"].get<string>() << " {group: " << js["groupid"].get<int>() << 
                        " -- " << js["name"].get<string>() << "} said: " << js["msg"].get<string>() << endl;
                }
                else
                {
                    cout << js["time"].get<string>() << " [" << js["id"].get<int>() << 
                        "] " << js["name"].get<string>() << " said: " <<
                        js["msg"].get<string>() << endl;
                }
                
            }
        }
        g_isLoginSuccess = true;
    }
}


// 处理注册业务逻辑
void doRegResponse(json& response)
{
    if(0 != response["errno"].get<int>())
    {
        cerr << "username is already exist, register failed" << endl;
    }
    else
    {
        cout << "register success, your user id: " << response["id"] << endl;
    }
    sem_post(&rwsem);
}

void readTaskHandler(int clientfd)
{
    while(1)
    {
        char buffer[4096] = {0};
        int len = recv(clientfd, buffer, 4096, 0);
        if(-1 == len || 0 == len)
        {
            close(clientfd);
            exit(-1);
        }

        json js = json::parse(buffer);
        if(ONE_CHAT_MSG == js["msgid"].get<int>())
        {
            cout << js["time"].get<string>() << " [" << js["id"].get<int>() << 
                "] " << js["name"].get<string>() << " said: " <<
                js["msg"].get<string>() << endl;
            continue;
        }
        else if(GROUP_CHAT_MSG == js["msgid"].get<int>())
        {
            cout << js["time"].get<string>() << " {group: " << js["groupid"].get<int>() << 
                " -- " << js["name"].get<string>() << "} said: " << js["msg"].get<string>() << endl;
            continue;
        }
        else if(LOGIN_MSG_ACK == js["msgid"].get<int>())
        {
            doLoginResponse(js); // 处理登陆响应的业务逻辑
            sem_post(&rwsem); // 通知主线程登陆结果处理完成
            continue;
        }
        else if(REG_MSG_ACK == js["msgid"].get<int>())
        {
            doRegResponse(js); // 处理注册响应的业务逻辑
            sem_post(&rwsem); // 通知主线程注册结果处理完成
            continue;
        }
    }
}

void help(int fd = 0, string s = "");
void chat(int, string);
void addfriend(int, string);
void creategroup(int, string);
void addgroup(int, string);
void groupchat(int, string);
void robot(int, string);
void logout(int, string);

//系统支持的客户端命令列表
unordered_map<string, string> commandMap = 
{
    {"help", "显示所有支持的命令,格式help"},
    {"chat", "一对一聊天, 格式chat:friendid:message"},
    {"addfriend", "添加好友, 格式addfriend:friendid"},
    {"creategroup", "创建群组, 格式creategroup:groupname:groupdesc"},
    {"addgroup", "加入群组, 格式addgroup:groupid"},
    {"groupchat", "群聊,格式groupchat:groupid:message"},
    {"robot", "与机器人聊天,格式robot:message"},
    {"logout", "注销, 格式logout"}
};

//注册系统支持的客户端命令处理
unordered_map<string, function<void(int, string)>> commandHandlerMap =
{
    {"help", help},
    {"chat", chat},
    {"addfriend", addfriend},
    {"creategroup", creategroup},
    {"addgroup", addgroup},
    {"groupchat", groupchat},
    {"robot", robot},
    {"logout", logout}
};

void mainMenu(int clientfd)
{
    help();
    char buffer[1024] = {0};
    while(mainMenuRunning)
    {
        cout << "write command: " << endl;
        cin.getline(buffer, 1024);
        string s(buffer);


        // connect robot server
        int robotfd = socket(AF_INET, SOCK_STREAM, 0);
        if(-1 == robotfd)
        {
            cerr << "robot socket create failed" <<endl;
        }
        sockaddr_in robot_server;
        memset(&robot_server, 0, sizeof(sockaddr_in));
        robot_server.sin_addr.s_addr = inet_addr("127.0.0.1");
        robot_server.sin_family = AF_INET;
        robot_server.sin_port = htons(8888);
        if(-1 == connect(robotfd, (sockaddr*)&robot_server, sizeof(sockaddr_in)))
        {
            cerr << "connect robot server error" << endl;
            close(robotfd);
            exit(-1);
        }

        int len = send(robotfd, s.c_str(), strlen(s.c_str())+1, 0);
        if(-1 == len)
        {
            cerr << "send robot-chat msg failed" << endl;
        }

        char answer[4096] = {0};
        recv(robotfd, answer, 4096, 0);
        string commandbuf = answer;
        if (commandbuf.rfind("hello", 0) == 0) { // rfind("hello", 0) == 0 检查字符串是否以"hello"开头
            cout << commandbuf << endl;
        } else {
            string command;
            int index = commandbuf.find(":");
            if(-1 == index)
            {
                command = commandbuf;
            }
            else
            {
                command = commandbuf.substr(0, index);
            }
            auto iter = commandHandlerMap.find(command);
            if(iter == commandHandlerMap.end())
            {
                cerr << "invalid command" << endl;
                cin.clear();
                continue;
            }
            iter->second(clientfd, commandbuf.substr(index+1, commandbuf.size()-index));
        }


        

    }
}


void mainMenuOld(int clientfd)
{
    help();
    char buffer[1024] = {0};
    while(mainMenuRunning)
    {
        cout << "write command: " << endl;
        cin.getline(buffer, 1024);
        string commandbuf(buffer);
        string command;
        int index = commandbuf.find(":");
        if(-1 == index)
        {
            command = commandbuf;
        }
        else
        {
            command = commandbuf.substr(0, index);
        }
        auto iter = commandHandlerMap.find(command);
        if(iter == commandHandlerMap.end())
        {
            cerr << "invalid command" << endl;
            cin.clear();
            continue;
        }
        iter->second(clientfd, commandbuf.substr(index+1, commandbuf.size()-index));

    }
}

void help(int clientfd, string s)
{
    cout << "command list:" << endl;
    cout << "Explore Your Options:\n"
         << "Ask whatever you are curious about!\n"
         << "Chat with anyone you want!\n"
         << "Start a group chat!\n"
         << "Add friends or join groups!\n"
         << "Logout when you're done!" << endl;
    // for(auto &p : commandMap)
    // {
    //     if(p.first.size() > 6)
    //         cout << p.first << " \t" << p.second << endl;
    //     else
    //         cout << p.first << " \t\t" << p.second << endl;
    // } 
    cout << "=================================================" << endl;
}

// friendid:message
void chat(int clientfd, string s)
{
    int index = s.find(":");
    if(-1 == index)
    {
        cerr << "chat command incorrect" << endl;
        return;
    }

    int friendid = atoi(s.substr(0,index).c_str());
    string msg = s.substr(index+1, s.size()-index-1);

    json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["to"] = friendid;
    js["msg"] = msg;
    js["time"] = getCurrentTime();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str())+1, 0);
    if(-1 == len)
    {
        cerr << "send one-chat msg failed" << endl;
    }
}

// friendid
void addfriend(int clientfd, string s)
{
    int friendid = atoi(s.c_str());
    json js;
    js["msgid"] = ADD_FRIEND_MSG;
    js["id"] = g_currentUser.getId();
    js["friendid"] = friendid;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str())+1, 0);
    if(-1 == len)
    {
        cerr << "send add friend msg failed" << endl;
    }
}

// groupname:groupdesc
void creategroup(int clientfd, string s)
{
    int index = s.find(":");
    if(-1 == index)
    {
        cerr << "create group command incorrect" << endl;
        return;
    }

    string groupname = s.substr(0, index);
    string groupdesc = s.substr(index+1, s.size()-index-1);

    json js;
    js["msgid"] = CREATE_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupname"] = groupname;
    js["groupdesc"] = groupdesc;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str())+1, 0);
    if(-1 == len)
    {
        cerr << "send create group msg failed" << endl;
    }
}

// groupid
void addgroup(int clientfd, string s)
{
    int groupid = atoi(s.c_str());
    json js;
    js["msgid"] = ADD_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupid"] = groupid;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str())+1, 0);
    if(-1 == len)
    {
        cerr << "send add group msg failed" << endl;
    }
}

// groupid:message
void groupchat(int clientfd, string s)
{
    int index = s.find(":");
    if(-1 == index)
    {
        cerr << "group-chat command incorrect" << endl;
        return;
    }

    int groupid = atoi(s.substr(0, index).c_str());
    string msg = s.substr(index+1, s.size()-index-1);

    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["groupid"] = groupid;
    js["msg"] = msg;
    js["time"] = getCurrentTime();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str())+1, 0);
    if(-1 == len)
    {
        cerr << "send group-chat msg failed" << endl;
    }
}

// message
void robot(int clientfd, string s)
{
    // connect robot server
    int robotfd = socket(AF_INET, SOCK_STREAM, 0);
    if(-1 == robotfd)
    {
        cerr << "robot socket create failed" <<endl;
    }
    sockaddr_in robot_server;
    memset(&robot_server, 0, sizeof(sockaddr_in));
    robot_server.sin_addr.s_addr = inet_addr("127.0.0.1");
    robot_server.sin_family = AF_INET;
    robot_server.sin_port = htons(8888);
    if(-1 == connect(robotfd, (sockaddr*)&robot_server, sizeof(sockaddr_in)))
    {
        cerr << "connect robot server error" << endl;
        close(robotfd);
        exit(-1);
    }

    int len = send(robotfd, s.c_str(), strlen(s.c_str())+1, 0);
    if(-1 == len)
    {
        cerr << "send robot-chat msg failed" << endl;
    }

    char answer[4096] = {0};
    recv(robotfd, answer, 4096, 0);
    cout << answer << endl;
}

void logout(int clientfd, string s)
{
    json js;
    js["msgid"] = LOGOUT_MSG;
    js["id"] = g_currentUser.getId();
    string buffer = js.dump();
    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str())+1, 0);
    if(-1 == len)
    {
        cerr << "send logout msg failed" << endl;
    }

    g_currentUser = User();
    g_currentUserFriendList.clear();
    g_currentUserGroupList.clear();
    mainMenuRunning = false;
}


string getCurrentTime()
{
    time_t t = time(nullptr);
	struct tm* now = localtime(&t);
 
    char s[1024] = {0};
    sprintf(s, "%04d-%02d-%02d %02d:%02d:%02d", 
        now->tm_year + 1900, now->tm_mon + 1, now->tm_mday, 
        now->tm_hour, now->tm_min, now->tm_sec);
    
    return string(s);
}