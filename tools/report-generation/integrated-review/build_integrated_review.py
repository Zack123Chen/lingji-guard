from __future__ import annotations

import math
import os
import re
import shutil
import subprocess
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont
from docx import Document
from docx.enum.section import WD_SECTION
from docx.enum.table import WD_CELL_VERTICAL_ALIGNMENT, WD_TABLE_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH, WD_BREAK, WD_LINE_SPACING
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Cm, Pt, RGBColor


ROOT = Path(__file__).resolve().parents[3]
REPORTS = ROOT / "docs" / "reports"
BASE_DOCX = REPORTS / "revisions" / "大一年度项目结题报告(2)_答辩修订版.docx"
OLD_DOC = REPORTS / "source" / "大一年度项目结题报告.doc"
OUT_DOCX = REPORTS / "revisions" / "大一年度项目结题报告_技术整合审阅版.docx"
OUT_PDF = REPORTS / "revisions" / "大一年度项目结题报告_技术整合审阅版.pdf"
OUT_MD = REPORTS / "notes" / "大一年度项目结题报告_技术整合说明.md"
WORK = Path(__file__).resolve().parent
IMG = WORK / "assets"
RENDER = WORK / "render"


def font(name: str, east_asia: str | None = None):
    rpr = OxmlElement("w:rPr")
    rfonts = OxmlElement("w:rFonts")
    rfonts.set(qn("w:ascii"), name)
    rfonts.set(qn("w:hAnsi"), name)
    rfonts.set(qn("w:eastAsia"), east_asia or name)
    rpr.append(rfonts)
    return rpr


def set_run_font(run, size_pt: float, cn: str = "宋体", en: str = "Times New Roman", bold=False):
    run.font.size = Pt(size_pt)
    run.font.bold = bold
    run.font.name = en
    run._element.rPr.rFonts.set(qn("w:eastAsia"), cn)
    run.font.color.rgb = RGBColor(0, 0, 0)


def set_para_format(p, size=12, cn="宋体", en="Times New Roman", align=None, first_indent=True):
    pf = p.paragraph_format
    pf.line_spacing_rule = WD_LINE_SPACING.EXACTLY
    pf.line_spacing = Pt(16)
    pf.space_before = Pt(0)
    pf.space_after = Pt(0)
    if first_indent:
        pf.first_line_indent = Pt(size * 2)
    if align is not None:
        p.alignment = align
    for r in p.runs:
        set_run_font(r, size, cn, en)


def clear_para(p):
    for r in list(p.runs):
        r._element.getparent().remove(r._element)


def add_para(doc, text="", size=12, cn="宋体", en="Times New Roman", align=None, first_indent=True, bold=False):
    p = doc.add_paragraph()
    r = p.add_run(text)
    set_run_font(r, size, cn, en, bold=bold)
    set_para_format(p, size=size, cn=cn, en=en, align=align, first_indent=first_indent)
    return p


def add_heading1(doc, text):
    add_para(doc, "", size=12, first_indent=False)
    p = add_para(doc, text, size=14, cn="黑体", align=WD_ALIGN_PARAGRAPH.CENTER, first_indent=False, bold=False)
    add_para(doc, "", size=12, first_indent=False)
    return p


def add_heading2(doc, text):
    p = add_para(doc, text, size=12, cn="黑体", align=WD_ALIGN_PARAGRAPH.LEFT, first_indent=False)
    return p


def add_caption(doc, text):
    return add_para(doc, text, size=10.5, cn="宋体", align=WD_ALIGN_PARAGRAPH.CENTER, first_indent=False)


def add_table(doc, headers, rows, widths_cm=None):
    table = doc.add_table(rows=1, cols=len(headers))
    table.alignment = WD_TABLE_ALIGNMENT.CENTER
    table.style = "Table Grid"
    for i, h in enumerate(headers):
        cell = table.rows[0].cells[i]
        cell.text = h
        cell.vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.CENTER
        for p in cell.paragraphs:
            p.alignment = WD_ALIGN_PARAGRAPH.CENTER
            set_para_format(p, size=10.5, cn="宋体", first_indent=False)
            for r in p.runs:
                set_run_font(r, 10.5, "宋体", bold=True)
    for row in rows:
        cells = table.add_row().cells
        for i, val in enumerate(row):
            cells[i].text = str(val)
            cells[i].vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.CENTER
            for p in cells[i].paragraphs:
                p.alignment = WD_ALIGN_PARAGRAPH.CENTER if i == 0 else WD_ALIGN_PARAGRAPH.LEFT
                set_para_format(p, size=10.5, cn="宋体", first_indent=False)
    if widths_cm:
        for row in table.rows:
            for idx, width in enumerate(widths_cm):
                row.cells[idx].width = Cm(width)
    return table


def remove_doc_grid_and_snap(doc):
    for sect in doc.sections:
        sect.page_width = Cm(21)
        sect.page_height = Cm(29.7)
        sect.top_margin = Cm(2)
        sect.bottom_margin = Cm(2)
        sect.left_margin = Cm(2)
        sect.right_margin = Cm(2)
        sect._sectPr.pgSz.set(qn("w:orient"), "portrait")
        for dg in sect._sectPr.findall(qn("w:docGrid")):
            sect._sectPr.remove(dg)
    for snap in doc.element.findall(".//" + qn("w:snapToGrid")):
        parent = snap.getparent()
        if parent is not None:
            parent.remove(snap)


