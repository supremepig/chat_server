import socket
import sys
from sparkai.llm.llm import ChatSparkLLM, ChunkPrintHandler
from sparkai.core.messages import ChatMessage

# SPARKAI_DOMAIN = "generalv3.5"      # Max版本
SPARKAI_DOMAIN = "generalv3"       # Pro版本
#SPARKAI_DOMAIN = "general"         # Lite版本

# SPARKAI_URL = "wss://spark-api.xf-yun.com/v3.5/chat"   # Max服务地址
SPARKAI_URL = "wss://spark-api.xf-yun.com/v3.1/chat"  # Pro服务地址
#SPARKAI_URL = "wss://spark-api.xf-yun.com/v1.1/chat"  # Lite服务地址

SPARKAI_APP_ID = '34908c2f'
SPARKAI_API_SECRET = 'ODYwY2RmMGEyNTI4M2QwYjdiZTliOGYz'
SPARKAI_API_KEY = 'a9ba61362ad9139a36ceb38d2575a2b0'


def BotServer():
    connection, address = server.accept()
    print(connection, address)
    recv_str = connection.recv(1024)
    recv_str = recv_str.decode("utf8")
    if not recv_str:
        return
    print("receive:    {}".format(recv_str))

    spark = ChatSparkLLM(
        spark_api_url=SPARKAI_URL,
        spark_app_id=SPARKAI_APP_ID,
        spark_api_key=SPARKAI_API_KEY,
        spark_api_secret=SPARKAI_API_SECRET,
        spark_llm_domain=SPARKAI_DOMAIN,
        streaming=False,
    )

    messages = [ChatMessage(
        role="system",
        content='''你是一个智能AI聊天机器人，我把你接入了一个聊天软件中，用户会直接对你说自然语言，你需要判断用户的需求是使用这个聊天软件还是向你询问问题。
        如果在向你询问问题，请你先回复"hello"，之后正常回复即可。注意，这里回复"hello"非常重要！！不然系统无法识别命令。
        如果不是在向你询问问题，你的任务是将用户的自然语言转化为格式化的输出，使其符合接口规范，使得计算机能够读懂。请你直接给出命令，不要有多余信息，例如”输出“或者”格式“之类的，而是直接给出命令，命令只包括以下几种：
    一对一聊天, 格式chat:friendid:message,
    添加好友, 格式addfriend:friendid,
    创建群组, 格式creategroup:groupname:groupdesc,
    加入群组, 格式addgroup:groupid,
    群聊,格式groupchat:groupid:message,
    注销, 格式logout''')
    ,
    ChatMessage(
        role="user",
        content=recv_str
    )]
    handler = ChunkPrintHandler()
    response = spark.generate([messages], callbacks=[handler])
    reply = response.generations[0][0].text

    connection.send(bytes(reply, encoding="utf8"))
    print("send:   {}".format(reply))

    connection.close()

if __name__ == '__main__':
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.bind(("localhost", 8888))
    server.listen(10)

    try:
        while True:
            BotServer()

    except KeyboardInterrupt:
        server.close()
        print("client end, exit!")
        sys.exit()
