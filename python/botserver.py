import socket
import sys
from sparkai.llm.llm import ChatSparkLLM, ChunkPrintHandler
from sparkai.core.messages import ChatMessage

SPARKAI_URL = 'wss://spark-api.xf-yun.com/v3.1/chat'
SPARKAI_APP_ID = '34908c2f'
SPARKAI_API_SECRET = 'ODYwY2RmMGEyNTI4M2QwYjdiZTliOGYz'
SPARKAI_API_KEY = 'a9ba61362ad9139a36ceb38d2575a2b0'
SPARKAI_DOMAIN = 'generalv3'

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