def set_defaults(doc):
    style = doc.styles["Normal"]
    style.font.name = "Times New Roman"
    style._element.rPr.rFonts.set(qn("w:eastAsia"), "宋体")
    style.font.size = Pt(12)
    style.paragraph_format.line_spacing_rule = WD_LINE_SPACING.EXACTLY
    style.paragraph_format.line_spacing = Pt(16)
    remove_doc_grid_and_snap(doc)


def strip_from_toc(doc):
    body = doc.element.body
    idx = None
    for i, child in enumerate(body):
        texts = "".join(child.itertext()).strip()
        if texts.startswith("目录"):
            idx = i
            break
    if idx is None:
        raise RuntimeError("未找到目录起点，停止以免破坏前置行政页。")
    children = list(body)
    sect_pr = children[-1] if children and children[-1].tag == qn("w:sectPr") else None
    for child in children[idx:]:
        if child is not sect_pr:
            body.remove(child)
    if sect_pr is not None and sect_pr.getparent() is None:
        body.append(sect_pr)


def image_size_cm(path: Path, width_cm: float) -> tuple[float, float]:
    with Image.open(path) as im:
        w, h = im.size
    return width_cm, width_cm * h / w


def add_picture(doc, path: Path, width_cm: float, caption: str | None = None):
    _, h_cm = image_size_cm(path, width_cm)
    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    pf = p.paragraph_format
    pf.line_spacing_rule = WD_LINE_SPACING.AT_LEAST
    pf.line_spacing = Pt(max(18, h_cm * 28.346 + 8))
    pf.space_before = Pt(2)
    pf.space_after = Pt(2)
    p.add_run().add_picture(str(path), width=Cm(width_cm))
    if caption:
        add_caption(doc, caption)


def draw_connection_diagram(path: Path):
    W, H = 1800, 1260
    im = Image.new("RGB", (W, H), "white")
    d = ImageDraw.Draw(im)
    try:
        font_regular = ImageFont.truetype("/System/Library/Fonts/PingFang.ttc", 38)
        font_small = ImageFont.truetype("/System/Library/Fonts/PingFang.ttc", 28)
        font_title = ImageFont.truetype("/System/Library/Fonts/PingFang.ttc", 44)
    except Exception:
        font_regular = font_small = font_title = None

    def rect(xy, fill, outline="#243047", width=3, radius=18):
        d.rounded_rectangle(xy, radius=radius, fill=fill, outline=outline, width=width)

    def text_center(xy, text, f=font_regular, fill="#111827"):
        x1, y1, x2, y2 = xy
        bbox = d.multiline_textbbox((0, 0), text, font=f, spacing=6, align="center")
        tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
        d.multiline_text(((x1 + x2 - tw) / 2, (y1 + y2 - th) / 2), text, font=f, fill=fill, spacing=6, align="center")

    rect((705, 430, 1095, 770), "#eef2ff", "#1d4ed8", 5)
    text_center((705, 430, 1095, 770), "ESP32-S3\n当前工程样机主控", font_title, "#1e3a8a")

    modules = [
        ((80, 120, 435, 260), "TMP117\nI²C 0x48\nSDA10/SCL9", "#ecfdf5"),
        ((80, 300, 435, 440), "MAX30102\nI²C 0x57\nSDA10/SCL9", "#ecfdf5"),
        ((80, 480, 435, 650), "OV2640 SCCB\nI²C 0x30\nSIOD10/SIOC9", "#ecfdf5"),
        ((80, 720, 435, 930), "OV2640 DVP\nD0-D7:34/48/47/33/35/37/38/39\nPCLK36 VSYNC41 HREF40\nXCLK=-1", "#fff7ed"),
        ((1365, 135, 1720, 285), "ADXL362\nSPI CS2\nMOSI12/MISO11/SCLK13", "#f0f9ff"),
        ((1365, 330, 1720, 500), "TFT\nSPI CS7 DC8 BL5\nMOSI12/MISO11/SCLK13", "#f0f9ff"),
        ((1365, 560, 1720, 725), "TF 卡\nSDMMC 1-bit\nCLK16 CMD21 DATA0 15 CD14", "#fefce8"),
        ((1365, 790, 1720, 955), "Air780E\nUART RX18/TX17\nEN6 PWRON4", "#fdf2f8"),
        ((520, 955, 810, 1110), "麦克风\nADC GPIO3", "#f8fafc"),
        ((990, 955, 1280, 1110), "扬声器\nEN42 OUT46/45", "#f8fafc"),
        ((755, 85, 1045, 230), "本地快门键\nGPIO4 低电平触发\n与 PWRON 复用", "#faf5ff"),
    ]
    for xy, label, fill in modules:
        rect(xy, fill)
        text_center(xy, label, font_small)

    def line(start, end, label, color="#334155", offset=(0, 0)):
        d.line([start, end], fill=color, width=5)
        mx, my = (start[0] + end[0]) / 2 + offset[0], (start[1] + end[1]) / 2 + offset[1]
        d.rounded_rectangle((mx - 150, my - 24, mx + 150, my + 24), radius=10, fill="white", outline=color, width=2)
        text_center((mx - 150, my - 24, mx + 150, my + 24), label, font_small, color)

    line((435, 190), (705, 520), "I²C 共享", "#047857", (-15, -30))
    line((435, 370), (705, 570), "I²C 共享", "#047857", (-15, 5))
    line((435, 565), (705, 620), "SCCB/I²C", "#047857", (-10, 25))
    line((435, 825), (705, 705), "DVP 并口", "#c2410c", (20, 40))
    line((1095, 210), (1365, 210), "GPIO4", "#7e22ce", (0, -40))
    line((1095, 540), (1365, 210), "SPI 共享", "#0369a1", (10, -25))
    line((1095, 590), (1365, 415), "SPI 共享", "#0369a1", (10, 15))
    line((1095, 655), (1365, 640), "SDMMC", "#a16207", (0, -35))
    line((1095, 710), (1365, 870), "UART/控制", "#be185d", (10, 25))
    line((810, 955), (805, 770), "ADC", "#475569", (-50, 0))
    line((990, 955), (995, 770), "PWM/控制", "#475569", (55, 0))
    d.text((70, 30), "最终样机模块连接与总线分配示意图（依据 Pins.h 与当前固件）", font=font_title, fill="#111827")
    d.text((70, 1165), "说明：该图表示最终固件 GPIO 连接关系，不作为完整电源原理图；相机 XCLK 配置为 -1，未绘制猜测时钟线。", font=font_small, fill="#334155")
    im.save(path, quality=95)


