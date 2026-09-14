const cloud = require("wx-server-sdk");
const mqtt = require("mqtt");

cloud.init({ env: cloud.DYNAMIC_CURRENT_ENV });

const db = cloud.database();
const CONTROL_TOPIC = "HIT/PetControl";
const MQTT_URL = process.env.CAREGUARD_MQTT_URL || "mqtt://broker-cn.emqx.io:1883";
const COMMANDS = new Set(["BEEP", "LIGHT_ON", "ECO_MODE", "NORMAL_MODE", "CAPTURE", "STATUS"]);

function normalizeCommand(value) {
  const command = String(value || "").trim().toUpperCase().replace(/-/g, "_");
  if (command === "SNAPSHOT" || command === "SNAP") return "CAPTURE";
  if (command === "LIGHT") return "LIGHT_ON";
  if (command === "ECO") return "ECO_MODE";
  if (command === "NORMAL") return "NORMAL_MODE";
  return command;
}

function publishMqtt(command) {
  return new Promise((resolve, reject) => {
    const client = mqtt.connect(MQTT_URL, {
      clientId: `WX_CareGuard_${Date.now()}_${Math.random().toString(16).slice(2, 8)}`,
      connectTimeout: 5000,
      reconnectPeriod: 0,
      clean: true
    });

    const timer = setTimeout(() => {
      client.end(true);
      reject(new Error("mqtt publish timeout"));
    }, 7000);

    client.on("connect", () => {
      const payload = JSON.stringify({
        command: command.command,
        desc: command.desc,
        operator: command.operator,
        timestamp: command.timestamp,
        source: "wx"
      });
      client.publish(CONTROL_TOPIC, payload, { qos: 0, retain: false }, error => {
        clearTimeout(timer);
        client.end(true);
        if (error) {
          reject(error);
          return;
        }
        resolve();
      });
    });

    client.on("error", error => {
      clearTimeout(timer);
      client.end(true);
      reject(error);
    });
  });
}

exports.main = async (event = {}) => {
  const commandCode = normalizeCommand(event.command);
  const command = {
    command: commandCode,
    desc: String(event.desc || ""),
    operator: String(event.operator || "WX_Guardian_Center"),
    timestamp: Number(event.timestamp || Date.now()),
    createdAt: db.serverDate()
  };

  if (!command.command) {
    return {
      ok: false,
      error: "command is required"
    };
  }
  if (!COMMANDS.has(command.command)) {
    return {
      ok: false,
      error: "unsupported command"
    };
  }

  const warnings = [];
  try {
    await db.collection("commands").add({ data: command });
  } catch (error) {
    warnings.push("commands collection is unavailable");
  }

  try {
    await publishMqtt(command);
  } catch (error) {
    warnings.push(`mqtt publish failed: ${error.message}`);
  }

  return {
    ok: warnings.length === 0,
    command,
    warnings
  };
};
