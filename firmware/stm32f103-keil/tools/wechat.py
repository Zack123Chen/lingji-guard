import paho.mqtt.client as mqtt
import requests
import json
import time
import os

SENDKEY = os.environ.get("SERVERCHAN_SENDKEY", "")
if not SENDKEY:
    raise RuntimeError("请先设置环境变量 SERVERCHAN_SENDKEY")
MQTT_BROKER = "broker-cn.emqx.io"
MQTT_PORT = 1883
MQTT_TOPIC = "HIT/PetData"

# 护士长专属：防打扰冷却记录本
last_alert_time = 0  
COOLDOWN_SECONDS = 60  # 60秒内绝不重复发微信骚扰你

def send_wechat_alert(hr, temp, state):
    url = f"https://sctapi.ftqq.com/{SENDKEY}.send"
    data = {
        "title": f"🚨 宠物健康警报: 心率 {hr} BPM",
        "desp": f"**状态**: {state}\n\n**实时心率**: {hr} BPM\n\n**体表温度**: {temp} °C\n\n*请立即查看宠物状况！*"
    }
    try:
        response = requests.post(url, data=data)
        # 抓取 Server酱 的真实反馈
        result = response.json() 
        if result.get("code") == 0:
            print("✅ 微信推送成功！快看手机！")
        else:
            # 打印被拒绝的真实原因（比如超频、额度耗尽等）
            print(f"❌ 推送失败，Server酱报错: {result.get('message', response.text)}")
    except Exception as e:
        print("❌ 网络请求错误:", e)

def on_connect(client, userdata, flags, rc):
    print(f"✅ 护士长已上线，正在监听 {MQTT_TOPIC} ...")
    client.subscribe(MQTT_TOPIC)

def on_message(client, userdata, msg):
    global last_alert_time
    payload = msg.payload.decode('utf-8')
    try:
        data = json.loads(payload)
        
        # 只要发现报警字眼
        if "ALARM" in data.get("state", ""):
            current_time = time.time()
            
            # 判断距离上一次发微信有没有超过 60 秒
            if current_time - last_alert_time > COOLDOWN_SECONDS:
                print(f"\n⚠️ 检测到心率异常 ({data['hr']})，准备呼叫铲屎官...")
                send_wechat_alert(data["hr"], data["temp"], data["state"])
                last_alert_time = current_time
            else:
                # 倒计时拦截提示，让你知道后台没死机，只是在冷却
                remain = int(COOLDOWN_SECONDS - (current_time - last_alert_time))
                print(f"⏳ 护士长拦截：报警过于频繁，剩余冷却时间 {remain} 秒", end="\r")
                
    except Exception as e:
        print("解析数据失败:", e)

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

client.connect(MQTT_BROKER, MQTT_PORT, 60)
client.loop_forever()