def draw_dataflow_diagram(path: Path):
    W, H = 1800, 880
    im = Image.new("RGB", (W, H), "white")
    d = ImageDraw.Draw(im)
    try:
        font_regular = ImageFont.truetype("/System/Library/Fonts/PingFang.ttc", 36)
        font_small = ImageFont.truetype("/System/Library/Fonts/PingFang.ttc", 27)
        font_title = ImageFont.truetype("/System/Library/Fonts/PingFang.ttc", 42)
    except Exception:
        font_regular = font_small = font_title = None

    def rect(xy, fill, outline="#243047", width=3, radius=18):
        d.rounded_rectangle(xy, radius=radius, fill=fill, outline=outline, width=width)

    def text_center(xy, text, f=font_regular, fill="#111827"):
        x1, y1, x2, y2 = xy
        bbox = d.multiline_textbbox((0, 0), text, font=f, spacing=5, align="center")
        tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
        d.multiline_text(((x1 + x2 - tw) / 2, (y1 + y2 - th) / 2), text, font=f, fill=fill, spacing=5, align="center")

    d.text((70, 35), "当前工程样机总体数据流", font=font_title, fill="#111827")
    rect((85, 165, 420, 700), "#ecfdf5", "#047857")
    text_center((85, 165, 420, 245), "感知与交互模块", font_regular, "#065f46")
    for idx, label in enumerate(["TMP117 接触温度参考", "MAX30102 PPG 信号", "ADXL362 运动数据", "OV2640 本地灰度预览", "麦克风/扬声器", "本地快门键"]):
        y = 270 + idx * 66
        rect((125, y, 380, y + 52), "white", "#86efac", 2, 10)
        text_center((125, y, 380, y + 52), label, font_small)

    rect((605, 245, 975, 600), "#eef2ff", "#1d4ed8", 5)
    text_center((605, 245, 975, 600), "ESP32-S3\n采集 / 调度 / 状态组包\nTFT Dashboard\nTF 本地日志", font_title, "#1e3a8a")
    rect((1140, 175, 1455, 345), "#fdf2f8", "#be185d")
    text_center((1140, 175, 1455, 345), "Air780E\n4G / MQTT\n定位相关链路", font_regular, "#831843")
    rect((1140, 455, 1455, 650), "#fefce8", "#a16207")
    text_center((1140, 455, 1455, 650), "TF 卡\n/careguard/telemetry.csv\n10 分钟 tfRows=118\nTF Log OK", font_small, "#713f12")
    for xy, label in [
        ((1560, 150, 1730, 300), "CareGuard\nLive Web"),
        ((1560, 365, 1730, 520), "SmartCollar\n微信小程序"),
        ((1560, 585, 1730, 735), "现场答辩\nTFT 演示"),
    ]:
        rect(xy, "#f8fafc", "#334155")
        text_center(xy, label, font_small)

    def arrow(start, end, label, color="#334155", off=(0, 0)):
        d.line([start, end], fill=color, width=5)
        ex, ey = end
        ang = math.atan2(end[1] - start[1], end[0] - start[0])
        pts = [(ex, ey), (ex - 22 * math.cos(ang - 0.45), ey - 22 * math.sin(ang - 0.45)), (ex - 22 * math.cos(ang + 0.45), ey - 22 * math.sin(ang + 0.45))]
        d.polygon(pts, fill=color)
        mx, my = (start[0] + end[0]) / 2 + off[0], (start[1] + end[1]) / 2 + off[1]
        d.rounded_rectangle((mx - 150, my - 24, mx + 150, my + 24), radius=10, fill="white", outline=color, width=2)
        text_center((mx - 150, my - 24, mx + 150, my + 24), label, font_small, color)

    arrow((420, 430), (605, 430), "I²C / SPI / DVP / ADC", "#047857", (0, -45))
    arrow((975, 330), (1140, 260), "MQTT 上报", "#be185d", (5, -45))
    arrow((975, 520), (1140, 555), "本地落盘", "#a16207", (0, 42))
    arrow((1455, 245), (1560, 225), "展示", "#334155", (0, -38))
    arrow((1455, 260), (1560, 440), "展示", "#334155", (0, 35))
    arrow((975, 420), (1560, 660), "TFT / 按键交互", "#1d4ed8", (50, 22))
    d.text((70, 790), "边界：当前系统为工程验证原型；图像仅本地即时预览，不保存、不上传；不包含医疗诊断结论。", font=font_small, fill="#334155")
    im.save(path, quality=95)


