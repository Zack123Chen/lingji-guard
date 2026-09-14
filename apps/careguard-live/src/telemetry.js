const STATE_LABELS = {
  active: "日常慢步",
  walk: "日常慢步",
  walking: "日常慢步",
  patrol: "日常慢步",
  run: "快速奔跑",
  running: "快速奔跑",
  charge: "快速奔跑",
  sleep: "熟睡",
  rest: "熟睡",
  fever: "异常体温预警",
  alarm: "异常体温预警",
  panic: "异常体温预警"
};

function firstDefined(...values) {
  return values.find(value => value !== undefined && value !== null && value !== "");
}

function toNumber(value) {
  if (typeof value === "number") return Number.isFinite(value) ? value : null;
  if (typeof value === "string") {
    const normalized = value.trim().replace(/[^\d.+-]/g, "");
    if (!normalized) return null;
    const parsed = Number(normalized);
    return Number.isFinite(parsed) ? parsed : null;
  }
  return null;
}

function normalizeState(value) {
  const raw = String(firstDefined(value, "Active")).trim();
  const key = raw.toLowerCase().replace(/[\s_-]/g, "");
  return STATE_LABELS[key] || raw;
}

function toBoolean(value) {
  if (typeof value === "boolean") return value;
  if (typeof value === "number") return value !== 0;
  if (typeof value === "string") return ["true", "1", "yes", "sim", "simulator"].includes(value.trim().toLowerCase());
  return false;
}

function toOptionalBoolean(value) {
  if (value === undefined || value === null || value === "") return null;
  return toBoolean(value);
}

function finiteNumberOrNull(value) {
  const number = toNumber(value);
  return number === null ? null : number;
}

export function parseTelemetryMessage(rawMessage) {
  if (typeof rawMessage !== "string") return rawMessage;

  try {
    return JSON.parse(rawMessage);
  } catch {
    const pairs = rawMessage
      .trim()
      .split(/[;,，\s]+/)
      .map(part => part.split(/[:=]/))
      .filter(pair => pair.length >= 2);

    if (!pairs.length) {
      throw new Error(`Unsupported telemetry message: ${rawMessage.slice(0, 80)}`);
    }

    return Object.fromEntries(pairs.map(([key, ...rest]) => [key.trim(), rest.join(":").trim()]));
  }
}

