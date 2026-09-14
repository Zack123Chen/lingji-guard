import assert from "node:assert/strict";
import test from "node:test";

import {
  buildMissionReplaySeries,
  createHealthArchiveMission,
  formatGpsLabel,
  normalizeTelemetryPayload,
  parseTelemetryMessage,
  shouldTriggerHealthAlarm
} from "../src/telemetry.js";

test("normalizes hardware aliases and missing GPS without throwing", () => {
  const telemetry = normalizeTelemetryPayload({
    heartRate: "92 BPM",
    temperature: "38.7C",
    status: "walking",
    batteryPct: "86%"
  });

  assert.equal(telemetry.hr, 92);
  assert.equal(telemetry.temp, 38.7);
  assert.equal(telemetry.battery, 86);
  assert.equal(telemetry.state, "日常慢步");
  assert.equal(telemetry.hasGps, false);
  assert.equal(formatGpsLabel(telemetry), "无有效定位");
});

test("accepts longitude aliases and nested location objects", () => {
  const telemetry = normalizeTelemetryPayload({
    bpm: 128,
    bodyTemp: 39.2,
    activity: "running",
    location: {
      latitude: 45.7411,
      longitude: 126.6255
    }
  });

  assert.equal(telemetry.state, "快速奔跑");
  assert.equal(telemetry.hasGps, true);
  assert.equal(formatGpsLabel(telemetry), "[126.62550, 45.74110]");
});

test("parses key-value telemetry text from MCU logs", () => {
  const payload = parseTelemetryMessage("hr=76 temp=38.4 lat=45.75 lon=126.63 battery=91 status=active isSimulator=false");
  const telemetry = normalizeTelemetryPayload(payload);

  assert.equal(telemetry.hr, 76);
  assert.equal(telemetry.temp, 38.4);
  assert.equal(telemetry.lng, 126.63);
  assert.equal(telemetry.isSimulator, false);
});

test("parses simulator truthy string deliberately", () => {
  const telemetry = normalizeTelemetryPayload({ hr: 80, temp: 38.5, isSimulator: "true" });
  assert.equal(telemetry.isSimulator, true);
});

test("preserves hardware raw telemetry fields and missing battery state", () => {
  const telemetry = normalizeTelemetryPayload({
    id: "CareGuardESP32S3",
    state: "walking",
    hr: 0,
    temp: 26.4,
    isSimulator: false,
    seq: 42,
    ts: 123456,
    sampleReason: "realtime_5s_publish",
    humidity: 51.2,
    ir: 12345,
    red: 6789,
    heartContact: true,
    hrSampleType: "contact_estimate",
    motion: "walking",
    motionScore: 0.42,
    steps: 12,
    cadence: 96.5,
    air: "Pub OK",
    camera: "Camera OK",
    hasGps: false,
    locationSource: "default_zhengxin",
    coordinateSystem: "gcj02",
    tf: "TF OK",
    tfReady: true,
    photo: "/careguard/camera/IMG_1_12345_mqtt.jpg",
    photoFormat: "jpg",
    photoCount: 1,
    audio: "Audio OK",
    noiseLevel: 27,
    noiseEvent: "quiet",
    mode: "normal",
    lastCommand: "CAPTURE",
    lastCommandResult: "ok",
    lastCommandAt: 223344
  });

  assert.equal(telemetry.id, "CareGuardESP32S3");
  assert.equal(telemetry.seq, 42);
  assert.equal(telemetry.ts, 123456);
  assert.equal(telemetry.sampleReason, "realtime_5s_publish");
  assert.equal(telemetry.ir, 12345);
  assert.equal(telemetry.red, 6789);
  assert.equal(telemetry.humidity, 51.2);
  assert.equal(telemetry.heartContact, true);
  assert.equal(telemetry.hrSampleType, "contact_estimate");
  assert.equal(telemetry.motion, "walking");
  assert.equal(telemetry.motionScore, 0.42);
  assert.equal(telemetry.steps, 12);
  assert.equal(telemetry.cadence, 96.5);
  assert.equal(telemetry.air, "Pub OK");
  assert.equal(telemetry.camera, "Camera OK");
  assert.equal(telemetry.hasGps, false);
  assert.equal(telemetry.locationSource, "default_zhengxin");
  assert.equal(telemetry.coordinateSystem, "gcj02");
  assert.equal(telemetry.tf, "TF OK");
  assert.equal(telemetry.tfReady, true);
  assert.equal(telemetry.photo, "/careguard/camera/IMG_1_12345_mqtt.jpg");
  assert.equal(telemetry.photoFormat, "jpg");
  assert.equal(telemetry.photoCount, 1);
  assert.equal(telemetry.audio, "Audio OK");
  assert.equal(telemetry.noiseLevel, 27);
  assert.equal(telemetry.noiseEvent, "quiet");
  assert.equal(telemetry.mode, "normal");
  assert.equal(telemetry.lastCommand, "CAPTURE");
  assert.equal(telemetry.lastCommandResult, "ok");
  assert.equal(telemetry.lastCommandAt, 223344);
  assert.equal(telemetry.battery, null);
  assert.equal(telemetry.hasBattery, false);
});