def set_cell_text(cell, text, size=10.5, bold=False):
    cell.text = text
    for p in cell.paragraphs:
        set_para_format(p, size=size, cn="宋体", first_indent=False)
        for r in p.runs:
            set_run_font(r, size, "宋体", bold=bold)


def add_body(doc, page_map=None):
    page_map = page_map or {}
    add_para(doc, "目录", size=16, cn="黑体", align=WD_ALIGN_PARAGRAPH.CENTER, first_indent=False)
    toc_items = [
        ("四、项目成果", "achievements"),
        ("五、项目研究结题报告（技术补强审阅版）", "report"),
        ("一、研究背景与工程目标", "bg"),
        ("二、研究内容与方法", "method"),
        ("三、研究结果", "result"),
        ("四、预期目标完成情况", "targets"),
        ("五、经费使用情况", "funding"),
        ("六、参考文献", "refs"),
    ]
    for title, key in toc_items:
        page = page_map.get(key, "待更新")
        p = add_para(doc, f"{title}    {page}", size=12, first_indent=False)
        p.alignment = WD_ALIGN_PARAGRAPH.LEFT
    add_para(doc, "", size=12, first_indent=False).add_run().add_break(WD_BREAK.PAGE)

    add_heading1(doc, "四、项目成果")
    add_caption(doc, "表 1 项目阶段成果清单")
    add_table(
        doc,
        ["序号", "名称", "说明"],
        [
            ["1", "ESP32-S3 智能宠物项圈工程样机", "在早期 STM32 原型验证基础上，完成 ESP32-S3 主控、多传感器、TFT、TF 卡、4G 模块和相机链路的现场联调。"],
            ["2", "CareGuard Live Web 展示端", "用于展示遥测数据、设备状态和定位相关信息，作为端云链路演示入口。"],
            ["3", "SmartCollar 微信小程序演示端", "用于移动端查看设备与宠物状态，配合现场演示验证基础交互流程。"],
            ["4", "TF 遥测数据持续存储与本地灰度即时预览", "TF 卡写入 /careguard/telemetry.csv；实体按键触发 160×120、19200 B 灰度预览，图像不保存、不上传。"],
        ],
        [1.2, 4.4, 9.5],
    )
    add_para(doc, "", size=12, first_indent=False).add_run().add_break(WD_BREAK.PAGE)

    add_heading1(doc, "五、项目研究结题报告（技术补强审阅版）")
    p = add_para(doc, "宠爱云护智能宠物项圈系统", size=22, cn="黑体", align=WD_ALIGN_PARAGRAPH.CENTER, first_indent=False)
    p.paragraph_format.line_spacing_rule = WD_LINE_SPACING.SINGLE
    add_para(doc, "未来技术学院  陈利奇 等", size=14, cn="楷体", align=WD_ALIGN_PARAGRAPH.CENTER, first_indent=False)
    add_para(doc, "指导教师：李乾坤", size=14, cn="楷体", align=WD_ALIGN_PARAGRAPH.CENTER, first_indent=False)
    add_para(
        doc,
        "摘要：项目在早期 STM32 原型验证基础上，迭代采用 ESP32-S3 作为当前工程原型主控，围绕宠物项圈的多参数感知、端云通信和本地交互完成工程样机搭建。当前样机已联调 TMP117、MAX30102、ADXL362、Air780E、OV2640、音频、TFT 与 TF 卡等模块，可实现接触温度参考、PPG 与运动信号采集、4G/MQTT 上报、TFT Dashboard 显示、TF 遥测写入和本地灰度即时预览。现场验证中，TF 卡持续写入 /careguard/telemetry.csv，10 分钟运行记录 tfRows=118，状态为 TF Log OK；实体按键可触发 OV2640 采集 160×120、19200 B 灰度帧并显示到 TFT，约 5 秒返回 Dashboard。系统定位为工程验证原型，不作为医疗诊断设备；PPG 宠物佩戴标定、低功耗曲线、长期续航和连续运行统计仍需后续完成。",
        size=10.5,
        first_indent=False,
    )
    add_para(doc, "关键词：智能宠物项圈；ESP32-S3；多传感器联调；TF 数据记录；本地灰度预览", size=10.5, first_indent=False)

    add_heading1(doc, "一、研究背景与工程目标")
    add_para(doc, "宠物项圈形态的智能设备需要在体积、供电、佩戴稳定性和无线连接之间折中。项目目标不是直接形成诊断类设备，而是先完成一套可演示、可记录、可复现实验现象的工程原型：项圈端采集温度、PPG、运动、音频和图像相关数据，经 4G/MQTT 上报到展示端，同时保留本地 TFT 与 TF 卡证据链。硬件平台采用 ESP32-S3，利用其 GPIO、I²C、SPI、UART、SDMMC、DVP 等外设资源连接各功能模块[1]。")
    add_para(doc, "项目的工程边界按实机验证结果确定。TMP117 仅作为接触温度或皮肤附近温度参考[2]，MAX30102 用于 PPG 与心率相关信号采集[3]，ADXL362 用于运动状态采集[4]，Air780E 承担 4G/MQTT 与定位相关链路[5]，OV2640 当前只承担本地灰度即时预览[6]。这些功能支撑现场答辩演示和后续迭代，不包含血氧诊断、医学体温判定、连续医疗级监测或云端图像留存。")

    add_heading1(doc, "二、研究内容与方法")
    add_heading2(doc, "1. 系统总体架构与数据流")
    add_para(doc, "系统由项圈端、通信链路和展示端构成。项圈端以 ESP32-S3 为中心，采集 TMP117、MAX30102、ADXL362、麦克风和 OV2640 等模块数据，并通过 TFT 显示 Dashboard；本地证据由 TF 卡记录遥测 CSV，远端链路通过 Air780E 上报 MQTT 消息，展示端包括 Web 页面和微信小程序演示界面。通信协议采用 MQTT 发布订阅模型组织设备数据上报和前端展示[8]。")
    add_picture(doc, IMG / "current_dataflow_diagram.png", 12.2, "图 1 当前工程样机总体数据流示意图")

    add_heading2(doc, "2. 硬件连接方案与总线分配")
    add_para(doc, "最终样机接口分配以当前工程 firmware/esp32-s3/include/Pins.h 和对应固件为准。旧版 DOC 中的原理图、PCB Layout 和结构图作为早期硬件资料迁移到审阅稿中，用于核对模块布局与设计演进；若图中个别网络名或历史器件型号与最终固件不一致，以表 2 的 GPIO 分配为准。")
    gpio_rows = [
            ["TMP117", "I²C", "SDA=GPIO10，SCL=GPIO9，地址0x48", "接触温度/皮肤附近温度参考"],
            ["MAX30102", "I²C", "SDA=GPIO10，SCL=GPIO9，地址0x57", "PPG 与心率相关信号采集"],
            ["OV2640 SCCB", "I²C", "SIOD=GPIO10，SIOC=GPIO9，地址0x30", "与 TMP117、MAX30102 共享控制总线"],
            ["OV2640 DVP", "DVP", "D0–D7=GPIO34、48、47、33、35、37、38、39；PCLK=GPIO36；VSYNC=GPIO41；HREF=GPIO40", "仅用于本地灰度预览"],
            ["ADXL362", "SPI", "MOSI=GPIO12，MISO=GPIO11，SCLK=GPIO13，CS=GPIO2", "三轴运动数据采集"],
            ["TFT", "SPI", "MOSI=GPIO12，MISO=GPIO11，SCLK=GPIO13，CS=GPIO7，DC=GPIO8，BL=GPIO5", "Dashboard 与灰度预览"],
            ["TF 卡", "SDMMC 1-bit", "CLK=GPIO16，CMD=GPIO21，DATA0=GPIO15，CD=GPIO14", "写入 telemetry CSV"],
            ["Air780E", "UART/控制", "RX=GPIO18，TX=GPIO17，EN=GPIO6，PWRON=GPIO4", "4G/MQTT 与定位相关链路"],
            ["麦克风", "ADC", "GPIO3", "环境声音采集"],
            ["扬声器", "PWM/控制", "EN=GPIO42，输出GPIO46、GPIO45", "提示音输出"],
            ["本地快门键", "GPIO", "GPIO4，低电平触发", "与 Air780E PWRON 复用；由启动时序区分"],
        ]
    add_caption(doc, "表 2 最终样机 GPIO 与接口分配")
    add_table(
        doc,
        ["模块", "接口/总线", "ESP32-S3 引脚", "连接说明"],
        gpio_rows[:6],
        [2.2, 2.3, 6.5, 4.1],
    )
    add_para(doc, "", size=12, first_indent=False).add_run().add_break(WD_BREAK.PAGE)
    add_caption(doc, "表 2（续）最终样机 GPIO 与接口分配")
    add_table(
        doc,
        ["模块", "接口/总线", "ESP32-S3 引脚", "连接说明"],
        gpio_rows[6:],
        [2.2, 2.3, 6.5, 4.1],
    )
    add_picture(doc, IMG / "final_connection_diagram.png", 12.8, "图 2 最终样机模块连接与总线分配示意图")
    add_para(doc, "共享资源处理是本轮技术补强的重点。GPIO10/9 构成 I²C 共享总线，挂载 TMP117（0x48）、MAX30102（0x57）和 OV2640 SCCB（0x30）；GPIO12/11/13 构成 SPI 共享总线，ADXL362 与 TFT 通过独立 CS 区分；TF 卡使用独立 SDMMC 1-bit 总线，不与显示屏争用 SPI。相机初始化会占用 SCCB/I²C 资源，固件在相机初始化后恢复 Arduino I²C 总线，避免 TMP117 出现读取超时。图像链路只做即时显示，framebuffer 使用后立即释放，不写入 TF，也不上云。")
    add_picture(doc, IMG / "old_schematic_core_1.png", 12.5, "图 3 早期硬件设计原理图局部（供审阅，最终接口分配以表 2 为准）")
    add_picture(doc, IMG / "old_pcb_layout.png", 12.5, "图 4 早期 PCB Layout 局部（供审阅，需人工核对版本与最终样机一致性）")

    add_heading2(doc, "3. 多模态采集与固件实现")
    add_para(doc, "温度链路采用 TMP117 读取接触温度参考值，报告中不将其称为医学诊断体温。PPG 链路采用 MAX30102 红光与红外通道采样，固件依据候选脉搏峰之间的时间间隔估计心率相关值，对连续候选结果采用中值与指数滑动平均，降低偶发跳变对 Dashboard 的影响。受样机形态与试验条件限制，当前结果仅作工程原型状态参考，真实宠物佩戴标定、运动伪影抑制和血氧相关结论仍待后续验证。")
    add_para(doc, "运动链路由 ADXL362 提供三轴运动数据，配合 PPG 与温度数据形成基本状态观测。音频链路包含 GPIO3 麦克风采样与 GPIO42/46/45 扬声器提示音输出。图像链路采用 esp32-camera 组件完成 OV2640 灰度帧获取[7]，配置为 QQVGA 灰度路径，当前固件中 XCLK=-1；实体按键触发后，TFT 显示 160×120、19200 B 灰度预览，约 5 秒返回 Dashboard。")

    add_heading2(doc, "4. 存储、通信与展示实现")
    add_para(doc, "本地存储链路使用 TF 卡记录遥测数据，目标文件为 /careguard/telemetry.csv。10 分钟实机运行中，状态显示 TF Log OK，遥测行数 tfRows=118，说明传感器采集、状态组包和文件追加写入在该时段内保持连续。该数据属于阶段性工程验证，不扩展为长期稳定性统计。")
    add_para(doc, "远端展示链路由 Air780E 连接蜂窝网络并执行 MQTT 上报，Web 与微信小程序用于展示设备状态、传感器数据和定位相关信息。当前已完成现场联调与功能演示，尚未开展连续运行成功率、端到端时延和多设备并发的系统统计。")
    add_picture(doc, IMG / "old_web_dashboard.png", 12.0, "图 5 Web 展示端截图素材（来自旧版报告，供审阅）")
    add_picture(doc, IMG / "old_miniprogram_screens.png", 11.5, "图 6 微信小程序演示端截图素材（来自旧版报告，供审阅）")

    add_heading1(doc, "三、研究结果")
    add_heading2(doc, "1. 硬件原型与多模块联调")
    add_para(doc, "项目已形成以 ESP32-S3 为主控的工程样机，完成 TMP117、MAX30102、ADXL362、Air780E、OV2640、音频、TFT 和 TF 卡等模块联调。早期 STM32 原型保留为验证阶段成果，当前正文中的功能边界均按 ESP32-S3 样机描述。")
    add_picture(doc, IMG / "old_prototype_photo.png", 10.8, "图 7 样机实物照片素材（来自旧版报告，供审阅，模块位置需结合实物再次确认）")
    add_heading2(doc, "2. TF 数据存储、通信与展示")
    add_para(doc, "TF 卡可持续写入 /careguard/telemetry.csv。现场 10 分钟验证中，Dashboard 和遥测字段显示 tfRows=118，状态为 TF Log OK。4G/MQTT、TFT 显示、传感器采集、音频与定位相关链路可用于现场演示，Web 与小程序界面可配合说明端云数据流。")
    add_heading2(doc, "3. 实体按键触发本地灰度预览")
    add_para(doc, "实体按键复用 GPIO4，在启动时序后按低电平触发相机预览。固件当前选择 OV2640 灰度 QQVGA 路径，采集 160×120、19200 B 的本地灰度帧并显示到 TFT，约 5 秒后返回 Dashboard。图像数据不保存到 TF 卡，不上传云端，JPEG 存档链路不属于当前交付功能。")
    add_caption(doc, "表 3 联调过程中的典型问题与处理")
    add_table(
        doc,
        ["问题", "实际现象", "处理方式", "验证结果"],
        [
            ["TF 长文件名不可用", "/careguard/telemetry.csv 创建失败，errno=22", "在项目级配置中启用 FatFs 长文件名支持", "TF Log OK，10 分钟 tfRows=118"],
            ["原始图像采集任务栈不足", "灰度/RGB565 路径触发 cam_task 异常", "使用源码 camera 组件，CONFIG_CAMERA_TASK_STACK_SIZE=4096", "可采集 160×120、19200 B 灰度帧"],
            ["相机初始化影响共享 I²C", "TMP117 出现读取超时", "相机初始化后恢复 I²C 总线", "TMP117、MAX30102 继续正常运行"],
            ["JPEG 图像存档兼容性不足", "JPEG 无法稳定被主机解码", "最终版本取消 JPEG 存档，仅保留灰度即时预览", "图片不保存、不上传，实体按键预览可用"],
        ],
        [3.0, 4.0, 4.5, 4.0],
    )
    add_heading2(doc, "4. 阶段性局限")
    add_para(doc, "当前系统仍处于工程验证阶段。PPG 尚缺少真实宠物佩戴标定，温度链路不是医学诊断体温测量，图像链路仅即时显示而不存档，低功耗策略与任务调度已有设计思路但完整功耗曲线和长期续航测试尚未完成。后续工作应优先补齐长期佩戴稳定性、传感器标定、功耗曲线、连续运行成功率和端到端时延统计。")

    add_heading1(doc, "四、预期目标完成情况")
    add_caption(doc, "表 4 预期目标完成情况")
    add_table(
        doc,
        ["目标", "完成情况", "说明"],
        [
            ["多参数感知", "完成阶段性工程验证", "已完成温度参考、PPG 原始信号、运动、音频和本地图像预览链路联调。"],
            ["端云链路", "完成现场演示", "Air780E 与 MQTT 上报、Web/小程序展示可演示，长期统计待补充。"],
            ["本地交互", "完成", "TFT Dashboard 与实体按键灰度预览可现场演示。"],
            ["低功耗设计", "部分完成/待验证", "已形成策略与任务调度思路，完整功耗曲线和续航实验待开展。"],
            ["真实宠物长期测试", "部分完成/待开展", "当前以样机联调和短时运行验证为主，长期佩戴与标定需后续补充。"],
        ],
        [3.3, 3.5, 7.5],
    )

    add_heading1(doc, "五、经费使用情况")
    add_para(doc, "经费主要用于 ESP32-S3 开发板、传感器模块、Air780E 通信模块、TFT 显示、TF 卡、电源与连接件、样机外壳与调试耗材等。旧版报告中的采购表可继续作为财务附件核对，本审阅稿不新增未经票据支持的采购结论。")

    add_heading1(doc, "六、参考文献")
    refs = [
        "[1] Espressif Systems. ESP32-S3 Series Datasheet[EB/OL]. https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf.",
        "[2] Texas Instruments. TMP117 High-Accuracy, Low-Power, Digital Temperature Sensor Datasheet[EB/OL]. https://www.ti.com/lit/ds/symlink/tmp117.pdf.",
        "[3] Analog Devices. MAX30102 High-Sensitivity Pulse Oximeter and Heart-Rate Sensor for Wearable Health Datasheet[EB/OL]. https://www.analog.com/media/en/technical-documentation/data-sheets/MAX30102.pdf.",
        "[4] Analog Devices. ADXL362 Micropower, 3-Axis, ±2 g/±4 g/±8 g Digital Output MEMS Accelerometer Datasheet[EB/OL]. https://www.analog.com/media/en/technical-documentation/data-sheets/ADXL362.pdf.",
        "[5] 合宙. Air780E 系列模块资料与 AT 指令文档[EB/OL]. https://docs.openluat.com/air780e/.",
        "[6] OmniVision Technologies. OV2640 CameraChip Sensor Datasheet[EB/OL].",
        "[7] Espressif Systems. esp32-camera Component Documentation and Source Code[EB/OL]. https://github.com/espressif/esp32-camera.",
        "[8] OASIS. MQTT Version 3.1.1 Standard[EB/OL]. https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html.",
    ]
    for ref in refs:
        p = add_para(doc, ref, size=10.5, first_indent=False)
        p.alignment = WD_ALIGN_PARAGRAPH.LEFT
        p.paragraph_format.left_indent = Pt(21)
        p.paragraph_format.first_line_indent = Pt(-21)