export function normalizeTelemetryPayload(input) {
  const payload = input?.data && typeof input.data === "object" ? input.data : input;
  if (!payload || typeof payload !== "object") {
    throw new Error("Telemetry payload must be an object");
  }

  const gps = payload.gps || payload.location || payload.position || {};
  const coords = Array.isArray(payload.coords) ? payload.coords : Array.isArray(payload.gps) ? payload.gps : null;
  const lat = toNumber(firstDefined(
    payload.lat,
    payload.latitude,
    payload.gpsLat,
    payload.gps_lat,
    gps.lat,
    gps.latitude,
    coords?.[0]
  ));
  const lng = toNumber(firstDefined(
    payload.lng,
    payload.lon,
    payload.long,
    payload.longitude,
    payload.gpsLng,
    payload.gpsLon,
    payload.gps_lng,
    payload.gps_lon,
    gps.lng,
    gps.lon,
    gps.longitude,
    coords?.[1]
  ));

  const hr = toNumber(firstDefined(
    payload.hr,
    payload.heartRate,
    payload.heart_rate,
    payload.bpm,
    payload.heart,
    payload.pulse
  ));
  const temp = toNumber(firstDefined(
    payload.temp,
    payload.temperature,
    payload.bodyTemp,
    payload.body_temp,
    payload.coreTemp,
    payload.core_temp
  ));
  const battery = toNumber(firstDefined(
    payload.battery,
    payload.bat,
    payload.batteryPct,
    payload.battery_pct,
    payload.power,
    payload.soc
  ));
  const ir = toNumber(firstDefined(payload.ir, payload.infrared, payload.maxIr, payload.max_ir));
  const red = toNumber(firstDefined(payload.red, payload.maxRed, payload.max_red));
  const humidity = toNumber(firstDefined(payload.humidity, payload.rh, payload.relativeHumidity, payload.relative_humidity));
  const motion = firstDefined(payload.motion, payload.activity, payload.mode, payload.state);
  const motionScore = toNumber(firstDefined(payload.motionScore, payload.motion_score, payload.activityScore, payload.activity_score));
  const steps = toNumber(firstDefined(payload.steps, payload.stepCount, payload.step_count));
  const cadence = toNumber(firstDefined(payload.cadence, payload.cadenceSpm, payload.cadence_spm));
  const air = firstDefined(payload.air, payload.airStatus, payload.air_status, payload.network, payload.mqtt);
  const camera = firstDefined(payload.camera, payload.cameraStatus, payload.camera_status);
  const deviceId = firstDefined(payload.id, payload.deviceId, payload.device_id);
  const seq = finiteNumberOrNull(firstDefined(payload.seq, payload.sequence, payload.sampleSeq, payload.sample_seq));
  const ts = firstDefined(payload.ts, payload.timestamp, payload.time);
  const sampleReason = firstDefined(payload.sampleReason, payload.sample_reason, payload.reason);
  const heartContact = toOptionalBoolean(firstDefined(payload.heartContact, payload.heart_contact, payload.contact));
  const hrSampleType = firstDefined(payload.hrSampleType, payload.hr_sample_type, payload.heartRateSampleType);
  const explicitHasGps = toOptionalBoolean(firstDefined(payload.hasGps, payload.has_gps, payload.gnssFix, payload.gnss_fix, payload.gpsFix, payload.gps_fix));
  const locationSource = firstDefined(payload.locationSource, payload.location_source, payload.locSource, payload.loc_source);
  const coordinateSystem = firstDefined(payload.coordinateSystem, payload.coordinate_system, payload.coordSystem, payload.coord_system);
  const tf = firstDefined(payload.tf, payload.tfStatus, payload.tf_status, payload.storage, payload.storageStatus, payload.storage_status);
  const tfReady = toOptionalBoolean(firstDefined(payload.tfReady, payload.tf_ready, payload.storageReady, payload.storage_ready));
  const photo = firstDefined(payload.photo, payload.photoPath, payload.photo_path, payload.lastPhoto, payload.last_photo);
  const photoFormat = firstDefined(payload.photoFormat, payload.photo_format);
  const photoCount = finiteNumberOrNull(firstDefined(payload.photoCount, payload.photo_count));
  const noiseLevel = finiteNumberOrNull(firstDefined(payload.noiseLevel, payload.noise_level, payload.soundLevel, payload.sound_level));
  const noiseEvent = firstDefined(payload.noiseEvent, payload.noise_event, payload.soundEvent, payload.sound_event);
  const audio = firstDefined(payload.audio, payload.audioStatus, payload.audio_status);
  const mode = firstDefined(payload.mode, payload.runtimeMode, payload.runtime_mode);
  const lastCommand = firstDefined(payload.lastCommand, payload.last_command);
  const lastCommandResult = firstDefined(payload.lastCommandResult, payload.last_command_result);
  const lastCommandAt = finiteNumberOrNull(firstDefined(payload.lastCommandAt, payload.last_command_at));
  const hasCoordinates = lat !== null && lng !== null;

  return {
    raw: payload,
    id: deviceId ?? null,
    seq,
    ts: ts ?? null,
    sampleReason: sampleReason ?? null,
    state: normalizeState(firstDefined(payload.state, payload.status, payload.motion, payload.mode, payload.activity)),
    hr: hr ?? 80,
    hrSampleType: hrSampleType ?? null,
    temp: temp ?? 38.5,
    // 硬件未上报电量时保持 null，由 UI 显示「未接入」，避免假的 100%
    battery,
    hasBattery: battery !== null,
    lat,
    lng,
    hasGps: explicitHasGps === null ? hasCoordinates : Boolean(explicitHasGps && hasCoordinates),
    locationSource: locationSource ?? null,
    coordinateSystem: coordinateSystem ?? null,
    isSimulator: toBoolean(payload.isSimulator),
    ir,
    red,
    humidity,
    heartContact,
    motion: motion ?? null,
    motionScore,
    steps,
    cadence,
    air: air ?? null,
    camera: camera ?? null,
    tf: tf ?? null,
    tfReady,
    photo: photo ?? null,
    photoFormat: photoFormat ?? null,
    photoCount,
    noiseLevel,
    noiseEvent: noiseEvent ?? null,
    audio: audio ?? null,
    mode: mode ?? null,
    lastCommand: lastCommand ?? null,
    lastCommandResult: lastCommandResult ?? null,
    lastCommandAt
  };
}

export function formatGpsLabel(telemetry) {
  const hasCoordinates = finiteNumberOrNull(telemetry?.lat) !== null && finiteNumberOrNull(telemetry?.lng) !== null;
  if (telemetry.hasGps && hasCoordinates) {
    return `[${telemetry.lng.toFixed(5)}, ${telemetry.lat.toFixed(5)}]`;
  }
  if (hasCoordinates && telemetry.locationSource === "default_zhengxin") {
    return `默认点 [${telemetry.lng.toFixed(5)}, ${telemetry.lat.toFixed(5)}]`;
  }
  return "无有效定位";
}

