#ifndef __CHATSERVICE_H_
#define __CHATSERVICE_H_

#include <unordered_map>
#include <functional>
#include <muduo/net/TcpConnection.h>
#include "json.hpp"
#include <mutex>

using namespace std;
using namespace muduo;
using namespace muduo::net;
using json = nlohmann::json;

#include "usermodel.hpp"
#include "offlinemsgmodel.hpp"
#include "friendmodel.hpp"
#include "groupmodel.hpp"

#include "redis.hpp"

// 处理消息事件的回调方法类型
using MsgHandler = std::function<void(const TcpConnectionPtr & conn, json js, Timestamp time)>;

class ChatService
{
public:
    void login(const TcpConnectionPtr & conn, json js, Timestamp time);
    void reg(const TcpConnectionPtr & conn, json js, Timestamp time);
    void oneChat(const TcpConnectionPtr & conn, json js, Timestamp time);
    void addFriend(const TcpConnectionPtr & conn, json js, Timestamp time);
    void createGroup(const TcpConnectionPtr & conn, json js, Timestamp time);
    void addToGroup(const TcpConnectionPtr & conn, json js, Timestamp time);
    void groupChat(const TcpConnectionPtr & conn, json js, Timestamp time);
    void logout(const TcpConnectionPtr & conn, json js, Timestamp time);

    // redis跨服务器消息回调函数的实现
    // clint发消息->对方不在当前服务器->redis消息队列->observer_channel_message()->
    //    _notify_message_handler()->绑定至redisSubscribeMessage()
    void redisSubscribeMessage(int userid, string message);

    // 获取sigleton单例的方法
    static ChatService* instance();

    // 通过msgid获得相应的处理函数(std::function)
    MsgHandler getHandler(int msgid);

    // 处理客户端异常断开 需要修改成offline
    void clientCloseException(const TcpConnectionPtr& conn);

    // 处理服务器异常断开
    void reset();

private:
    // 消息类型的id -> 处理方法(std::function)
    unordered_map<int, MsgHandler> _msgHandlerMap;

    // 数据操作类对象
    UserModel _userModel;
    OfflineMsgModel _offlineMsgModel;
    FriendModel _friendModel;
    GroupModel _groupModel;

    // 存储在线用户的通信连接
    unordered_map<int, TcpConnectionPtr> _userConnMap;
    // 此unordered_map的并发锁
    mutex _connMutex;

    // redis对象
    Redis _redis;

    // sigleton 构造函数放private
    ChatService();
};
#endif

// {"msgid":1,"id":22,"password":"cza"}
// {"msgid":1,"id":23,"password":"cat"}
// {"msgid":5,"id":22,"from":"cza","to":23,"msg":"mimi"}
// {"msgid":5,"id":23,"from":"cat","to":22,"msg":"meow"}
// {"msgid":7,"id":22,"friendid":23}
// {"msgid":3,"name":"aatrox","password":"aatrox"}
// {"msgid":3,"name":"jinx","password":"jinx"}
// {"msgid":3,"name":"dog","password":"dog"}
// {"msgid":8,"id":22,"groupname":"game","groupdesc":"lol group chat"}
// {"msgid":9,"id":24,"groupid":4}
// {"msgid":9,"id":25,"groupid":4}
// {"msgid":8,"id":22,"groupname":"myhome","groupdesc":"animals and i"}
// {"msgid":9,"id":23,"groupid":5}
// {"msgid":9,"id":26,"groupid":5}
// {"msgid":6,"id":22,"groupid":5,"msg":"hi my pets"}
// {"msgid":7,"id":22,"friendid":24}
// {"msgid":7,"id":22,"friendid":25}
// {"msgid":7,"id":22,"friendid":26}
// {"msgid":1,"id":26,"password":"dog"}