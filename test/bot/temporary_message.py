from sparkai.llm.llm import ChatSparkLLM, ChunkPrintHandler
from sparkai.core.messages import ChatMessage

#星火认知大模型Spark Max的URL值，其他版本大模型URL值请前往文档（https://www.xfyun.cn/doc/spark/Web.html）查看
SPARKAI_URL = 'wss://spark-api.xf-yun.com/v3.1/chat'
#星火认知大模型调用秘钥信息，请前往讯飞开放平台控制台（https://console.xfyun.cn/services/bm35）查看
SPARKAI_APP_ID = '34908c2f'
SPARKAI_API_SECRET = 'ODYwY2RmMGEyNTI4M2QwYjdiZTliOGYz'
SPARKAI_API_KEY = 'a9ba61362ad9139a36ceb38d2575a2b0'
#星火认知大模型Spark Max的domain值，其他版本大模型domain值请前往文档（https://www.xfyun.cn/doc/spark/Web.html）查看
SPARKAI_DOMAIN = 'generalv3'

if __name__ == '__main__':
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
        content=''
    )]
    handler = ChunkPrintHandler()
    a = spark.generate([messages], callbacks=[handler])
    print(a)

# 你是一个智能AI聊天机器人，我把你接入了一个聊天软件中，用户会直接对你说自然语言，你的任务是将用户的自然语言转化为格式化的输出，使其符合接口规范，使得计算机能够读懂。请你直接给出命令，不要有多余信息，例如”输出“或者”格式“之类的，而是直接给如命令。参考：
#     显示所有支持的命令,格式help,
#     一对一聊天, 格式chat:friendid:message,
#     添加好友, 格式addfriend:friendid,
#     创建群组, 格式creategroup:groupname:groupdesc,
#     加入群组, 格式addgroup:groupid,
#     群聊,格式groupchat:groupid:message,
#     与机器人聊天,格式robot:message,
#     注销, 格式logout

# 你是一个专业的AI聊天机器人