export function shouldTriggerHealthAlarm(telemetry) {
  const state = String(telemetry?.state ?? "");
  const hr = finiteNumberOrNull(telemetry?.hr);
  const temp = finiteNumberOrNull(telemetry?.temp);
  const hasReliableHeartRate = telemetry?.heartContact !== false && telemetry?.hrSampleType !== "no_contact";

  return (temp !== null && temp > 40.0) ||
    (hr !== null && hr > 150 && hasReliableHeartRate) ||
    state.includes("ALARM") ||
    state === "异常体温预警";
}

export function createHealthArchiveMission({
  recentTelemetry = [],
  routeCoordinates = [],
  cumulativeDistance = 0,
  now = new Date()
} = {}) {
  const samples = recentTelemetry
    .filter(sample => sample && typeof sample === "object")
    .map(sample => ({
      timestamp: sample.timestamp ?? Date.now(),
      recordedAt: sample.recordedAt ?? null,
      source: sample.source ?? "hardware",
      state: sample.state ?? sample.displayState ?? "unknown",
      hr: finiteNumberOrNull(sample.hr),
      temp: finiteNumberOrNull(sample.temp),
      heartContact: sample.heartContact ?? null,
      hrSampleType: sample.hrSampleType ?? null,
      sampleReason: sample.sampleReason ?? null,
      motionScore: finiteNumberOrNull(sample.motionScore),
      steps: finiteNumberOrNull(sample.steps),
      cadence: finiteNumberOrNull(sample.cadence),
      hasGps: Boolean(sample.hasGps),
      lat: finiteNumberOrNull(sample.lat),
      lng: finiteNumberOrNull(sample.lng),
      locationSource: sample.locationSource ?? null,
      coordinateSystem: sample.coordinateSystem ?? null,
      tf: sample.tf ?? null,
      photo: sample.photo ?? null,
      noiseLevel: finiteNumberOrNull(sample.noiseLevel),
      noiseEvent: sample.noiseEvent ?? null,
      mode: sample.mode ?? null,
      lastCommand: sample.lastCommand ?? null,
      lastCommandResult: sample.lastCommandResult ?? null
    }));

  const path = routeCoordinates
    .map(point => Array.isArray(point) ? point : [point?.lat, point?.lng])
    .map(([lat, lng]) => [finiteNumberOrNull(lat), finiteNumberOrNull(lng)])
    .filter(([lat, lng]) => lat !== null && lng !== null);

  const reliableHr = samples
    .filter(sample => sample.hr !== null && sample.hr > 0 && sample.heartContact !== false && sample.hrSampleType !== "no_contact")
    .map(sample => sample.hr);
  const temps = samples
    .filter(sample => sample.temp !== null)
    .map(sample => sample.temp);

  const hasRoute = path.length >= 2;
  const avgHr = reliableHr.length
    ? Math.round(reliableHr.reduce((sum, value) => sum + value, 0) / reliableHr.length)
    : 0;
  const maxTemp = temps.length ? Math.max(...temps) : 0;

  return {
    name: `${hasRoute ? "运动归档" : "健康监测"}_${now.toLocaleTimeString()}`,
    type: hasRoute ? "motion_route" : "health_monitoring",
    avgHr,
    maxTemp: parseFloat(maxTemp.toFixed(1)),
    distance: hasRoute ? parseFloat(Number(cumulativeDistance || 0).toFixed(2)) : 0,
    path,
    telemetryPoints: samples,
    validHeartRateCount: reliableHr.length,
    heartContactRate: samples.length
      ? parseFloat((samples.filter(sample => sample.heartContact === true).length / samples.length).toFixed(3))
      : 0
  };
}

export function buildMissionReplaySeries(mission) {
  const points = Array.isArray(mission?.telemetryPoints) ? mission.telemetryPoints : [];
  if (!points.length) {
    return {
      labels: [],
      hr: [],
      temp: []
    };
  }

  return {
    labels: points.map((point, index) => point.recordedAt
      ? new Date(point.recordedAt).toLocaleTimeString()
      : `T-${index * 5}s`),
    hr: points.map(point => finiteNumberOrNull(point.hr) ?? 0),
    temp: points.map(point => finiteNumberOrNull(point.temp) ?? 0)
  };
}