test("keeps Zhengxin default coordinates visible while marking GPS as unavailable", () => {
  const telemetry = normalizeTelemetryPayload({
    id: "CareGuardESP32S3",
    seq: 8,
    hr: 0,
    temp: 32.1,
    isSimulator: false,
    lat: 45.74303224082512,
    lng: 126.6314330493297,
    hasGps: false,
    locationSource: "default_zhengxin",
    coordinateSystem: "gcj02"
  });

  assert.equal(telemetry.isSimulator, false);
  assert.equal(telemetry.hasGps, false);
  assert.equal(telemetry.lat, 45.74303224082512);
  assert.equal(telemetry.lng, 126.6314330493297);
  assert.equal(formatGpsLabel(telemetry), "默认点 [126.63143, 45.74303]");
});

test("normalizes ESP32 real MQTT JSON sample for the Web dashboard", () => {
  const payload = parseTelemetryMessage(JSON.stringify({
    id: "CareGuardESP32S3",
    seq: 7,
    ts: 84250,
    sampleReason: "realtime_5s_publish",
    state: "walking",
    hr: 84,
    hrSampleType: "beat_avg",
    temp: 38.6,
    isSimulator: false,
    lat: 45.7411,
    lng: 126.6255,
    humidity: 48.2,
    ir: 55231,
    red: 33120,
    heartContact: true,
    motion: "walking",
    motionScore: 0.34,
    steps: 128,
    cadence: 91.5,
    air: "Pub OK",
    camera: "Frame OK"
  }));
  const telemetry = normalizeTelemetryPayload(payload);

  assert.equal(telemetry.id, "CareGuardESP32S3");
  assert.equal(telemetry.seq, 7);
  assert.equal(telemetry.state, "日常慢步");
  assert.equal(telemetry.isSimulator, false);
  assert.equal(telemetry.hasGps, true);
  assert.equal(telemetry.hasBattery, false);
  assert.equal(telemetry.air, "Pub OK");
  assert.equal(telemetry.camera, "Frame OK");
  assert.equal(telemetry.heartContact, true);
  assert.equal(telemetry.hrSampleType, "beat_avg");
});

test("does not alarm when heart rate is zero because the sensor has no contact", () => {
  const telemetry = normalizeTelemetryPayload({
    hr: 0,
    temp: 38.2,
    state: "walking",
    heartContact: false,
    hrSampleType: "no_contact"
  });

  assert.equal(shouldTriggerHealthAlarm(telemetry), false);
});

test("creates a health archive without GPS and ignores invalid no-contact heart rate samples", () => {
  const mission = createHealthArchiveMission({
    now: new Date("2026-06-09T10:00:00"),
    recentTelemetry: [
      { source: "hardware", hr: 0, temp: 22.4, heartContact: false, hrSampleType: "no_contact", recordedAt: "2026-06-09T09:59:50" },
      { source: "hardware", hr: 82, temp: 22.7, heartContact: true, hrSampleType: "contact_estimate", recordedAt: "2026-06-09T09:59:55" }
    ],
    routeCoordinates: [],
    cumulativeDistance: 123
  });

  assert.equal(mission.type, "health_monitoring");
  assert.equal(mission.avgHr, 82);
  assert.equal(mission.maxTemp, 22.7);
  assert.equal(mission.distance, 0);
  assert.deepEqual(mission.path, []);
  assert.equal(mission.telemetryPoints.length, 2);
});

test("builds mission replay series from stored telemetry points instead of synthetic data", () => {
  const replay = buildMissionReplaySeries({
    avgHr: 99,
    maxTemp: 40,
    telemetryPoints: [
      { recordedAt: "2026-06-09T10:00:00", hr: 0, temp: 22.4 },
      { recordedAt: "2026-06-09T10:00:05", hr: 82, temp: 22.7 }
    ]
  });

  assert.deepEqual(replay.hr, [0, 82]);
  assert.deepEqual(replay.temp, [22.4, 22.7]);
  assert.equal(replay.labels.length, 2);
});
