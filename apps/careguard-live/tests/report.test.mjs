import assert from "node:assert/strict";
import test from "node:test";

import { generateReadableDataReport } from "../src/report.js";

test("generates a readable HTML report instead of raw JSON", () => {
  const report = generateReadableDataReport({
    exportedAt: "2026-06-25T10:00:00.000Z",
    telemetry: [
      {
        recordedAt: "2026-06-25T09:59:00.000Z",
        state: "异常体温预警",
        hr: 168,
        temp: 41.3,
        battery: null,
        hasBattery: false,
        hasGps: true,
        lat: 45.7411,
        lng: 126.6255
      }
    ],
    missions: [
      {
        name: "路演健康档案",
        createdAt: "2026-06-25T09:58:00.000Z",
        avgHr: 120,
        maxTemp: 39.2,
        distance: 120.5,
        telemetryPoints: [{ hr: 120 }]
      }
    ]
  });

  assert.match(report, /^<!doctype html>/);
  assert.match(report, /宠爱云护健康监测报告/);
  assert.match(report, /异常体温预警/);
  assert.match(report, /41\.3 °C/);
  assert.match(report, /路演健康档案/);
  assert.doesNotMatch(report, /"telemetry"/);
});

test("escapes report fields before rendering HTML", () => {
  const report = generateReadableDataReport({
    telemetry: [{ state: "<script>alert(1)</script>", hr: 90, temp: 38.5 }],
    missions: [{ name: "<b>bad</b>", avgHr: 90, maxTemp: 38.5 }]
  });

  assert.match(report, /&lt;script&gt;alert\(1\)&lt;\/script&gt;/);
  assert.match(report, /&lt;b&gt;bad&lt;\/b&gt;/);
  assert.doesNotMatch(report, /<script>alert/);
});
