function escapeHtml(value) {
  return String(value ?? "")
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;")
    .replace(/'/g, "&#39;");
}

function numberOrNull(value) {
  const number = Number(value);
  return Number.isFinite(number) ? number : null;
}

function formatDateTime(value) {
  if (!value) return "未记录";
  const date = new Date(value);
  if (Number.isNaN(date.getTime())) return "未记录";
  return date.toLocaleString("zh-CN", { hour12: false });
}

function formatMetric(value, unit, fallback = "未接入") {
  const number = numberOrNull(value);
  return number === null ? fallback : `${number}${unit}`;
}

function formatGps(sample) {
  const lat = numberOrNull(sample?.lat);
  const lng = numberOrNull(sample?.lng);
  if (lat === null || lng === null) return "无定位";
  const prefix = sample?.hasGps === false ? "默认点" : "实时定位";
  return `${prefix}：${lng.toFixed(5)}, ${lat.toFixed(5)}`;
}

function average(values) {
  const numbers = values.map(numberOrNull).filter(value => value !== null);
  if (!numbers.length) return null;
  return Math.round(numbers.reduce((sum, value) => sum + value, 0) / numbers.length);
}

function max(values) {
  const numbers = values.map(numberOrNull).filter(value => value !== null);
  return numbers.length ? Math.max(...numbers) : null;
}

function min(values) {
  const numbers = values.map(numberOrNull).filter(value => value !== null);
  return numbers.length ? Math.min(...numbers) : null;
}

function summarizeTelemetry(telemetry) {
  const latest = telemetry.at(-1) || null;
  const alarmCount = telemetry.filter(sample => {
    const state = String(sample.state || sample.displayState || "");
    const hr = numberOrNull(sample.hr);
    const temp = numberOrNull(sample.temp);
    return state.includes("预警") || state.includes("异常") || (temp !== null && temp > 40) || (hr !== null && hr > 150);
  }).length;

  return {
    latest,
    alarmCount,
    avgHr: average(telemetry.map(sample => sample.hr)),
    maxTemp: max(telemetry.map(sample => sample.temp)),
    minTemp: min(telemetry.map(sample => sample.temp))
  };
}

function renderRows(rows, emptyMessage) {
  if (!rows.length) {
    return `<tr><td colspan="6" class="empty">${escapeHtml(emptyMessage)}</td></tr>`;
  }
  return rows.join("");
}

function renderTelemetryRows(telemetry) {
  const recent = telemetry.slice(-30).reverse();
  return renderRows(recent.map(sample => `
    <tr>
      <td>${escapeHtml(formatDateTime(sample.recordedAt || sample.timestamp))}</td>
      <td>${escapeHtml(sample.state || "未知")}</td>
      <td>${escapeHtml(formatMetric(sample.hr, " BPM"))}</td>
      <td>${escapeHtml(formatMetric(sample.temp, " °C"))}</td>
      <td>${escapeHtml(sample.hasBattery === false ? "未接入" : formatMetric(sample.battery, "%"))}</td>
      <td>${escapeHtml(formatGps(sample))}</td>
    </tr>
  `), "暂无体征记录");
}

function renderMissionRows(missions) {
  return renderRows(missions.map(mission => `
    <tr>
      <td>${escapeHtml(mission.name || "未命名档案")}</td>
      <td>${escapeHtml(formatDateTime(mission.createdAt))}</td>
      <td>${escapeHtml(formatMetric(mission.avgHr, " BPM"))}</td>
      <td>${escapeHtml(formatMetric(mission.maxTemp, " °C"))}</td>
      <td>${escapeHtml(formatMetric(mission.distance, " 米", "无有效定位"))}</td>
      <td>${escapeHtml(Array.isArray(mission.telemetryPoints) ? `${mission.telemetryPoints.length} 组` : "未记录")}</td>
    </tr>
  `), "暂无运动或健康档案");
}

export function generateReadableDataReport(data = {}) {
  const telemetry = Array.isArray(data.telemetry) ? data.telemetry : [];
  const missions = Array.isArray(data.missions) ? data.missions : [];
  const summary = summarizeTelemetry(telemetry);
  const latest = summary.latest;
  const exportedAt = formatDateTime(data.exportedAt || Date.now());

  return `<!doctype html>
<html lang="zh-CN">
  <head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <title>宠爱云护健康监测报告</title>
    <style>
      :root { color: #1d1d1f; background: #f5f5f7; font-family: -apple-system, BlinkMacSystemFont, "SF Pro Text", "PingFang SC", "Microsoft YaHei", sans-serif; }
      body { margin: 0; padding: 32px; }
      main { max-width: 1040px; margin: 0 auto; background: #fff; border: 1px solid #e5e5e7; border-radius: 18px; padding: 36px; box-shadow: 0 18px 48px rgba(0,0,0,.08); }
      header { display: flex; justify-content: space-between; gap: 24px; border-bottom: 1px solid #ececef; padding-bottom: 24px; }
      h1 { margin: 0 0 8px; font-size: 30px; letter-spacing: 0; }
      h2 { margin: 34px 0 14px; font-size: 18px; }
      p { margin: 0; color: #6e6e73; line-height: 1.7; }
      .badge { display: inline-block; padding: 6px 10px; border-radius: 999px; background: #e9f5ef; color: #1f7a4d; font-weight: 700; font-size: 12px; white-space: nowrap; }
      .cards { display: grid; grid-template-columns: repeat(4, minmax(0, 1fr)); gap: 12px; margin-top: 24px; }
      .card { border: 1px solid #ececef; border-radius: 12px; padding: 16px; background: #fbfbfd; }
      .label { color: #86868b; font-size: 12px; margin-bottom: 8px; }
      .value { font-size: 22px; font-weight: 800; color: #1d1d1f; }
      .note { background: #f6f8fa; border-left: 4px solid #2f7d5b; padding: 14px 16px; border-radius: 10px; margin-top: 18px; }
      table { width: 100%; border-collapse: collapse; overflow: hidden; border: 1px solid #ececef; border-radius: 12px; font-size: 13px; }
      th, td { text-align: left; padding: 12px 10px; border-bottom: 1px solid #ececef; vertical-align: top; }
      th { background: #f5f5f7; color: #515154; font-weight: 700; }
      tr:last-child td { border-bottom: 0; }
      .empty { text-align: center; color: #86868b; padding: 22px; }
      footer { margin-top: 32px; color: #86868b; font-size: 12px; border-top: 1px solid #ececef; padding-top: 18px; }
      @media print { body { background: #fff; padding: 0; } main { box-shadow: none; border: 0; border-radius: 0; } .cards { grid-template-columns: repeat(2, 1fr); } }
      @media (max-width: 760px) { body { padding: 16px; } main { padding: 22px; } header { display: block; } .cards { grid-template-columns: repeat(2, minmax(0, 1fr)); } table { font-size: 12px; } }
    </style>
  </head>
  <body>
    <main>
      <header>
        <div>
          <h1>宠爱云护健康监测报告</h1>
          <p>面向路演、答辩和非技术读者的可读版导出。数据来自本机浏览器保存的宠物项圈体征与运动档案。</p>
        </div>
        <div><span class="badge">导出时间：${escapeHtml(exportedAt)}</span></div>
      </header>

      <section class="cards" aria-label="摘要">
        <div class="card"><div class="label">体征记录</div><div class="value">${telemetry.length} 条</div></div>
        <div class="card"><div class="label">健康档案</div><div class="value">${missions.length} 个</div></div>
        <div class="card"><div class="label">平均心率</div><div class="value">${escapeHtml(formatMetric(summary.avgHr, " BPM"))}</div></div>
        <div class="card"><div class="label">预警次数</div><div class="value">${summary.alarmCount} 次</div></div>
      </section>

      <section>
        <h2>当前状态</h2>
        <div class="note">
          <p><strong>最近状态：</strong>${escapeHtml(latest?.state || "暂无数据")}</p>
          <p><strong>最近心率：</strong>${escapeHtml(formatMetric(latest?.hr, " BPM"))}；<strong>最近体温：</strong>${escapeHtml(formatMetric(latest?.temp, " °C"))}；<strong>体温范围：</strong>${escapeHtml(formatMetric(summary.minTemp, " °C"))} - ${escapeHtml(formatMetric(summary.maxTemp, " °C"))}</p>
          <p><strong>定位：</strong>${escapeHtml(latest ? formatGps(latest) : "暂无定位")}</p>
        </div>
      </section>

      <section>
        <h2>最近体征记录</h2>
        <table>
          <thead><tr><th>时间</th><th>状态</th><th>心率</th><th>体温</th><th>电量</th><th>定位</th></tr></thead>
          <tbody>${renderTelemetryRows(telemetry)}</tbody>
        </table>
      </section>

      <section>
        <h2>运动与健康档案</h2>
        <table>
          <thead><tr><th>档案</th><th>创建时间</th><th>平均心率</th><th>最高体温</th><th>路程</th><th>样本量</th></tr></thead>
          <tbody>${renderMissionRows(missions)}</tbody>
        </table>
      </section>

      <footer>
        <p>说明：本报告用于展示和沟通，不替代兽医诊断。若出现持续高热、心率异常或精神萎靡，请及时联系宠物医院。</p>
      </footer>
    </main>
  </body>
</html>`;
}
