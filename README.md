# 基于muduo网络库的集群服务器项目 #

基于muduo网络库的集群服务器，可以实现跨服务器的聊天功能  
支持接入chatGPT/Claude/Sage和机器人对话

1. 基于muduo网络库的ThreadPool Reactor  
2. 基于json序列化与反序列化的消息收发协议  
3. 基于mysql设计聊天服务器的数据库  
4. 基于nginx实现tcp负载均衡  
5. 基于redis实现服务器间通信  
6. 基于Quora's Poe逆向工程，向POE发送HTTP请求，实现用户与机器人的对话  

## Enviornment ##

ubuntu 18.04

[boost 1.77.0](https://www.boost.org/) && [guide](https://blog.csdn.net/QIANGWEIYUAN/article/details/88792874) 

[muduo](https://github.com/chenshuo/muduo) && [guide](https://blog.csdn.net/QIANGWEIYUAN/article/details/89023980) 

mysql 
```bash
sudo apt install mysql-server
sudo apt-get install libmysqlclient-dev
```
修改mysql默认密码
```bash
sudo systemctl start mysql
sudo cat /etc/mysql/debian.cnf
mysql -u debian-sys-maint -p
ALTER USER 'root'@'localhost' IDENTIFIED BY '123456';
ALTER USER 'root'@'localhost' IDENTIFIED WITH 'mysql_native_password' BY '123456';
FLUSH PRIVILEGES;
exit;
```
```bash
mysql -u root -p
set character_set_server=utf8;
create database chat;
use chat;
source ./thirdparty/chat.sql;
```

[nginx 1.12.2](http://nginx.org/en/download.html) 
```bash
sudo apt-get install libpcre3 libpcre3-dev

./configure --with-stream
sudo make
sudo make install

sudo gedit /usr/local/nginx/conf/nginx.conf
```
add these
```text
# nginx tcp load balance config
stream {
    upstream MyServer {
        server 127.0.0.1:6000 weight=1 max_fails=3 fail_timeout=30s;
        server 127.0.0.1:6002 weight=1 max_fails=3 fail_timeout=30s;
    }
    server {
        proxy_connect_timeout 1s;
        # proxy_timeout 3s;
        listen 8000;
        proxy_pass MyServer;
        tcp_nodelay on;
    }
}
```

```bash
cd /usr/local/nginx/sbin
sudo ./nginx
```

[redis 4.0.9](https://redis.io/download/#redis-downloads) && [guide](https://blog.csdn.net/m0_61491995/article/details/125906242)
```bash
sudo apt-get install redis-server
mkdir /etc/redis
cd /etc/redis
sudo cp /usr/local/redis/redis.conf .
sudo gedit redis.conf

# find "daemonize no"
# change it into "daemonize yes"

redis-server /etc/redis/redis.conf
```
[hiredis](https://github.com/redis/hiredis)
```bash
make
sudo make install
sudo ldconfig /usr/local/lib
```

[poe-api](https://github.com/ading2210/poe-api)
```bash
pip install poe-api
```

## run ##
```bash
# make the project
cd chat_server
mkdir build
cd build
cmake ..
make

# make sure nginx and redis are running
sudo ./nginx
redis-server /etc/redis/redis.conf

# start the chat servers
cd ../bin
./server 127.0.0.1 6000
# new terminal
./server 127.0.0.1 6002

# start chat robot server
cd ../python
python botserver.py

# start clients
cd ../bin
./client 127.0.0.1 8000
# new terminal
./client 127.0.0.1 8000
```

<image src="imgs/demo1.png">
<image src="imgs/demo2.png">
<image src="imgs/demo3.png">