def export_pdf(docx_path: Path, out_dir: Path):
    out_dir.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        ["soffice", "--headless", "--convert-to", "pdf", "--outdir", str(out_dir), str(docx_path)],
        check=True,
        cwd=str(ROOT),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    produced = out_dir / (docx_path.stem + ".pdf")
    if produced != OUT_PDF:
        shutil.copy2(produced, OUT_PDF)


def extract_page_map(pdf: Path) -> dict[str, str]:
    txt = WORK / "review_pages.txt"
    subprocess.run(["pdftotext", "-layout", str(pdf), str(txt)], check=True)
    raw = txt.read_text(errors="ignore")
    pages = raw.split("\f")
    targets = {
        "achievements": "四、项目成果",
        "report": "五、项目研究结题报告",
        "bg": "一、研究背景与工程目标",
        "method": "二、研究内容与方法",
        "result": "三、研究结果",
        "targets": "四、预期目标完成情况",
        "funding": "五、经费使用情况",
        "refs": "六、参考文献",
    }
    out = {}
    for key, phrase in targets.items():
        for i, page in enumerate(pages, start=1):
            if i <= 5:
                continue
            if phrase in page:
                out[key] = str(i)
                break
    return out


def build_docx(page_map=None):
    draw_connection_diagram(IMG / "final_connection_diagram.png")
    draw_dataflow_diagram(IMG / "current_dataflow_diagram.png")
    if OUT_DOCX.exists():
        OUT_DOCX.unlink()
    shutil.copy2(BASE_DOCX, OUT_DOCX)
    doc = Document(str(OUT_DOCX))
    doc.core_properties.title = "宠爱云护智能宠物项圈系统——技术整合审阅版"
    doc.core_properties.subject = "哈尔滨工业大学大一年度项目结题报告技术补强审阅稿"
    doc.core_properties.author = "陈利奇"
    doc.core_properties.comments = "技术整合审阅版"
    set_defaults(doc)
    strip_from_toc(doc)
    add_body(doc, page_map=page_map)
    doc.save(str(OUT_DOCX))


