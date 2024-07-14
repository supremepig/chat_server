#include "chatservice.hpp"
#include "public.hpp"
#include <muduo/base/Logging.h>
#include <vector>
#include <iostream>

using namespace std;

ChatService* ChatService::instance()
{
    static ChatService service;
    return &service;
}

ChatService::ChatService()
{
    // 为不同消息类型注册处理函数，记入unordered_map中
    _msgHandlerMap.insert({LOGIN_MSG, std::bind(&ChatService::login, this, _1, _2, _3)});
    _msgHandlerMap.insert({REG_MSG, std::bind(&ChatService::reg, this, _1, _2, _3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG, std::bind(&ChatService::addFriend, this, _1, _2, _3)});
    _msgHandlerMap.insert({CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG, std::bind(&ChatService::addToGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});
    _msgHandlerMap.insert({LOGOUT_MSG, std::bind(&ChatService::logout, this, _1, _2, _3)});

    // 连接redis
    if(_redis.connect())
    {
        // 业务实现redisSubscribeMessage(int, string)，注册回调
        _redis.init_notify_handler(std::bind(&ChatService::redisSubscribeMessage, this, _1, _2));
    }
}

MsgHandler ChatService::getHandler(int msgid)
{
    auto iter = _msgHandlerMap.find(msgid);
    if(iter == _msgHandlerMap.end())
    {
        // lambda 默认处理器 -> 空操作
        return [=](const TcpConnectionPtr & conn, json js, Timestamp time)
        {
            LOG_ERROR << "msgid: " << msgid << "can not find handler";
        };
    }
    return _msgHandlerMap[msgid];
}

// 跨服务器的消息来了，找到用户转发给他
void ChatService::redisSubscribeMessage(int userid, string message)
{
    lock_guard<mutex> lock(_connMutex);
    auto iter = _userConnMap.find(userid);
    if(iter != _userConnMap.end())
    {
        iter->second->send(message);
        return;
    }
    else
    {
        _offlineMsgModel.insert(userid, message);
    }
}


// 登陆业务  ORM object-reation-map  不希望在业务代码中看到mysql语句
// msgid = 1
void ChatService::login(const TcpConnectionPtr & conn, json js, Timestamp time)
{
    LOG_INFO << "login service";
    int id = js["id"].get<int>();
    string pwd = js["password"];
    User user = _userModel.query(id);
    if(user.getId() == id && user.getPwd() == pwd)
    {
        // 重复登陆
        if(user.getState() == "online")
        {
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2;
            response["errmsg"] = "repeated login";
            conn->send(response.dump());
        }
        else
        {
            // 登陆成功


            // id用户登录成功后，向redis订阅channel(id)
            _redis.subscribe(id);

            // 记录通信连接  要考虑线程安全
            // 用大括号括起来  出作用域自动释放资源，不用lock/unlock了
            {
                lock_guard<mutex> lock(_connMutex);
                _userConnMap.insert({user.getId(), conn});
            }

            // 修改在线状态  线程安全由mysql保证
            user.setState("online");
            _userModel.updateState(user);

            // 返回消息
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0;
            response["id"] = user.getId();
            response["name"] = user.getName();

            // 查询离线消息，如果有，一并写入
            vector<string> vec = _offlineMsgModel.query(id);
            if(!vec.empty())
            {
                response["offlinemsg"] = vec;
                _offlineMsgModel.remove(id); // 清空之前的离线消息
            }

            // 查询好友信息并写入
            vector<User> uservec = _friendModel.query(id);
            if(!uservec.empty())
            {
                vector<string> friendvec;
                for(User& user : uservec)
                {
                    json temp;
                    temp["id"] = user.getId();
                    temp["name"] = user.getName();
                    temp["state"] = user.getState();
                    friendvec.push_back(temp.dump());
                }
                response["friends"] = friendvec;
            }

            // 查询群信息并写入
            vector<Group> groupvec = _groupModel.queryGroups(id);
            if(!groupvec.empty())
            {
                vector<string> groupstringvec;
                for(Group& group : groupvec)
                {
                    json level1;
                    level1["id"] = group.getId();
                    level1["groupname"] = group.getName();
                    level1["groupdesc"] = group.getDesc();
                    vector<string> usersingroup;
                    for(GroupUser& u : group.getUsers())
                    {
                        json level2;
                        level2["id"] = u.getId();
                        level2["name"] = u.getName();
                        level2["state"] = u.getState();
                        level2["role"] = u.getRole();
                        usersingroup.push_back(level2.dump());
                        // cout << level2 << endl;
                    }
                    level1["groupuser"] = usersingroup;
                    groupstringvec.push_back(level1.dump());
                }
                response["groups"] = groupstringvec;
            }

            // 最终发送
            conn->send(response.dump());
        }

    }
    else
    {
        // 查无此人
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "wrong id or password";
        conn->send(response.dump());
    }
}

// 注册业务 msgid = 3
void ChatService::reg(const TcpConnectionPtr & conn, json js, Timestamp time)
{
    LOG_INFO << "reg service";
    string name = js["name"];
    string pwd = js["password"];
    // LOG_INFO << "name:" << name;
    User user;
    user.setName(name);
    user.setPwd(pwd);
    bool ret = _userModel.insert(user);
    if(ret)
    {
        // 注册成功
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 0;
        response["id"] = user.getId();
        conn->send(response.dump());
    }
    else
    {
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 1;
        conn->send(response.dump());
    }

}

// 一对一聊天业务 msgid = 5
void ChatService::oneChat(const TcpConnectionPtr & conn, json js, Timestamp time)
{
    int to_id = js["to"].get<int>();

    // 线程安全
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(to_id);
        if(it != _userConnMap.end())
        {
            // 在线  且对方也连接至本服务器 直接转发消息
            it->second->send(js.dump());
            return;
        }
        else
        {
            // 在线，连到别的服务器了
            User user = _userModel.query(to_id);
            if(user.getState() == "online")
            {
                _redis.publish(to_id, js.dump());
                return;
            }
            else
            {
                // 不在线 存入离线消息
                _offlineMsgModel.insert(to_id, js.dump());
            }
        }
    }
}

// 添加好友服务 msgid = 7
void ChatService::addFriend(const TcpConnectionPtr & conn, json js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();
    _friendModel.insert(userid, friendid);
    _friendModel.insert(friendid, userid);
}

// 创建群业务 msgid = 8
void ChatService::createGroup(const TcpConnectionPtr & conn, json js, Timestamp time)
{
    int userid = js["id"].get<int>();
    string groupname = js["groupname"];
    string groupdesc = js["groupdesc"];
    Group group;
    group.setName(groupname);
    group.setDesc(groupdesc);
    if(_groupModel.createGroup(group))
    {
        _groupModel.addToGroup(group.getId(), userid, "creator");
    }
}

// 加入群业务 msgid = 9
void ChatService::addToGroup(const TcpConnectionPtr & conn, json js, Timestamp time)
{
    int groupid = js["groupid"].get<int>();
    int userid = js["id"].get<int>();
    _groupModel.addToGroup(groupid, userid, "normal");
}

// 群聊业务 msgid = 6
void ChatService::groupChat(const TcpConnectionPtr & conn, json js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    vector<int> useridvec = _groupModel.queryUsersInGroup(userid, groupid);

    {
        lock_guard<mutex> lock(_connMutex);
        for(int id : useridvec)
        {
            cout<< id << endl;
            auto iter = _userConnMap.find(id);
            if(iter != _userConnMap.end())
            {
                // 在线  且对方也连接至本服务器 直接转发消息
                iter->second->send(js.dump());
                continue;
            }
            else
            {
                // 在线，连到别的服务器了
                User user = _userModel.query(id);
                cout << user.getState() << endl;
                if(user.getState() == "online")
                {
                    _redis.publish(id, js.dump());
                    continue;
                }
                else
                {
                    // 不在线 存入离线消息表
                    _offlineMsgModel.insert(id, js.dump());
                }
            }
        }
    }
}

// 登出业务 msgid = 10
void ChatService::logout(const TcpConnectionPtr & conn, json js, Timestamp time)
{
    int userid = js["id"].get<int>();

    {
        lock_guard<mutex> lock(_connMutex);
        auto iter = _userConnMap.find(userid);
        if(iter != _userConnMap.end())
            _userConnMap.erase(iter);
    }

    User user;
    user.setId(userid);
    user.setState("offline");
    _userModel.updateState(user);

    // redis取消订阅
    _redis.unsubscribe(user.getId());
}

// 异常退出，先在unordered_map里删除，再改state=offline
void ChatService::clientCloseException(const TcpConnectionPtr& conn)
{
    User user;
    {
        lock_guard<mutex> lock(_connMutex);
        for(auto iter = _userConnMap.begin(); iter != _userConnMap.end(); iter++)
        {
            if(iter->second == conn)
            {
                user.setId(iter->first);
                _userConnMap.erase(iter);
                break;
            }
        }
    }
    
    if(user.getId() != -1)
    {
        user.setState("offline");
        _userModel.updateState(user);
    }

    // redis取消订阅
    _redis.unsubscribe(user.getId());
}

void ChatService::reset()
{
    // 将所有用户设为offline
    _userModel.resetState();
}