/* XiangShan alignment score history: data + builders.
 *
 * Build-time tool, not page script: site/tools/render-scorecard.js writes the
 * static SVG and table into site/learn.html and site/learn.zh.html between the
 * `score-history` markers.  Add a point here, run the renderer, and update
 * docs/scorecard-history.md whenever the scorecard is re-reviewed.
 */
(function (root) {
  "use strict";

  var HISTORY = {
    target: 65,
    points: [
      {
        date: "2026-11-15",
        total: 34.4,
        grade: "D+",
        summary: {
          zh: "首版评分：网站上线时的八维基线",
          en: "First scorecard: the eight-dimension baseline published with the site"
        },
        changes: {
          zh: [
            "参考模型 8.5、锁步差分 4.0、CI 门禁 1.5",
            "生成化 0、检查点 4.0、性能 2.5、后端 2.0、协作 5.0"
          ],
          en: [
            "Reference 8.5, DiffTest 4.0, CI 1.5",
            "Generation 0, checkpoints 4.0, performance 2.5, backend 2.0, collaboration 5.0"
          ]
        }
      },
      {
        date: "2026-11-16",
        total: 46.8,
        grade: "C",
        summary: {
          zh: "am6502 M1–M3 落地；根 MIT 许可证发布",
          en: "am6502 M1–M3 landed; root MIT licence published"
        },
        changes: {
          zh: [
            "锁步差分 4.0 → 6.5：同一镜像在 NEMU 与 RTL 上逐条 retire 比对",
            "CI 门禁 1.5 → 4.5：am6502.yml 跑平坦镜像、卡带三后端与 DiffTest",
            "参考模型 8.5 → 9.0：am6502 的镜像由构建产出且逐字节可复现",
            "开源协作 5.0 → 8.0：根 MIT 许可证与 THIRD_PARTY_NOTICES 已发布"
          ],
          en: [
            "DiffTest 4.0 → 6.5: the same image retires instruction by instruction on NEMU and RTL",
            "CI 1.5 → 4.5: am6502.yml runs flat images, the cartridge on three backends and the DiffTest",
            "Reference 8.5 → 9.0: am6502 images are built reproducibly",
            "Collaboration 5.0 → 8.0: root MIT licence and THIRD_PARTY_NOTICES published"
          ]
        }
      },
      {
        date: "2026-11-22",
        total: 49.3,
        grade: "C",
        summary: {
          zh: "Lean 4 形式化验证落地；八维核心 46.8 不变，形式化加分 +2.5",
          en: "Lean 4 formal verification landed; eight-dimension core stays at 46.8, +2.5 formal bonus"
        },
        changes: {
          zh: [
            "形式化加分 0 → +2.5（上限 +5）：151 条指令译码与 RTL 分派一致，19 条指令逐周期精化（mrun n m = step m）",
            "ADC/SBC（含 BCD）、CMP、移位、BIT 对全部 8 位操作数证明；15 个程序测试、4 个 Lean↔RTL 协同仿真进 CI",
            "八维核心 46.8 不变：生成化 0、帧级/总线比对与后端闭环仍缺"
          ],
          en: [
            "Formal bonus 0 → +2.5 (cap +5): the 151-opcode decode matches the RTL dispatch and 19 instructions have mrun n m = step m refinement proofs",
            "ADC/SBC (including BCD), CMP, shifts and BIT proved for all 8-bit operands; 15 program tests and 4 Lean↔RTL co-simulations run in CI",
            "Eight-dimension core unchanged at 46.8: generation 0, frame/bus compare and the backend loop are still missing"
          ]
        }
      }
    ]
  };

  function chartSvg(points, lang) {
    var W = 720, H = 260, L = 54, R = 54, T = 22, B = 48;
    var pw = W - L - R, ph = H - T - B;
    var x = function (i) {
      return points.length === 1 ? L + pw / 2 : L + pw * i / (points.length - 1);
    };
    var y = function (v) { return T + ph * (1 - v / 100); };

    var out = [];
    out.push('<svg viewBox="0 0 ' + W + ' ' + H + '" role="img" aria-label="' +
      (lang === "zh" ? "香山对齐评分历史曲线" : "XiangShan alignment score history") + '">');

    [0, 20, 40, 60, 80, 100].forEach(function (v) {
      out.push('<line class="history-grid" stroke="#26303f" stroke-width="1" x1="' + L + '" y1="' + y(v) +
        '" x2="' + (W - R) + '" y2="' + y(v) + '"/>');
      out.push('<text class="history-axis" fill="#7e8da2" font-size="9" font-family="Menlo,ui-monospace,monospace" x="' + (L - 10) + '" y="' + (y(v) + 3) +
        '" text-anchor="end">' + v + '</text>');
    });

    out.push('<line class="history-target" stroke="#f4c66a" stroke-width="1.2" stroke-dasharray="5 5" opacity="0.7" x1="' + L + '" y1="' + y(HISTORY.target) +
      '" x2="' + (W - R) + '" y2="' + y(HISTORY.target) + '"/>');
    out.push('<text class="history-target-label" fill="#f4c66a" font-size="9" font-family="Menlo,ui-monospace,monospace" x="' + (W - R) + '" y="' +
      (y(HISTORY.target) - 7) + '" text-anchor="end">' +
      (lang === "zh" ? "12 周目标 65–68" : "12-week target 65–68") + '</text>');

    if (points.length > 1) {
      var line = points.map(function (p, i) { return x(i) + "," + y(p.total); }).join(" ");
      out.push('<polyline class="history-line" fill="none" stroke="#ff4d57" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round" points="' + line + '"/>');
    }

    points.forEach(function (p, i) {
      var last = i === points.length - 1;
      var first = i === 0;
      // Keep the end labels inside the viewBox: first anchors start, last end.
      var scoreX = x(i) + (first && !last ? 10 : last && !first ? -10 : 0);
      var scoreAnchor = first && !last ? "start" : last && !first ? "end" : "middle";
      var dateAnchor = first && !last ? "start" : last && !first ? "end" : "middle";
      out.push('<circle class="history-dot' + (last ? " history-dot--last" : "") +
        '" fill="' + (last ? "#f4c66a" : "#06080c") + '" stroke="' +
        (last ? "#f4c66a" : "#ff4d57") + '" stroke-width="2" cx="' + x(i) +
        '" cy="' + y(p.total) + '" r="' + (last ? 6.5 : 4.8) + '"/>');
      out.push('<text class="history-score" fill="#e9eff8" font-size="11" font-weight="600" font-family="Menlo,ui-monospace,monospace" x="' + scoreX + '" y="' + (y(p.total) - 13) +
        '" text-anchor="' + scoreAnchor + '">' + p.total.toFixed(1) + '</text>');
      out.push('<text class="history-date" fill="#7e8da2" font-size="10" font-family="Menlo,ui-monospace,monospace" x="' + x(i) + '" y="' + (H - 20) +
        '" text-anchor="' + dateAnchor + '">' + p.date + '</text>');
    });

    out.push('</svg>');
    return out.join("");
  }

  function tableHtml(points, lang) {
    var head = lang === "zh"
      ? ["日期", "总分", "变化", "等级", "这次改了什么"]
      : ["Date", "Total", "Delta", "Grade", "What changed"];
    var rows = points.map(function (p, i) {
      var prev = i > 0 ? points[i - 1].total : null;
      var delta = prev === null ? "—" :
        (p.total - prev >= 0 ? "+" : "") + (p.total - prev).toFixed(1);
      var items = p.changes[lang].map(function (c) { return "<li>" + c + "</li>"; }).join("");
      return '<tr><td class="history-date-cell">' + p.date + '</td>' +
        '<td class="history-total">' + p.total.toFixed(1) + '</td>' +
        '<td>' + delta + '</td><td>' + p.grade + '</td>' +
        '<td><strong>' + p.summary[lang] + '</strong>' +
        '<ul class="history-changes">' + items + '</ul></td></tr>';
    }).join("");
    return '<table class="doc-table history-table"><thead><tr>' +
      head.map(function (h) { return "<th>" + h + "</th>"; }).join("") +
      '</tr></thead><tbody>' + rows + '</tbody></table>';
  }

  module.exports = {
    HISTORY: HISTORY,
    chartSvg: chartSvg,
    tableHtml: tableHtml
  };
})(typeof globalThis !== "undefined" ? globalThis : this);