def write_md(page_count: str, page_map: dict[str, str]):
    def mtime(p: Path):
        return subprocess.check_output(["stat", "-f", "%Sm", "-t", "%Y-%m-%d %H:%M:%S", str(p)], text=True).strip()

    def workspace_path(p: Path):
        return p.relative_to(ROOT).as_posix()

    md = f"""# 技术整合审阅版说明

## 输入文件定位

- 答辩修订版 DOCX：`{workspace_path(BASE_DOCX)}`，修改时间：{mtime(BASE_DOCX)}
- 旧版 DOC：`{workspace_path(OLD_DOC)}`，修改时间：{mtime(OLD_DOC)}
- 未找到精确文件名 `大一年度项目结题报告(2)_答辩修订版(1).docx`；本次按同目录最接近且已存在的答辩修订版继续整合。

## 输出文件

- DOCX：`{workspace_path(OUT_DOCX)}`
- PDF：`{workspace_path(OUT_PDF)}`
- 最终页数：{page_count}

## 从旧版迁移的图像素材

- 早期硬件设计原理图局部：作为原理图素材迁移，正文已注明最终接口分配以表 2 为准。
- 早期 PCB Layout 局部：作为 PCB 设计素材迁移，需要人工核对版本。
- 样机实物照片：作为样机外观与结构素材迁移，模块位置需结合实物再次确认。
- Web 展示端截图、微信小程序演示端截图：作为已做展示端的界面素材迁移。

## 新增图表

- 表 2 最终样机 GPIO 与接口分配：依据 `firmware/esp32-s3/include/Pins.h` 与当前固件整理。
- 图 1 当前工程样机总体数据流示意图：替代旧版中含历史通信模块型号的架构图。
- 图 2 最终样机模块连接与总线分配示意图：自绘 GPIO/总线连接图，不作为完整电源原理图。
- 表 3 联调过程中的典型问题与处理：记录 TF 长文件名、相机任务栈、I²C 恢复、JPEG 存档降级等工程问题。
- 表 4 预期目标完成情况：区分已完成演示、部分完成和待验证项。

## 需要人工核实的内容

- 旧版原理图、PCB Layout 与最终 ESP32-S3 样机是否同一硬件版本。
- 旧版系统架构图含 `Air780EG/EC800K` 历史型号标注，未直接插入正文；若要保留旧图，需要人工确认或重绘为 Air780E。
- 旧版样机照片中各模块的可见位置，必要时可补拍清晰标注图。
- Web 与小程序截图是否为今晚希望保留的最终界面；若界面已更新，应补换新截图。
- 供电电压、电源路径、电池与充电相关关系未在正文扩写，原因是本轮未从最终固件和图像中得到足够清晰证据。
- `OV2640` 数据手册条目的 URL/版本信息建议最终提交前再由人工补齐或换成可核验来源。

## 参考文献处理

- 保留并按正文首次出现顺序引用：ESP32-S3、TMP117、MAX30102、ADXL362、Air780E、OV2640、esp32-camera、MQTT。
- 未保留泛化市场资料、无法对应正文的论文和旧版中支撑不足的效果类引用。
- 团队实测结果放入正文与表格，不作为外部参考文献。

## 旧版表述未迁移及原因

- 卡尔曼滤波实时验证、76.3% 波动下降：当前 ESP32-S3 工程未形成相应实机统计证据。
- Stop 0.048 W、72 h+ 续航、性能提升 30%：未见完整功耗曲线和长期续航试验。
- 体温偏差 `<0.15 ℃`、定位 CEP50 `<4.8 m`：未见对应标定流程和样本统计。
- MQTT 4 小时、7200 条、99.94% 成功率、P50/P99 延迟：当前仅保留现场演示和短时验证边界。
- 真实宠物佩戴压力、3D 扫描、SLA 感压纸压力：未见足够佐证材料。
- JPEG 图像存档、云端图像上传：最终固件已降级为本地灰度即时预览，图片不保存、不上传。
- 血氧、医学诊断、核心体温等结论：与当前工程验证原型边界不一致。

## 页码索引检查

{os.linesep.join(f'- {k}: 第 {v} 页' for k, v in page_map.items())}
"""
    OUT_MD.write_text(md, encoding="utf-8")


def main():
    build_docx(page_map=None)
    export_pdf(OUT_DOCX, RENDER)
    page_map = extract_page_map(OUT_PDF)
    build_docx(page_map=page_map)
    export_pdf(OUT_DOCX, RENDER)
    info = subprocess.check_output(["pdfinfo", str(OUT_PDF)], text=True)
    m = re.search(r"Pages:\s+(\d+)", info)
    page_count = m.group(1) if m else "未知"
    page_map = extract_page_map(OUT_PDF)
    write_md(page_count, page_map)
    print(f"DOCX={OUT_DOCX}")
    print(f"PDF={OUT_PDF}")
    print(f"MD={OUT_MD}")
    print(f"PAGES={page_count}")
    print("PAGE_MAP=", page_map)


if __name__ == "__main__":
    main()
