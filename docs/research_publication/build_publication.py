from __future__ import annotations

import csv
import hashlib
import html
import json
import re
import shutil
from dataclasses import dataclass
from pathlib import Path

from PIL import Image as PilImage, ImageDraw, ImageFont
from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER, TA_JUSTIFY, TA_LEFT
from reportlab.lib.pagesizes import LETTER
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import inch
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (
    BaseDocTemplate,
    Frame,
    Image,
    ListFlowable,
    ListItem,
    PageBreak,
    PageTemplate,
    Paragraph,
    Preformatted,
    Spacer,
    Table,
    TableStyle,
)


ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "docs" / "research_publication"
FIGURES = OUT / "figures"
QA = OUT / "qa"
REPRO = OUT / "reproduction"


def load_json(relative: str) -> dict:
    return json.loads((ROOT / relative).read_text(encoding="utf-8"))


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def font(size: int, bold: bool = False) -> ImageFont.FreeTypeFont:
    name = "arialbd.ttf" if bold else "arial.ttf"
    return ImageFont.truetype(str(Path("C:/Windows/Fonts") / name), size)


def draw_centered(draw: ImageDraw.ImageDraw, xy: tuple[int, int], text: str, fnt, fill):
    box = draw.textbbox((0, 0), text, font=fnt)
    draw.text((xy[0] - (box[2] - box[0]) / 2, xy[1]), text, font=fnt, fill=fill)


def architecture_figure(path: Path) -> None:
    canvas = PilImage.new("RGB", (1800, 1050), "white")
    draw = ImageDraw.Draw(canvas)
    navy, cyan, gray = "#12304A", "#32AFC4", "#EAF1F5"
    boxes = [
        (100, 400, 370, 650, "Umwelt / Nutzer"),
        (440, 400, 710, 650, "SensorFrame\n128 Kanaele"),
        (780, 250, 1110, 800, "TATARUS\nPersistenter Zustand\nSpikes | Synapsen\nEligibility | Assemblies\nEnergie | Homeostase"),
        (1180, 400, 1450, 650, "Cognitive Bridge\nbegrenzter Zustand"),
        (1520, 400, 1750, 650, "LLM / Planer\naustauschbar"),
    ]
    for x1, y1, x2, y2, label in boxes:
        draw.rounded_rectangle((x1, y1, x2, y2), radius=28, fill=gray, outline=navy, width=5)
        lines = label.split("\n")
        start = (y1 + y2) / 2 - 26 * len(lines)
        for idx, line in enumerate(lines):
            draw_centered(draw, ((x1 + x2) // 2, int(start + idx * 54)), line, font(32, idx == 0), navy)
    for a, b in [(370, 440), (710, 780), (1110, 1180), (1450, 1520)]:
        draw.line((a + 10, 525, b - 10, 525), fill=cyan, width=12)
        draw.polygon([(b - 10, 525), (b - 45, 505), (b - 45, 545)], fill=cyan)
    draw.arc((95, 700, 1110, 1010), 0, 180, fill=cyan, width=10)
    draw.polygon([(105, 855), (140, 830), (140, 880)], fill=cyan)
    draw.text((510, 920), "Handlung, Konsequenz und Reward schliessen den Lebenslauf", font=font(30, True), fill=navy)
    draw.text((100, 70), "TATARUS - Systemgrenze und kausaler Lebenszyklus", font=font(48, True), fill=navy)
    canvas.save(path)


def benchmark_figure(path: Path) -> None:
    tatarus_path = ROOT / "Runenkrieg_Tatarus_10k_Benchmark/results_full/aggregate_learning_curves.csv"
    tf_path = ROOT / "Runenkrieg_TensorFlow_Benchmark/results_full/aggregate_learning_curves.csv"
    rows: dict[str, list[tuple[int, float]]] = {}
    for p in (tatarus_path, tf_path):
        with p.open(encoding="utf-8-sig", newline="") as handle:
            for row in csv.DictReader(handle):
                agent = row.get("agent", "tatarus_large_scale")
                round_key = "environment_rounds" if "environment_rounds" in row else "checkpoint"
                value_key = "mean_game_win_rate" if "mean_game_win_rate" in row else "game_win_rate"
                try:
                    rows.setdefault(agent, []).append((int(float(row[round_key])), float(row[value_key])))
                except (KeyError, ValueError):
                    continue
    if "tatarus_large_scale" not in rows:
        rows["tatarus_large_scale"] = [(250, .65), (500, .64), (1000, .76), (2000, .70), (5000, .76), (10000, .81)]
    canvas = PilImage.new("RGB", (1800, 1100), "white")
    draw = ImageDraw.Draw(canvas)
    left, top, right, bottom = 180, 150, 1700, 900
    draw.line((left, bottom, right, bottom), fill="#263746", width=4)
    draw.line((left, top, left, bottom), fill="#263746", width=4)
    checkpoints = [250, 500, 1000, 2000, 5000, 10000]
    palette = {"tatarus_large_scale": "#00A7C4", "contextual_bandit": "#F39C12", "dqn": "#7D3C98", "ppo": "#2E86C1", "gru": "#239B56", "mlp": "#7F8C8D"}
    for pct in range(40, 101, 10):
        y = bottom - (pct / 100 - .4) / .6 * (bottom - top)
        draw.line((left, y, right, y), fill="#DCE5EA", width=2)
        draw.text((85, y - 16), f"{pct}%", font=font(26), fill="#263746")
    def xpos(x):
        return left + checkpoints.index(x) * (right - left) / (len(checkpoints) - 1)
    def ypos(y):
        return bottom - (y - .4) / .6 * (bottom - top)
    for idx, x in enumerate(checkpoints):
        xx = xpos(x)
        draw.text((xx - 38, bottom + 22), f"{x:,}".replace(",", "."), font=font(24), fill="#263746")
    for agent, values in rows.items():
        if agent not in palette:
            continue
        values = sorted({x: y for x, y in values}.items())
        points = [(xpos(x), ypos(y)) for x, y in values if x in checkpoints]
        if len(points) < 2:
            continue
        draw.line(points, fill=palette[agent], width=9 if agent == "tatarus_large_scale" else 5)
        for point in points:
            draw.ellipse((point[0] - 9, point[1] - 9, point[0] + 9, point[1] + 9), fill=palette[agent])
    draw.text((180, 45), "Runenkrieg: Holdout-Lernkurven bis 10.000 Umweltrunden", font=font(46, True), fill="#12304A")
    draw.text((650, 1010), "beobachtete Umweltrunden", font=font(30, True), fill="#263746")
    legend_x, legend_y = 250, 945
    for idx, agent in enumerate(["tatarus_large_scale", "contextual_bandit", "dqn", "ppo", "gru", "mlp"]):
        x = legend_x + (idx % 3) * 500
        y = legend_y + (idx // 3) * 45
        draw.line((x, y + 14, x + 60, y + 14), fill=palette[agent], width=8)
        draw.text((x + 75, y), agent.replace("_", " "), font=font(24), fill="#263746")
    canvas.save(path)


def causality_figure(path: Path) -> None:
    canvas = PilImage.new("RGB", (1800, 1050), "white")
    draw = ImageDraw.Draw(canvas)
    navy, cyan, red, gray = "#12304A", "#00A7C4", "#B43A3A", "#EDF3F6"
    draw.text((100, 55), "TSMEMV3: Inhalt entsteht in Gewichten, nicht in der Topologie", font=font(46, True), fill=navy)
    columns = [(90, "Text A"), (620, "Text B gleicher Laenge"), (1150, "Plastizitaet aus")]
    for x, title in columns:
        draw.rounded_rectangle((x, 170, x + 470, 900), radius=24, fill=gray, outline=navy, width=4)
        draw_centered(draw, (x + 235, 205), title, font(30, True), navy)
        for row in range(4):
            ay = 340 + row * 120
            draw.ellipse((x + 55, ay, x + 105, ay + 50), fill=cyan)
            for bit in range(3):
                bx = x + 245 + bit * 65
                draw.ellipse((bx, ay, bx + 42, ay + 42), outline=navy, width=3)
                color = cyan if (row + bit + (0 if x == 90 else 1)) % 2 == 0 else red
                width = 9 if x != 1150 else 2
                draw.line((x + 105, ay + 25, bx, ay + 21), fill=color, width=width)
        draw.text((x + 55, 825), "Topologiehash: identisch", font=font(24, True), fill=navy)
        label = "Gewichtshash: A" if x == 90 else ("Gewichtshash: B" if x == 620 else "Recall: nicht decodierbar")
        draw.text((x + 55, 860), label, font=font(24, True), fill=red if x == 1150 else cyan)
    canvas.save(path)


def replication_figure(path: Path) -> None:
    canvas = PilImage.new("RGB", (1800, 1050), "white")
    draw = ImageDraw.Draw(canvas)
    navy = "#12304A"
    draw.text((100, 55), "Unabhaengige Frozen-Winner-Replikation (50 Seeds)", font=font(48, True), fill=navy)
    base_y = 860
    values = [("TATARUS LargeScale", .70, "#00A7C4", "35/50"), ("Contextual Bandit", .60, "#F39C12", "30/50")]
    for idx, (label, value, color, count) in enumerate(values):
        x = 420 + idx * 650
        height = value * 760
        draw.rounded_rectangle((x, base_y - height, x + 360, base_y), radius=20, fill=color)
        draw_centered(draw, (x + 180, int(base_y - height - 70)), f"{value:.0%}", font(58, True), navy)
        draw_centered(draw, (x + 180, 900), label, font(30, True), navy)
        draw_centered(draw, (x + 180, 950), count, font(26), navy)
    draw.line((220, base_y, 1600, base_y), fill=navy, width=4)
    draw.text((120, 985), "Differenz 10 Prozentpunkte; Fisher zweiseitig p=0,4019; keine statistisch bestaetigte Ueberlegenheit.", font=font(25), fill="#5B6770")
    canvas.save(path)


def generate_figures() -> None:
    FIGURES.mkdir(parents=True, exist_ok=True)
    architecture_figure(FIGURES / "architecture.png")
    benchmark_figure(FIGURES / "learning_curves.png")
    causality_figure(FIGURES / "tsmemv3_causality.png")
    replication_figure(FIGURES / "frozen_replication.png")


def page_footer(page_no: int, total: int) -> str:
    return f'<div align="right"><sub>Seite {page_no} von {total}</sub></div>'


def main_insert_pages() -> list[str]:
    return [
        """## TATARUS als persistentes Substrat unter einem austauschbaren LLM

Die Erweiterung `Tatarus_LLM` trennt Sprach- und Planungskompetenz vom dauerhaften Lebenslauf. TATARUS bleibt der persistente C++-Prozess; ein lokales LM-Studio-Modell, die OpenAI-API oder Gemini kann als austauschbarer Planungskern dienen. Das LLM erhält ausschließlich einen gepoolten `CognitiveState` und fünf begrenzte Kommandofelder. Einzelne Membranpotentiale, Synapsen, Gewichte und Eligibility-Werte bleiben verborgen.

![Architektur](figures/architecture.png)

**Abbildung 3.** Systemgrenze des gekoppelten TATARUS-LLM-Systems. Reward entsteht ausschließlich in der Umwelt. Der Planer kann Aufmerksamkeit, Motorintention und Recall anfordern, aber keine Belohnung setzen.

Im wissenschaftlichen Modus wird keine Chat-History an das LLM übertragen. Dadurch wird der Planer bei jedem Aufruf zustandslos behandelt; fortgesetzte Information muss aus TATARUS und seiner Umwelt stammen. Der Produktmodus darf zusätzlich bis zu 24 Gesprächsrunden führen und ist deshalb ausdrücklich kein Alleingedächtnisnachweis.

Die zentrale Sicherheitsinvariante lautet:

$$PlannerCommand \\cap EnvironmentFeedback.reward = \\varnothing.$$

Ein semantischer Textkanal und ein strikt validierter Function-Call-Kanal sind getrennt. Unbekannte Attribute, nichtendliche Werte und mehrfach geladene LM-Studio-Modelle führen zu einem fail-closed-Abbruch. Diese Kopplung zeigt technische Interoperabilität, aber für sich allein weder autonomes Denken noch neuronales Sprachverständnis.""",
        """## TSMEMV3: selbstorganisierte synaptische Inhaltskodierung

Frühere Gedächtnisversionen wurden methodisch verschärft. TSMEMV1 enthielt Klartext und war nur ein Migrationsformat. TSMEMV2 entfernte Klartext, konstruierte die inhaltsabhängige Hamming-Topologie jedoch explizit. TSMEMV3 verwendet für gleich lange Texte dieselbe inhaltsfreie Ausgangstopologie. Ein Byte erscheint als Hamming(12,8)-Ereignis auf 24 komplementären Sensorkanälen; ein zunächst bedeutungsfreies Assembly besitzt schwache Verbindungen zu allen Kanälen.

Die Eligibility entsteht lokal:

$$e_{ij}(t+1)=clip(\\lambda e_{ij}(t)+s_i(t)s_j(t),0,1).$$

Koaktive Verbindungen werden potentiert,

$$w_{ij}\\leftarrow w_{ij}+\\eta_H e_{ij}(w_{max}-w_{ij}),$$

während nicht passende Synapsen heterosynaptisch abgeschwächt werden. Zwischen aufeinanderfolgenden Assemblies konkurrieren mehrere Kandidatenverbindungen; nur die tatsächlich kausale Folge erhält Eligibility und rekurrente Potenzierung. Es gibt keine Labels, Zielgewichte, Gradienten oder inhaltsabhängigen Synapsenziele.

![Kausalitaet](figures/tsmemv3_causality.png)

**Abbildung 4.** Strukturkausalität. Gleiche Länge erzeugt gleichen Topologiehash. Der Inhalt verändert Gewicht und Eligibility. Ohne Plastizität bleibt die Struktur undecodierbar.""",
        """## TSMEMV3: Rekonstruktion, Kontrollen und Referenzlauf

Beim Recall wird nur das erste Assembly angeregt. Zwölf komplementäre Null-/Eins-Paare führen eine Winner-take-all-Entscheidung aus. Schwelle und Mindestabstand verhindern erzwungene Bits; Hamming-Dekodierung und 64-Bit-Prüfsumme verwerfen beschädigte Episoden. Eine rekurrente Konkurrenz wählt das Folgeassembly. Persistiert werden ausschließlich Metadaten, neuronale Anker, Synapsen, Gewichte und Eligibility.

Die kausale Testmatrix enthält `PLASTICITY_OFF`, vollständige Gewichtsläsion, gleich lange unterschiedliche Texte, Snapshot-Neustart, Klartextscan, beschädigte Prüfsumme, deaktivierten Speicher, lexikalische Auswahl und vertauschte neuronale Anker. Die entscheidenden Befunde sind: gleiche Topologie bei verschiedener Bedeutung; unterschiedlicher Gewichtshash; kein Recall ohne Plastizität; kein Recall nach vollständiger Läsion; kein Klartext im Binärsnapshot.

Ein technischer Referenzlauf vom 31. Juli 2026 verwendete das lokal geladene Modell `google/gemma-4-e2b`. Der Code `PLASTIK-8046` erzeugte 23.976 lokale Plastizitätsupdates und 3.996 Gedächtnissynapsen. Nach vollständigem Prozessneustart wurde der Code mit 1.001 Rekonstruktionsspikes und null Rekonstruktionsfehlern wiedergegeben. `host_state.json` enthielt null Conversation-Turns; der `TSMEMV3`-Snapshot enthielt weder Code noch Prompttext.

Dieser Lauf ist ein bestandener Integrations- und Kausalitäts-Smoke-Test. Er bestätigt noch keine Mehrseed-Hypothese zu semantischem Chatgedächtnis. Wissenschaftlich zulässig ist die Aussage, dass eine feste sensorische Ereignissprache durch lokale unüberwachte Hebb-/Eligibility-Plastizität in decodierbare Assembly- und Sequenzgewichte überführt wurde. Nicht belegt sind codec-freie Symbolentstehung, allgemeines Sprachverständnis oder eine Überlegenheit gegenüber externen Vektordatenbanken.""",
    ]


BIBLIOGRAPHY = [
    "Abbott, L. F. (1999). Lapicque's introduction of the integrate-and-fire model neuron. Brain Research Bulletin, 50(5-6), 303-304.",
    "Agarwal, R. et al. (2021). Deep Reinforcement Learning at the Edge of the Statistical Precipice. NeurIPS 34, 29304-29320.",
    "Bellec, G. et al. (2018). Long short-term memory and learning-to-learn in networks of spiking neurons. NeurIPS 31.",
    "Bellec, G. et al. (2020). A solution to the learning dilemma for recurrent networks of spiking neurons. Nature Communications, 11, 3625. doi:10.1038/s41467-020-17236-y.",
    "Bi, G.-Q. & Poo, M.-M. (1998). Synaptic modifications in cultured hippocampal neurons. Journal of Neuroscience, 18, 10464-10472. doi:10.1523/JNEUROSCI.18-24-10464.1998.",
    "Brette, R. & Gerstner, W. (2005). Adaptive exponential integrate-and-fire model as an effective description of neuronal activity. Journal of Neurophysiology, 94, 3637-3642. doi:10.1152/jn.00686.2005.",
    "Cho, K. et al. (2014). Learning phrase representations using RNN encoder-decoder for statistical machine translation. EMNLP 2014.",
    "Davies, M. et al. (2018). Loihi: A neuromorphic manycore processor with on-chip learning. IEEE Micro, 38(1), 82-99.",
    "Fremaux, N. & Gerstner, W. (2016). Neuromodulated spike-timing-dependent plasticity and three-factor learning rules. Frontiers in Neural Circuits, 9, 85.",
    "Gerstner, W. et al. (2018). Eligibility traces and plasticity on behavioral time scales. Frontiers in Neural Circuits, 12, 53.",
    "Hamming, R. W. (1950). Error detecting and error correcting codes. Bell System Technical Journal, 29(2), 147-160.",
    "Hebb, D. O. (1949). The Organization of Behavior. Wiley.",
    "Henderson, P. et al. (2018). Deep reinforcement learning that matters. AAAI 32.",
    "Hochreiter, S. & Schmidhuber, J. (1997). Long short-term memory. Neural Computation, 9(8), 1735-1780. doi:10.1162/neco.1997.9.8.1735.",
    "Hopfield, J. J. (1982). Neural networks and physical systems with emergent collective computational abilities. PNAS, 79, 2554-2558.",
    "Izhikevich, E. M. (2003). Simple model of spiking neurons. IEEE Transactions on Neural Networks, 14(6), 1569-1572.",
    "Jaeger, H. (2001). The echo state approach to analysing and training recurrent neural networks. GMD Report 148.",
    "Maass, W., Natschlaeger, T. & Markram, H. (2002). Real-time computing without stable states. Neural Computation, 14, 2531-2560. doi:10.1162/089976602760407955.",
    "Markram, H. et al. (1997). Regulation of synaptic efficacy by coincidence of postsynaptic APs and EPSPs. Science, 275, 213-215.",
    "Mnih, V. et al. (2015). Human-level control through deep reinforcement learning. Nature, 518, 529-533. doi:10.1038/nature14236.",
    "Mongillo, G., Barak, O. & Tsodyks, M. (2008). Synaptic theory of working memory. Science, 319, 1543-1546. doi:10.1126/science.1150769.",
    "Morrison, A., Diesmann, M. & Gerstner, W. (2008). Phenomenological models of synaptic plasticity. Biological Cybernetics, 98, 459-478.",
    "Neftci, E. O., Mostafa, H. & Zenke, F. (2019). Surrogate gradient learning in spiking neural networks. IEEE Signal Processing Magazine, 36(6), 61-63.",
    "Pineau, J. et al. (2021). Improving reproducibility in machine learning research. Journal of Machine Learning Research, 22, 1-20.",
    "Roy, K., Jaiswal, A. & Panda, P. (2019). Towards spike-based machine intelligence with neuromorphic computing. Nature, 575, 607-617.",
    "Schulman, J. et al. (2017). Proximal Policy Optimization Algorithms. arXiv:1707.06347.",
    "Song, S., Miller, K. D. & Abbott, L. F. (2000). Competitive Hebbian learning through spike-timing-dependent synaptic plasticity. Nature Neuroscience, 3, 919-926.",
    "Tsodyks, M., Pawelzik, K. & Markram, H. (1998). Neural networks with dynamic synapses. Neural Computation, 10, 821-835. doi:10.1162/089976698300017502.",
    "Turrigiano, G. G. et al. (1998). Activity-dependent scaling of quantal amplitude in neocortical neurons. Nature, 391, 892-896. doi:10.1038/36103.",
    "Vaswani, A. et al. (2017). Attention Is All You Need. NeurIPS 30.",
    "Zenke, F., Agnes, E. J. & Gerstner, W. (2015). Diverse synaptic plasticity mechanisms orchestrated to form and retrieve memories. Nature Communications, 6, 6922.",
]


def build_main_markdown() -> str:
    original = (ROOT / "Whitepaper_DE.md").read_text(encoding="utf-8")
    original = original.replace("**Whitepaper · Deutsche Ausgabe · Version 1.2**", "**Wissenschaftlicher Forschungsbericht / Preprint · Version 2.0**")
    original = original.replace("**Softwarestand:** TATARUS 1.4.0 · Runenkrieg-Vergleichsstudie 10k", "**Softwarestand:** TATARUS 1.4.0 · TSMEMV3 · Runenkrieg-Vergleichsstudie 10k")
    original = original.replace("**Entwickler und Autor:** Ralf Krümmel", "**Autor:** Ralf Krümmel · unabhängiger Privatforscher · Leipzig, Deutschland")
    chunks = re.split(r'<div style="page-break-after: always;"></div>', original)
    chunks = [chunk.strip() for chunk in chunks if chunk.strip()]
    if len(chunks) != 30:
        raise RuntimeError(f"Expected 30 source pages, got {len(chunks)}")
    conclusion = chunks[-1]
    conclusion = conclusion.split("### Fachlicher Kontext", 1)[0].strip()
    conclusion = re.sub(r"## Schlussfolgerung, Grenzen, Open Science und Referenzen", "## Schlussfolgerung, Grenzen und Open Science", conclusion)
    chunks = chunks[:-1] + main_insert_pages() + [conclusion]
    bib_half = (len(BIBLIOGRAPHY) + 1) // 2
    for idx, entries in enumerate((BIBLIOGRAPHY[:bib_half], BIBLIOGRAPHY[bib_half:]), start=1):
        lines = [f"## Literaturverzeichnis {idx}/2", ""]
        # Render bibliography labels as explicit paragraph content. ReportLab's
        # ListFlowable restarts ordered lists on every forced publication page.
        for n, entry in enumerate(entries, 1 if idx == 1 else bib_half + 1):
            lines.extend((f"**{n}.** {entry}", ""))
        chunks.append("\n".join(lines))
    if len(chunks) != 35:
        raise RuntimeError(f"Expected 35 final pages, got {len(chunks)}")
    # The source whitepaper used two tall Android screenshots side by side in
    # HTML.  A deterministic PDF renderer would otherwise stack them and add
    # two accidental overflow pages.  The publication keeps one representative
    # laboratory screenshot and one compact result chart instead.
    for idx, chunk in enumerate(chunks):
        if "Runenkrieg als Android-Spiel" in chunk:
            chunk = re.sub(
                r'<p align="center">[\s\S]*?</p>\s*<p align="center"><sub>[\s\S]*?</sub></p>',
                "![LargeScale-Labor](../../docs/whitepaper/images/android/runenkrieg_tatarus_largescale_lab.png)\n\n**Abbildung 1.** Laboransicht des LargeScale-Zweigs mit 1.024 Neuronen, 32.768 Synapsen und 128 Kanälen.",
                chunk,
                count=1,
            )
        if "Eingefrorene Sieger und unabhängiger Seed-Lauf" in chunk:
            chunk = re.sub(
                r'<p align="center">[\s\S]*?</p>\s*<p align="center"><sub>[\s\S]*?</sub></p>',
                "![Frozen-Winner-Replikation](figures/frozen_replication.png)\n\n**Abbildung 2.** Eingefrorene Gewinner auf 50 zuvor unberührten Replikationsseeds; Lernen war deaktiviert.",
                chunk,
                count=1,
            )
        chunks[idx] = chunk
    chunks[1] = re.sub(
        r"### Inhalt[\s\S]*$",
        """### Inhalt

1. Positionierung und Forschungsziel - Seiten 3-6  
2. Architektur und Mathematik - Seiten 7-17  
3. Versuchsdesign und Evidenz - Seiten 18-25  
4. Runenkrieg-Reallabor und Entwicklungsneuausrichtung - Seiten 26-29  
5. TATARUS-LLM und TSMEMV3 - Seiten 30-32  
6. Schlussfolgerung und Open Science - Seite 33  
7. Literaturverzeichnis - Seiten 34-35""",
        chunks[1],
    )
    chunks[2] = chunks[2].replace(
        "**Schlüsselwörter:**",
        "Der neueste Speicherzweig TSMEMV3 rekonstruiert UTF-8-Inhalte aus lokal plastisch entstandenen Gewichten bei inhaltsunabhängiger Topologie; ein Prozessneustart-Referenzlauf bestand die vorgesehenen Struktur- und Läsionskontrollen. Dieser Einzelversuch wird ausdrücklich nicht als Mehrseed-Bestätigung interpretiert.\n\n**Schlüsselwörter:**",
    )
    final_pages = []
    for page_no, chunk in enumerate(chunks, start=1):
        chunk = re.sub(r'<!-- PAGE \d+/\d+ -->', '', chunk)
        chunk = re.sub(r'<div align="right"><sub>Seite \d+ von \d+</sub></div>', '', chunk)
        final_pages.append(f"<!-- PAGE {page_no:02d}/35 -->\n\n{chunk.strip()}\n\n{page_footer(page_no, 35)}")
    return '\n\n<div style="page-break-after: always;"></div>\n\n'.join(final_pages) + "\n"


@dataclass
class SupplementPage:
    title: str
    body: str


def supplement_pages() -> list[SupplementPage]:
    stage18 = load_json("research/ag_signal_morpher_1ee27305a6aa/06_temporal_classification/ui_cpp/stage18_release_confirmation/stage18_confirmation.json")
    stage19 = load_json("research/ag_signal_morpher_1ee27305a6aa/06_temporal_classification/ui_cpp/stage19_final_release/stage19_results.json")
    stage23 = load_json("research/ag_signal_morpher_1ee27305a6aa/06_temporal_classification/ui_cpp/stage23_final_release_v2/stage20_23_results.json")
    sup11 = load_json("research/ag_signal_morpher_1ee27305a6aa/11_superiority_multiseed/summary.json")
    dxor = load_json("research/ag_signal_morpher_1ee27305a6aa/12_delayed_xor_replication/summary.json")
    holdout = load_json("research/ag_signal_morpher_1ee27305a6aa/13_memory_readout_development/holdout_summary.json")
    tf = load_json("Runenkrieg_TensorFlow_Benchmark/results_full/winner.json")
    tat = load_json("Runenkrieg_Tatarus_10k_Benchmark/results_full/independent_replication.json")
    pages = [
        SupplementPage("Technisches Supplement", """# TATARUS

## Technisches Supplement und Reproduktionshandbuch

**Zum wissenschaftlichen Forschungsbericht / Preprint Version 2.0**

**Autor:** Ralf Krümmel  
**Affiliation:** unabhängiger Privatforscher, Leipzig, Deutschland  
**Software:** TATARUS - A Persistent Synthetic Nervous System  
**Stand:** 31. Juli 2026  
**Code-Lizenz:** Apache License 2.0

Dieses Supplement beschreibt Modelle, Parameter, Binärformate, Experimente, negative Resultate, Ausschlussregeln und Reproduktionsschritte. Es ist kein Ersatz für externe Begutachtung. Alle Leistungsangaben gelten ausschließlich für die dokumentierten synthetischen Aufgaben und Implementierungen."""),
        SupplementPage("Geltungsbereich und Evidenzklassen", """Die Dokumentation verwendet vier getrennte Evidenzklassen: **implementiert** bezeichnet ausführbaren Quellcode; **technisch verifiziert** bezeichnet bestandene Unit-, Integrations- oder Snapshot-Tests; **experimentell bestätigt** bezeichnet vorab eingefrorene Kriterien auf getrennten Seeds; **offen** bezeichnet Hypothesen ohne ausreichenden Holdout- oder Replikationsnachweis.

| Aussage | Mindestbeleg | Nicht ausreichend |
|---|---|---|
| Mechanik vorhanden | Test + Quellpfad | UI-Anzeige allein |
| Gedächtnis kausal | Recall + Neutralisierung | Korrelation |
| Überlegenheit | Holdout + Intervall + Kontrolle | bester Einzelrun |
| biologische Relevanz | externe biologische Validierung | biologisch inspirierte Benennung |

Der Ausdruck „synthetisches Nervensystem“ ist eine funktionale Softwaredefinition. Er impliziert weder Bewusstsein noch biologische Identität."""),
        SupplementPage("Reproduktionspaket und Verzeichnisstruktur", """Das Veröffentlichungspaket liegt unter `docs/research_publication`. `reproduction/source` enthält ein Quellmanifest statt einer zweiten, divergierenden Codekopie. `config` und `protocols` enthalten die ausgewählten Konfigurationen und Versuchspläne; `raw_results` enthält unveränderte maschinenlesbare Resultate; `snapshot_hashes` identifiziert eingefrorene Modelle; `analysis` dokumentiert Ableitungen; `figures` enthält reproduzierte Abbildungen.

Jede Kopie wird im Manifest mit relativer Herkunft, Bytezahl und SHA-256 erfasst. Der Git-Status wird gesondert dokumentiert, weil ein Dirty-Worktree eine relevante Reproduktionsvariable ist. Große generierbare Snapshots werden nicht dupliziert; ihr Hash und Erzeugungsprotokoll bleiben erhalten."""),
        SupplementPage("Softwarelinie und Forschungsstufen", """Die Entwicklung begann mit der mathematischen Charakterisierung des generierten Operators und führte über ereigniskausale Spikes, zeitliche Klassifikation, konfigurierbare Neurodynamik, Delayed-XOR-Ablationen und trace-essential Memory zu einem persistenten Lebenslaufsystem. Stufen 17-19 prüften Repräsentation, sequenzielles Rohsignal, reizfreien Recall, Reparatur und die Cognitive Bridge. Stufen 20-23 ergänzten offene Lebenswelt, mehrskaliges Gedächtnis, Skalierung und Replikationspaket.

Runenkrieg bildet einen separaten anwendungsnahen Zweig. Tatarus_LLM setzt wiederum auf dem persistenten Kern auf. Diese Zweige dürfen nicht so dargestellt werden, als stammten alle Resultate aus demselben Netz oder derselben Parametrisierung."""),
        SupplementPage("Notation, Zeit und Zustandsübergang", """Der vollständige Nervenzustand wird als `S_t` bezeichnet. Ein Simulationsschritt verwendet standardmäßig `dt = 1 ms`. Externe Sensorwerte bilden einen `SensorFrame`; neuronale und synaptische Variablen werden deterministisch fortgesetzt. Ein Prozessneustart darf den Zustand nur über ein erfolgreich validiertes Snapshotformat verändern.

$$S_{t+1}=F(S_t, X_t, A_t, R_{t-1}; \\theta).$$

`X_t` ist die Beobachtung, `A_t` ein begrenzter Planerimpuls und `R_{t-1}` eine von der Umwelt stammende Konsequenz. Parameter `theta` werden pro Experiment gehasht. Der statistische Seed ist nicht mit dem neuronalen Schritt oder einem Bytepositionsindex gleichzusetzen."""),
        SupplementPage("Leaky-Integrate-and-Fire-Soma", """Das kontrollierte Basismodell integriert Leckstrom, externe Ströme, rekurrente Übertragung und optional leitwertbasierte Rezeptorströme. In diskreter Form gilt näherungsweise:

$$V_i(t+dt)=V_i(t)+dt[-(V_i-E_L)/tau_m+I_i(t)].$$

Die Referenzparameter sind `E_L=-65 mV`, Reset `-70 mV`, Basisschwelle `-50 mV`, `tau_m=20 ms` und Refraktärzeit `2 ms`. Ein Spike setzt das Soma zurück und erhöht die adaptive Schwelle. Die Parameter sind keine Schätzung eines bestimmten biologischen Zelltyps; sie definieren einen kontrollierbaren synthetischen Phänotyp."""),
        SupplementPage("Leitwertbasierte Rezeptoren", """Der persistente Kern trennt AMPA, NMDA, GABA-A und GABA-B. Für Rezeptor `r` gilt:

$$I_{r,i}=g_{r,i}(E_r-V_i), \\qquad g_{r,i}(t+dt)=g_{r,i}(t)e^{-dt/tau_r}+Delta g.$$

Standardzeitkonstanten sind 5, 80, 10 und 120 ms. Umkehrpotentiale liegen bei 0, 0, -75 und -95 mV. Diese Aufteilung erzeugt mehrere dynamische Zeitskalen, bleibt aber phänomenologisch: keine Kanaluntereinheiten, Calciumhaushalte oder molekularen Kaskaden werden simuliert."""),
        SupplementPage("Passives Dendritenkompartiment", """Ein optionales passives Dendritenkompartiment trennt Eingangsintegration vom Soma. Der persistente Standard verwendet `tau_d=35 ms` und Kopplung `0,22`; der experimentelle GO-SNN-Zweig verwendet standardmäßig 30 ms und 0,20. Externe Eingänge können anteilig auf den Dendriten gelegt werden.

Die Dendritenablation entfernt dieses Kompartiment bei ansonsten identischen Seeds. Ein Leistungseinbruch belegt einen Nutzen der zusätzlichen Zustandsvariable, nicht die biologische Realitätsnähe eines echten Dendritenbaums."""),
        SupplementPage("Adaptation und Refraktärdynamik", """Nach einem Spike erhöht sich die effektive Schwelle. Die Adaptation zerfällt exponentiell; je nach Kernzweig gelten `1,2 mV / 100 ms` oder `1,5 mV / 80 ms`. Die Refraktärphase verhindert sofortige Wiederfeuerung. Beide Mechanismen begrenzen Burstketten und erzeugen kurzzeitige Zustandsabhängigkeit.

Die Dokumentation führt die abweichenden Standardwerte getrennt auf. Werte des GO-SNN-Versuchsnetzes dürfen nicht als Parameter des persistenten Endsystems oder der 1.024-Neuronen-Android-Instanz zitiert werden."""),
        SupplementPage("Dale-konforme E/I-Topologie", """Neuronen werden als exzitatorisch oder inhibitorisch klassifiziert; ausgehende Synapsen behalten ihr Vorzeichen. Der GO-SNN-Referenzzweig verwendet 80 % exzitatorische Neuronen. Getrennte E->E-, E->I-, I->E- und I->I-Operatorrollen erlauben kontrollierte Synapsenklassenablationen.

Diese Dale-Konformität ist eine Modellrestriktion. Moderne Neurobiologie kennt Kotransmission und differenziertere Zelltypen. TATARUS verwendet die Trennung, um E/I-Balance und kausale Kontrollen klar zu definieren, nicht um alle biologischen Synapsen abzubilden."""),
        SupplementPage("Axonverzögerungen und Ereignisqueues", """Ein `SpikeEvent` speichert Quelle, Emissionsschritt, Amplitude und am Emissionszeitpunkt berechneten Gatewert. Individuelle Verzögerungen bestimmen den späteren Lieferzeitpunkt. Dadurch bleibt die Modulation kausal an das erzeugende Ereignis gebunden und wird nicht aus einem bereits zurückgesetzten Soma rekonstruiert.

Die Queue ist Teil des Snapshots. Exakte Restaurierung verlangt daher nicht nur gleiche Gewichte, sondern auch identische noch ausstehende Ereignisse, Verzögerungen und Simulationszeit."""),
        SupplementPage("STDP-Regel", """Der kontrollierte Zweig verwendet prä- und postsynaptische Spuren mit typischer Zeitkonstante 20 ms. Potenzierung und Depression werden durch relative Spikezeit bestimmt; die Depression ist standardmäßig mit Faktor 1,05 leicht stärker. Gewichte werden begrenzt und respektieren das Synapsenvorzeichen.

STDP kann experimentell deaktiviert werden. Die Implementierung ist phänomenologisch und entspricht keiner direkten Messung an einer bestimmten Synapsenklasse. Bi und Poo (1998) bilden den biologischen Referenzkontext, nicht eine Validierung der TATARUS-Parameter."""),
        SupplementPage("Lokale Eligibility-Spuren", """Jede Synapse besitzt eine signierte oder nichtnegative lokale Spur, die frühere Prä-/Post-Ereignisse über eine Verzögerung erhält. Im persistenten Kern beträgt die Standardzeitkonstante 400 ms; im Suchraum der Stufe 15 wurden 20, 50, 100, 200 und 400 ms geprüft.

Eligibility allein ändert noch nicht zwingend das Langzeitgewicht. Sie moduliert spätere Übertragung oder wird mit einem neuromodulatorischen Lernsignal verknüpft. Gain 0 ist die Neutralitätskontrolle. Zeitverschiebung, Synapsentausch, Absolutwert und Vorzeicheninvertierung trennen Betrag, Timing, Ort und Richtung."""),
        SupplementPage("Kurzzeitressourcen und Facilitation", """Synapsen führen Ressourcen-, Nutzungs- und Facilitation-Zustände. Referenzwerte des persistenten Kerns sind 180 ms Ressourcenerholung, 120 ms Facilitation und 0,18 Freisetzungswahrscheinlichkeit. Die effektive Übertragung hängt dadurch von jüngster Präsynapsenaktivität ab.

Dieser Mechanismus ist vom ursprünglichen konstanten Reset-Gate zu unterscheiden. Die Event-Konstante von 0,128311 reduzierte jeden rekurrenten Spike gleich; Ressourcenplastizität besitzt dagegen Frequenz- und Erholungsabhängigkeit."""),
        SupplementPage("Generated-Operator und Gatekontrollen", f"""Der Algorithmic-Genesis-Operator wird als experimenteller Modulator eingesetzt. Die Pflichtkontrollen sind deaktiviert, event-gematchte Konstante, Vorzeichengate, tanh, verteilungsgematchtes Zufallsgate, zeitverschoben und state-shuffled.

Im 24-Seed-Lauf betrug die Kernel-Accuracy {sup11['mode_means'][0]['accuracy']:.4f}; die Accuracy-Überlegenheit war nicht bestätigt. Die Spikekosten waren gegenüber mehreren Kontrollen geringer, jedoch nicht gegenüber dem Vorzeichengate. Deshalb lautet die zulässige Aussage „spezifischer Effizienzvorteil in der geprüften Aufgabe“, nicht allgemeine Operatorüberlegenheit."""),
        SupplementPage("Ereigniskausale Featureprojektion", """Der Gateeingang kann E/I-Balance, Membransteigung, Schwellenüberschuss und Inter-Spike-Intervall kombinieren:

$$phi=a_1 b_{EI}+a_2 tanh(v'/s_v)+a_3 tanh(o/s_o)+a_4 r_{ISI}.$$

Standardgewichte sind 0,40; 0,25; 0,15; 0,20. Die Projektion wird im Emissionsmoment ausgewertet und mit dem Spike gespeichert. Komponentenablationen prüfen, ob ein beobachteter Effekt tatsächlich aus einer dynamischen Zustandsprojektion entsteht."""),
        SupplementPage("Assembly-Rekrutierung und Konkurrenz", """Assemblies sind wiederkehrende Gruppen aktivierter Neuronen. Neue Muster werden anhand einer Ähnlichkeitsschwelle rekrutiert; der persistente Standard verwendet 0,68. Konkurrenz begrenzt die Zahl gleichzeitig dominanter Repräsentationen. Familiarität, Alter und Aktivierung werden gepoolt an die Cognitive Bridge übertragen.

Stabilität wird über Überlappung, Reaktivierung, Trennung und Snapshotfortsetzung gemessen. Eine Assembly-ID ist keine semantische Bezeichnung. Bedeutung entsteht nur relativ zu Sensorik, Geschichte und Verhalten."""),
        SupplementPage("Energie und Homeostase", """Jedes Neuron besitzt Energie. Der persistente Standard verwendet Erholung 0,0015 pro ms, Spikekosten 0,025 und Transmissionskosten 0,0004. Ein langsamer Regler mit Zielrate 8 Hz und Zeitkonstante 2.000 ms beeinflusst Erregbarkeit oder Kopplung.

Die Energiegröße ist eine interne Kostenfunktion und keine Messung in Joule. Sie ermöglicht kontrollierte Vergleiche innerhalb derselben Implementierung. Hardwareenergie muss separat instrumentiert werden; Spikezahl oder CPU-Zeit sind nur Proxys."""),
        SupplementPage("Strukturplastizität und Reparatur", """In Intervallen von standardmäßig 500 ms werden schwach genutzte und sehr leichte Synapsen für Pruning bewertet; Wachstum kann alternative Pfade erzeugen. Reparaturberichte speichern Pfadprovenienz: verlorene Funktion, übernehmende Struktur und Unterschied zwischen identischer und alternativer Lösung.

Positive Belohnung nach Schaden genügt nicht. Die Stufe-18-Kriterien verlangten funktionale Wiederherstellung und nachvollziehbare Pfadübernahme auf getrennten Seeds."""),
        SupplementPage("Snapshotsemantik", """Ein vollständiger Snapshot umfasst Neuronen, Synapsen, Rezeptorleitwerte, Eligibility, Ressourcen, Axonqueues, Assemblies, Neuromodulatoren, Energie, Umweltdaten und Bridgezustand. Laden validiert Version, Größen, Endlichkeit, IDs und Prüfsummen.

„Persistenz“ bedeutet exakte Zustandsfortsetzung, nicht bloß erneutes Laden trainierter Gewichte. Tests vergleichen Zustands- und Funktionshashes vor dem Speichern und nach der Restaurierung."""),
        SupplementPage("Cognitive Bridge", """Die Bridge abstrahiert maximal acht Repräsentationen und sechzehn Recallkanäle sowie Neuheit, Salienz, Energiebedarf, Aktivitätsbedarf, Vorhersagefehler, Konfidenz und funktionalen Fingerprint. 64-Bit-IDs werden als Dezimalstrings serialisiert.

Der Planer sieht keine internen Synapsen. Diese Informationsgrenze reduziert Leckage und hält die Rollen getrennt: TATARUS trägt Zustand; der Planer interpretiert und formuliert begrenzte Handlungsimpulse."""),
        SupplementPage("Provider und Reward-Isolation", """`LlmProvider` besitzt ein gemeinsames Interface für LM Studio, OpenAI und Gemini. Planung und sichtbare Antwort sind getrennte Operationen. Nur die Planungsoperation hat ein Tool; die Sprachantwort kann weder Reward noch neuronale Kommandos setzen.

`PlannerCommand` enthält keinen Reward. `EnvironmentFeedback` stammt aus der Umwelt und wird erst im Host mit dem validierten Kommando kombiniert. Schemafehler, unbekannte Felder oder nichtendliche Zahlen brechen den Schritt ab."""),
        SupplementPage("Hamming(12,8)-Sensorrepräsentation", """TSMEMV3 bildet jedes UTF-8-Byte auf zwölf Hammingbits ab. Für jedes Bit existieren getrennte Null- und Eins-Sensorkanäle, insgesamt 24. Diese komplementäre Darstellung macht den sensorischen Ereignisraum explizit und erlaubt WTA-Dekodierung.

Der Hammingcode ist fest vorgegeben. Daher ist die wissenschaftlich korrekte Aussage nicht „spontane Entstehung eines Codes“, sondern „spontane Gewichtskodierung innerhalb einer festen Sensorsprache“. Codec-freie Symbolbildung bleibt eine offene Stufe."""),
        SupplementPage("TSMEMV3-Plastizitätsregel", """Jede neue Byteposition rekrutiert ein Assembly mit derselben schwachen, inhaltsfreien Topologie zu allen 24 Kanälen. Startgewichte liegen deterministisch zwischen 0,01 und 0,05. Sechs Expositionsepochen, Hebb-Rate 0,65, rekurrente Rate 0,70, Depression 0,20 und Eligibility-Zerfall 0,85 bilden die Referenzkonfiguration.

Nur lokale Koinzidenz entscheidet, welche Synapsen wachsen. Gleich lange Texte müssen daher denselben Topologie-, aber unterschiedliche Gewichtshashes erzeugen. Dieser Test ist eine zentrale Strukturkontrolle."""),
        SupplementPage("TSMEMV3-Rekonstruktion", """Der Recall startet mit einem Cue-Spike am ersten Assembly. Für jedes Hammingbit gewinnt der stärkere komplementäre Kanal nur oberhalb der Decodierschwelle und bei einem Mindestabstand von 0,10. Recurrent fan-out 4 erzeugt Konkurrenz um die nächste Position.

Nach Hamming-Dekodierung validiert eine 64-Bit-Prüfsumme die gesamte Bytefolge. Eine unvollständige, untrainierte oder beschädigte Episode wird nicht als Teiltext ausgegeben; sie erhöht den Rekonstruktionsfehlerzähler."""),
        SupplementPage("TSMEM-Versionen und Migration", """TSMEMV1 war ein Klartext-JSON und wird nach erfolgreicher Migration entfernt. TSMEMV2 speicherte synaptisch rekonstruierbare Inhalte, hatte die inhaltsabhängige Topologie jedoch konstruktiv gesetzt. V3 rekonstruiert V1/V2 einmalig, exponiert den Ereignisstrom gegenüber dem lokalen Reservoir und speichert anschließend ausschließlich V3.

Der reale Migrationstest wandelte `TSMEMV2` in `TSMEMV3`, erhielt vier Episoden und erzeugte 8.928 Synapsen sowie 53.568 Plastizitätsupdates. Der resultierende Snapshot enthielt weder Prompt noch Codeklartext."""),
        SupplementPage("Ablationsmatrix TSMEMV3", """| Bedingung | Gleich gehalten | Manipulation | Erwartung |
|---|---|---|---|
| anchored | alles | keine | Recall |
| plasticity-off | Topologie/Startwerte | Lernen aus | kein Recall |
| weight lesion | Topologie | Gewichte lesioniert | Recallverlust |
| shuffled anchors | Gewichte/Inhalt | Zustandszuordnung | schlechtere Auswahl |
| lexical-only | Rekonstruktion | Ankerbedingung aus | Inhaltsbaseline |
| disabled | Host/LLM | Speicher aus | kein Episodenrecall |

Zusätzlich müssen gleich lange Texte gleiche Topologie und verschiedene Gewichte besitzen. Ein Binärscan prüft das Fehlen von Klartext."""),
        SupplementPage("Stufe 11: scoped Effizienz", f"""Der vorab festgelegte Versuch `GO-SNN-MS24-C4-v1` umfasste {sup11['seed_count']} Seeds, vier Belastungsbedingungen und eine Million Permutationen pro Vergleich. Kernel und Konstante erreichten beide rund 90,43 % Accuracy. Die Kernelkosten lagen bei {sup11['mode_means'][0]['spikes_per_correct']:.3f} Spikes pro korrekter Entscheidung; das Vorzeichengate war mit {sup11['mode_means'][3]['spikes_per_correct']:.3f} geringfügig sparsamer.

Der Befund rechtfertigt keine globale Überlegenheit. Er motivierte die unabhängige Delayed-XOR-Replikation."""),
        SupplementPage("Stufe 12: negative Delayed-XOR-Replikation", f"""Die Replikation `GO-SNN-DXOR-MS24-D2-v1` umfasste {dxor['raw_evaluation_count']} Rohbewertungen. Alle Gatevarianten lagen nahe Zufall; der Kernel erreichte {dxor['mode_means'][0]['accuracy']:.4f}. Gegen Konstante und Vorzeichengate bestand kein Spikekostenvorteil. Entscheidung: `{dxor['decision']}`.

Dieses negative Resultat bleibt Bestandteil der Veröffentlichung. Es widerlegt die Verallgemeinerung des früheren Effizienzbefunds auf Delayed XOR und führte zur Entwicklung eines tatsächlichen Gedächtnisreadouts."""),
        SupplementPage("Stufe 13: entwickelter Gedächtnisreadout", f"""Auf verbrauchten Entwicklungsseeds wurden längere exponentielle Traces, Soma-/Dendritzustände, Eligibility-Memory und Interaktionsprodukte entwickelt. Das Modell wurde mit Hash `{holdout['model_hash']}` eingefroren. Auf 16 unberührten Seeds erreichte es {holdout['kernel_accuracy']:.4f} Accuracy; die einseitige untere 95-%-Grenze lag bei {holdout['kernel_accuracy_lower_95_one_sided']:.4f}.

Delayed XOR wurde damit zuverlässig gelernt. Die Operator-Effizienzüberlegenheit replizierte sich weiterhin nicht; Funktionserfolg und Operatorbehauptung bleiben getrennt."""),
        SupplementPage("Stufe 14: lokale synaptische Eligibility", """Stufe 14 verlegte Gedächtnis von expliziten Readoutmerkmalen in lokale Synapsenzustände. Tests prüften Neutralität bei Gain 0 und die tatsächliche Modulation späterer Übertragung.

Der mechanische Nachweis war notwendig, aber noch nicht hinreichend: Solange das Readout frühere Cues direkt sah, konnte die Aufgabe ohne interne reizfreie Speicherung gelöst werden. Deshalb wurde Stufe 15 methodisch neu entworfen."""),
        SupplementPage("Stufe 15: trace-essential Memory", """Zwei frühe Cues tragen XOR-Information, gefolgt von einer vollständig reizfreien Phase und einem klassenidentischen Recall-Cue. Das Readout sieht nur das letzte Recallfenster; Eligibility-Features und Interaktionsprodukte werden nicht ausgegeben.

Pflichtkontrollen isolieren Mittelwert, Verteilung, Synapsenort, Timing und Prä-/Post-Richtung. Der Suchraum umfasst fünf Zeitkonstanten, fünf Gains und fünf Maxima. Entwicklungsseeds wählen Pareto-Kandidaten; unberührte Seeds prüfen nur den eingefrorenen Kandidaten."""),
        SupplementPage("Stufe 16: persistenter Nervensystemkern", """Der C++-Kern führt Sensorik, Closed Loop, Snapshots, Neuromodulation, Energie, Homeostase, Strukturplastizität und Mechanismenbibliothek zusammen. Ein Lebenslauf wird nicht zwischen Aufgabenepisoden auf Initialzustand gesetzt.

Die Abnahme prüft deterministische Wiederholbarkeit, Snapshotgleichheit, lokale Neutralisierung und Schaden. Dies bestätigt eine technische Plattform, nicht automatisch jede höhere kognitive Hypothese."""),
        SupplementPage("Stufe 17: Repräsentation und rohe Sequenzen", """Stufe 17 prüft Assemblyüberlappung, Wiederaktivierung ähnlicher Reize, Trennung unterschiedlicher Reize, rohe Übergänge und Grenzerkennung. Der Fokus liegt auf selbstgebildeten internen Einheiten statt vorgegebenen Tokenlabels.

„Tokenizerfrei“ bedeutet hier rohe Byte-/Ereigniseingabe in einer synthetischen Grammatik. Es bedeutet nicht, dass natürliche Sprache bereits ohne externen Decoder verstanden oder generiert wird."""),
        SupplementPage("Stufe 18: eingefrorene Endzielkriterien", f"""Auf {stage18['seed_count']} unberührten Seeds bestanden Assemblybildung {stage18['assembly_passes']}/8, Sequenzbildung {stage18['sequence_passes']}/8 und Reparatur {stage18['recovery_passes']}/8. Trace-essential Recall erreichte {stage18['trace_accuracy']:.0%}; ohne Spur {stage18['trace_control_accuracy']:.4%}.

Diese Resultate bestätigen die vorab definierten synthetischen Kriterien. Sie sind keine biologische Validierung und keine externe Replikation."""),
        SupplementPage("Stufe 19: persistente KI-Kopplung", f"""Die Cognitive Bridge koppelte einen höheren Planer an denselben fortgesetzten Nervenzustand. Auf {stage19['seeds']} Lebenslaufseeds erreichte das Vollsystem {stage19['nervous_accuracy']:.0%}; ohne lokale Spur {stage19['no_trace_accuracy']:.4%}; ohne Nervensystem {stage19['no_system_accuracy']:.0%}. Snapshots setzten exakt fort.

Die Aufgabe war kontrolliert und teilweise beobachtbar. Sie belegt kausale Zustandsnutzung in dieser Domäne, nicht allgemeine Intelligenz."""),
        SupplementPage("Stufe 20: prozedurale offene Lebenswelt", """Die Lebenswelt führte konkurrierende Ziele, verzögerte Konsequenzen, Regelwechsel und unbekannte Ereignisse ein. Sechs von acht Einzelkriterien bestanden. Zwei offene Kriterien verhindern die Aussage, eine vollständig offene Umwelt sei gelöst.

Der teilweise Erfolg wird als Richtungsnachweis behandelt. Künftige Aufgaben müssen frei entstehende Situationen und eigenständig gewählte längere Handlungsfolgen stärker erzwingen."""),
        SupplementPage("Stufe 21: mehrskaliges Gedächtnis", """Episodische Einmalerinnerung, Konsolidierung, prozedurale Anpassung, kontrolliertes Vergessen und Schutz vor Interferenz wurden auf acht neuen Seeds geprüft und bestanden. Unterschiedliche Speicherkomponenten dürfen dennoch nicht mit menschlichen Gedächtnissystemen gleichgesetzt werden.

Eine zentrale Folgeprüfung ist Retention über reale Zeit, Softwareversionen und wechselnde Umwelten ohne nachträgliche Parameteranpassung."""),
        SupplementPage("Stufe 22: Sparse-Skalierung", f"""Die größte ausgeführte Konfiguration umfasste {stage23['maximum_executed_neurons']:,} Neuronen. Der Lauf bestätigte Allokation, Integrität und Snapshot-Restaurierung; er war kein Echtzeitnachweis. Ein dokumentierter Referenzlauf verwendete 2.097.328 aktive Synapsen.

Skalierung wird getrennt nach mathematischer Komplexität, Speicherbedarf, Schrittzeit und funktionaler Qualität berichtet. Mehr Neuronen allein implizieren keine höhere Intelligenz."""),
        SupplementPage("Stufe 23: Replikationspaket", """Das Paket enthält unabhängige Seeds, erwartete Resultate, Protokoll und Manifest. Der lokale Clean-Build wurde ausgeführt; die externe Replikation auf zweiter Hardware ist weiterhin ausstehend.

Der Status lautet daher `package_ready_external_run_pending`. Eine zweite Maschine, andere CPU/GPU und unabhängig erzeugte Seeds sind der nächste Vertrauensschritt."""),
        SupplementPage("Runenkrieg als interaktives Reallabor", """Runenkrieg ist gleichzeitig Spiel, kontrollierte Umwelt und Messlabor. Jede Runde erzeugt Kandidatenaktionen aus denselben 128 Kanälen; Wetter, Kartentyp, Elemente, Token und Verlauf beeinflussen Reward. Reale Spielrunden und Selbsttraining werden getrennt gezählt.

Die Spielintegration zeigt, dass TATARUS unter mobilen Ressourcen kontinuierlich entscheiden und persistieren kann. Sie ersetzt keine standardisierte externe Benchmark-Suite."""),
        SupplementPage("TATARUS LargeScale auf Android", """Der LargeScale-Zweig verwendet 1.024 Neuronen, 32.768 rekurrente Synapsen, 128 verdrahtete Kanäle, 1.024 afferente Projektionen und einen 80-dimensionalen Readout. Snapshots werden flach und gzip-komprimiert persistiert.

Der frühere 72/432/32-Zweig bleibt als Referenz erhalten. Ergebnisse beider Größen werden nicht zusammengelegt. Das mobile Labor protokolliert Beobachtungen, reale Runden, Belohnung, Spikes, Transmissionen und Energieproxy."""),
        SupplementPage("Konventionelle Vergleichsmodelle", """Die Vergleichsgruppe umfasst MLP, GRU, DQN, PPO und Contextual Bandit. Alle erhalten denselben 128-dimensionalen aktuellen Zustand und denselben legalen Aktionsraum. Die GRU erhält Verlauf über ihre Rekurrenz; DQN verwendet Replay; PPO wird on-policy trainiert; der Bandit besitzt keine Rekurrenz.

Die Implementierungen sind repräsentative Kontrollarchitekturen, keine erschöpfende Hyperparametersuche für jede Modellfamilie. Der Vergleich gilt für dieses Protokoll."""),
        SupplementPage("Training, Holdout und Gewinnerregel", """Fünf Trainingsseeds werden bei 250, 500, 1.000, 2.000, 5.000 und 10.000 Umweltrunden geprüft. Pro Checkpoint laufen 20 Holdoutspiele auf Seeds 30000-30019. Die Gewinnerregel priorisiert Spielgewinnrate, Tokenbilanz und Entscheidungszeit.

Erst nach der Auswahl werden Seeds 60000-60049 ausgewertet. Lernen ist deaktiviert; der Zustand muss nach der Evaluation unverändert bleiben. Diese Trennung verhindert Auswahl auf dem finalen Replikationssatz."""),
        SupplementPage("Lernkurven bis 10.000 Runden", """![Lernkurven](figures/learning_curves.png)

**Abbildung S1.** Mittelwerte über fünf Trainingsseeds. TATARUS erreicht am 10.000er-Punkt 81 %, der Contextual Bandit 65 %. DQN und GRU zeigen nichtmonotone Verläufe. Lernkurvenpunkte sind keine unabhängigen Stichproben, da sie aus fortgesetzten Trainingsläufen stammen.

Die Darstellung berichtet alle registrierten Checkpoints und ersetzt keinen Punkt nachträglich durch ein besseres Zwischenergebnis."""),
        SupplementPage("Frozen-Winner-Replikation", f"""![Replikation](figures/frozen_replication.png)

Der TATARUS-Snapshot gewann {tat['wins']}/{tat['games']} Spiele = {tat['game_win_rate']:.0%}. Der eingefrorene Contextual Bandit gewann 30/50 = {tf['independent_replication']['game_win_rate']:.0%}. TATARUS-Lernen war deaktiviert und der Zustand blieb unverändert.

Der Unterschied von zehn Prozentpunkten war mit Fisher `p=0,4019` nicht signifikant. Er ist ein numerischer Replikationsbefund, keine bestätigte Überlegenheit."""),
        SupplementPage("Statistische Interpretation", """Seed beziehungsweise vollständiges Spiel ist die statistische Einheit, nicht jeder neuronale Schritt. Gepaarte Designs werden bevorzugt, wenn Episoden bitidentisch erzeugt werden können. Berichtet werden Punktwert, Streuung, Bootstrap- oder Wilson-Intervall und Effektgröße.

Die aktuelle Android/Kotlin- und Python-Umwelt verwendet dieselben Seedbereiche, aber unterschiedliche Zufallszahlengeneratoren. Der Vergleich ist distributionssymmetrisch, nicht bitidentisch gepaart. Entscheidungszeiten stammen zudem aus verschiedenen Laufzeitpfaden."""),
        SupplementPage("Fehlschläge und Neuausrichtungen", """1. Das Reset-Gate war effektiv eine Konstante von 0,128311; die Kernelgeometrie war nicht nachgewiesen.  
2. Delayed XOR replizierte zunächst nur Zufallsniveau.  
3. Ein Readout konnte Cue-Memory statt synaptisches Gedächtnis nutzen.  
4. Die Operator-Effizienz replizierte nicht auf Delayed XOR.  
5. Die offene Lebenswelt bestand nur 6/8 Kriterien.  
6. Der erste KI-Vergleich war asymmetrisch und wurde durch einen TATARUS-10k-Gegenlauf korrigiert.  
7. Ein Hardwarewechsel pausierte den Lauf sicher.  
8. Android Asset Packaging erforderte ein neutrales Snapshot-Suffix.

Diese Punkte sind Teil der Evidenz und dürfen nicht aus der Veröffentlichung entfernt werden."""),
        SupplementPage("Build- und Laufumgebung", """Die Referenzumgebung ist Windows 11 auf AMD64 mit MSVC/C++20. Der TensorFlow-Lauf dokumentiert Python 3.12.10, TensorFlow 2.21.0, zwölf logische CPUs, deterministische Ops und deaktivierte oneDNN-Optimierungen. Android-Builds verwenden getrennte App-IDs für Referenz-, LargeScale- und Winner-Varianten.

Jede Replikation soll Compiler, Buildtyp, Commit, Dirty-Status, CPU/GPU, Betriebssystem, Smartphone-Modell, LM-Studio-Version und exakte Modell-ID protokollieren."""),
        SupplementPage("Konfiguration und Parameterprovenienz", """Parameter stammen aus expliziten C++-Strukturen und JSON-Konfigurationen. Das Reproduktionsmanifest verweist auf `bio_core.hpp`, `nervous_system.hpp` und `tatarus_llm.example.json`. Für jeden Lauf ist der vollständig geparste Konfigurationshash maßgeblich, nicht nur die UI-Anzeige.

Parameteroptimierung und Holdoutauswertung sind zu trennen. Nach Einfrieren eines Kandidaten dürfen weder Projektionsgewichte noch Schwellen anhand des Evaluationssatzes geändert werden."""),
        SupplementPage("Testspezifikation", """Die Testebenen sind: mathematische Unit-Tests; deterministische Netzwerkregression; Snapshot-Roundtrip; Neutralitätskontrollen; Ablationen; Mehrseed-Holdout; Frozen-Winner-Replikation; Live-Provider-Smoke-Test. Ein bestandener niedrigerer Test ersetzt keinen höheren.

TSMEMV3 verlangt zusätzlich Klartextscan, gleiche Topologie bei gleichem Umfang, verschiedene Gewichtshashes, Plastizität-aus, Läsion und beschädigte Prüfsumme. Provider-Tests prüfen Schema, Reward-Isolation und History-Leerung."""),
        SupplementPage("Timeout-, Fehler- und Ausschlussregeln", """Provider-Timeouts, ungültige Tool-Calls, Snapshotfehler und Buildabbrüche werden separat gezählt. Sie dürfen nicht stillschweigend als falsche Antwort oder auswertungsfreie Runde verschwinden. Ein Seed wird nur nach einer vorab definierten technischen Regel ausgeschlossen.

Mehrere geladene LM-Studio-Modelle machen den Lauf ungültig und führen fail-closed zum Abbruch. Hardwareabweichungen pausieren Zeitmessungen. Nach einem Absturz wird aus dem letzten validierten Snapshot fortgesetzt oder der Lauf vollständig wiederholt."""),
        SupplementPage("Rohdaten und Aggregation", """CSV- und JSON-Rohdaten bleiben unverändert. Aggregierte Tabellen werden aus diesen Dateien erzeugt und enthalten Protokoll-ID, Seeds, Checkpoint und Modell. Die Veröffentlichung kopiert nur ausgewählte kleine Rohartefakte; das Manifest verweist auf den vollständigen Repositorypfad.

Analyseskripte müssen fehlende Werte, Duplikate und erwartete Zeilenzahlen prüfen. 30/30 TATARUS-Checkpoints und die registrierten konventionellen Kombinationen bilden Integritätskriterien."""),
        SupplementPage("Snapshot- und Gewinnerhashes", """Der eingefrorene TATARUS-Gewinner besitzt SHA-256 `98c5671ebfcf64734d4d791e324543a727549b6c2a6aad53db78f445e4f71668` und 1.564.970 Bytes. Der Contextual-Bandit-Export besitzt SHA-256 `e94827bd1a09120e8fe4ec531af9da9a2418971b570c3804b1a6de68f7510e8e` und 1.504 Bytes.

Die Größen sind nicht semantisch vergleichbar: TATARUS speichert einen reicheren neuronalen Zustand. Hashes identifizieren Artefakte, nicht ihre funktionale Gleichwertigkeit."""),
        SupplementPage("LM-Studio- und Cloudprotokolle", """LM Studio wird über den OpenAI-kompatiblen lokalen Endpunkt angesprochen; das aktuell geladene Einzelmodell wird vor jedem Planungsschritt erkannt. OpenAI und Gemini verwenden denselben internen `PlannerCommand`, aber anbieterspezifische Function-Calling-Formate.

Wissenschaftliche Läufe protokollieren Provider, exakte Modell-ID, Temperatur 0, Requesthash, Antwortstatus und Latenz. API-Schlüssel, vollständige neuronale Zustände und Snapshots dürfen nicht in Protokolle oder Prompts gelangen."""),
        SupplementPage("Prompt- und Sprachkanal", """Der Systemprompt beschreibt eine begrenzte Planerrolle und verlangt genau einen Tool-Call. Anschließend verarbeitet TATARUS den Schritt. Ein zweiter Request ohne Tools formuliert die sichtbare Antwort aus aktuellem Zustand und ausgewählten, als Daten markierten Episoden.

Im wissenschaftlichen Modus wird der LLM-Verlauf aktiv geleert. Produktmodusresultate müssen gesondert ausgewiesen werden, da dort LLM-Kontext und TATARUS-Gedächtnis gleichzeitig wirken."""),
        SupplementPage("Privacy und Sicherheitsgrenzen", """Der lokale Demo-Server bindet nur an localhost und begrenzt Requestgröße. Snapshots sind validiert, aber nicht verschlüsselt. Ein bösartiger lokaler Prozess oder kompromittierter LLM-Server liegt außerhalb des Bedrohungsmodells.

Für öffentliche Bereitstellung fehlen TLS, Authentifizierung, Rate Limits und getrennte Versuchszustände. Der Forschungsbericht behauptet daher keine produktionsreife Internetexposition."""),
        SupplementPage("Reproduktionsanleitung", """1. Repository in einen neuen Pfad klonen und Commit dokumentieren.  
2. C++-Kern und Tests im Release-Modus bauen.  
3. Stufen 18/19 mit den bereitgestellten Seeds regressieren.  
4. TSMEMV3-Tests inklusive Topologie-, Läsions- und Klartextkontrolle ausführen.  
5. Für LLM-Tests genau ein Modell in LM Studio laden.  
6. Mehrseed-Läufe nur mit registrierten Konfigurationen starten.  
7. Rohdaten unverändert sichern und Hashmanifest erzeugen.  
8. Abweichungen als Ergebnis dokumentieren.

Die genaue Befehlsfolge und Pfade stehen in `README_REPRODUCTION.md`."""),
        SupplementPage("Artefaktmanifest", """Das maschinenlesbare `MANIFEST_SHA256.json` erfasst Dokumente, Konfigurationen, Protokolle, Rohresultate, Abbildungen und Quellverweise. Jeder Eintrag enthält relativen Pfad, Bytezahl, SHA-256 und Rolle.

Der Manifesthash wird nach Erstellung der finalen PDFs erneut berechnet. Die PDFs selbst werden in einem separaten Publikationsmanifest geführt, damit keine zirkuläre Selbsthash-Abhängigkeit entsteht."""),
        SupplementPage("Offene Hypothesen und Falsifikationsplan", """Offen sind: semantisches TSMEMV3-Mehrseed-Recall ohne LLM-History; Modellwechseltransfer; codec-freie Symbolbildung; langfristige Retention; unabhängige Hardware-Replikation; strikt gepaarter Runenkrieg-Vergleich; statistisch belastbare Leistungsdifferenz; Echtzeitskalierung; Transfer auf Audio und Bilder.

Eine Hypothese gilt als widerlegt oder nicht bestätigt, wenn das vorregistrierte Intervall die praktische Mindestdifferenz nicht überschreitet, eine kausale Kontrolle gleichwertig ist oder ein klartext-/topologiegetragenes Alternativerklärungsmodell den Effekt reproduziert. Negative Resultate bleiben versioniert erhalten."""),
        SupplementPage("Technische Referenzen und Quellenpfade", """Primäre interne Quellen sind `README.md`, `UI_DOKUMENTATION.md`, `research/.../FINAL_REPORT.md`, die Stufenberichte 11-23, `RUNENKRIEG_VERGLEICHSBERICHT.md`, beide 10k-Statistikberichte sowie `Tatarus_LLM/ARCHITECTURE.md`, `EXPERIMENT_PROTOCOL.md` und `REQUIREMENTS_TRACEABILITY.md`.

Die vollständige externe Fachliteratur steht auf Seiten 34-35 des Hauptpapers. Lokale Artefakte werden nicht als Peer-Review-Ersatz zitiert, sondern als prüfbare Provenienz der berichteten Software- und Messresultate."""),
        SupplementPage("Abschließende technische Aussagegrenze", """TATARUS ist ein ausführbares, persistentes synthetisches Nervensystem mit lokal plastischen, regulatorischen und strukturellen Zuständen. In den dokumentierten synthetischen Aufgaben bestehen kausale Gedächtnis-, Repräsentations- und Reparaturkontrollen. Im Runenkrieg-Labor wurde ein numerischer, nicht signifikanter Frozen-Winner-Vorsprung beobachtet. TSMEMV3 zeigt lokal gelernte, spike-rekonstruierbare Gewichtskodierung innerhalb einer festen Sensorsprache.

Nicht belegt sind Bewusstsein, biologische Gleichwertigkeit, allgemeine Intelligenz, universelle Operatorüberlegenheit, statistisch bestätigte Spielüberlegenheit, codec-freie Sprachentstehung oder externe Replikation. Diese Grenzen sind Bestandteil des Ergebnisses."""),
    ]
    # Two descriptions are already covered in the immediately adjacent model
    # and provenance pages.  Keeping them there as separate pages would push
    # the supplement beyond the pre-registered 40-60 page window.
    pages = [page for page in pages if page.title not in {
        "Adaptation und Refraktärdynamik",
        "Technische Referenzen und Quellenpfade",
    }]
    if len(pages) != 60:
        raise RuntimeError(f"Expected 60 supplement source units, got {len(pages)}")
    # Consolidate related short technical units.  This yields forty dense
    # publication pages instead of a visually padded sixty-page supplement:
    # cover and evidence frame stay separate; units 3-42 are combined in
    # adjacent pairs; the data-, protocol- and limitation-heavy final units
    # remain separate.
    consolidated = pages[:2]
    for idx in range(2, 42, 2):
        first, second = pages[idx], pages[idx + 1]
        consolidated.append(SupplementPage(
            f"{first.title} / {second.title}",
            f"{first.body}\n\n### {second.title}\n\n{second.body}",
        ))
    consolidated.extend(pages[42:])
    if len(consolidated) != 40:
        raise RuntimeError(f"Expected 40 consolidated supplement pages, got {len(consolidated)}")
    return consolidated


def build_supplement_markdown() -> str:
    pages = supplement_pages()
    blocks = []
    for idx, page in enumerate(pages, start=1):
        body = page.body if page.body.lstrip().startswith("#") else f"## {page.title}\n\n{page.body}"
        blocks.append(f"<!-- PAGE {idx:02d}/40 -->\n\n{body.strip()}\n\n{page_footer(idx, 40)}")
    return '\n\n<div style="page-break-after: always;"></div>\n\n'.join(blocks) + "\n"


def register_fonts() -> None:
    pdfmetrics.registerFont(TTFont("Arial", "C:/Windows/Fonts/arial.ttf"))
    pdfmetrics.registerFont(TTFont("Arial-Bold", "C:/Windows/Fonts/arialbd.ttf"))
    pdfmetrics.registerFont(TTFont("Arial-Italic", "C:/Windows/Fonts/ariali.ttf"))
    pdfmetrics.registerFontFamily("Arial", normal="Arial", bold="Arial-Bold", italic="Arial-Italic")


def inline_markup(text: str) -> str:
    text = re.sub(r'<br\s*/?>', ' | ', text, flags=re.IGNORECASE)
    text = re.sub(r'<[^>]+>', '', text)
    replacements = {
        r"\Delta": "Δ", r"\tau": "τ", r"\theta": "θ", r"\eta": "η",
        r"\lambda": "λ", r"\phi": "φ", r"\kappa": "κ", r"\rho": "ρ",
        r"\gamma": "γ", r"\sum": "Σ", r"\neq": "≠", r"\approx": "≈",
        r"\geq": "≥", r"\leq": "≤", r"\ge": "≥", r"\le": "≤",
        r"\in": " in ", r"\cap": " intersects ", r"\varnothing": "EMPTY", r"\cdot": "·",
        r"\rightarrow": "→", r"\to": "→", r"\lvert": "|", r"\rvert": "|",
        r"\Vert": "||", r"\qquad": "   ", r"\quad": "  ", r"\,": " ",
    }
    for source, target in replacements.items():
        text = text.replace(source, target)
    text = re.sub(r'\\(?:mathcal|mathrm|text|operatorname|boxed)\s*\{([^{}]*)\}', r'\1', text)
    text = re.sub(r'\\frac\s*\{([^{}]*)\}\s*\{([^{}]*)\}', r'(\1)/(\2)', text)
    text = text.replace(r"\left", "").replace(r"\right", "")
    text = text.replace(r"\!", "").replace(r"\\", " ")
    text = text.replace("{", "").replace("}", "")
    text = re.sub(r'\\[A-Za-z]+', '', text)
    text = html.escape(text.strip())
    text = re.sub(r'!\[([^]]*)\]\(([^)]+)\)', '', text)
    text = re.sub(r'\[([^]]+)\]\(([^)]+)\)', r'<link href="\2" color="#176B87">\1</link>', text)
    text = re.sub(r'`([^`]+)`', r'<font name="Courier">\1</font>', text)
    text = re.sub(r'\*\*([^*]+)\*\*', r'<b>\1</b>', text)
    text = re.sub(r'(?<!\*)\*([^*]+)\*(?!\*)', r'<i>\1</i>', text)
    text = text.replace("$", "")
    return text


def styles():
    ss = getSampleStyleSheet()
    return {
        "title": ParagraphStyle("TitleAcademic", parent=ss["Title"], fontName="Arial-Bold", fontSize=27, leading=31, textColor=colors.HexColor("#12304A"), alignment=TA_CENTER, spaceAfter=20),
        "subtitle": ParagraphStyle("SubtitleAcademic", parent=ss["Normal"], fontName="Arial", fontSize=14, leading=18, textColor=colors.HexColor("#31627A"), alignment=TA_CENTER, spaceAfter=14),
        "h1": ParagraphStyle("H1Academic", parent=ss["Heading1"], fontName="Arial-Bold", fontSize=18, leading=22, textColor=colors.HexColor("#12304A"), spaceBefore=3, spaceAfter=10),
        "h2": ParagraphStyle("H2Academic", parent=ss["Heading2"], fontName="Arial-Bold", fontSize=14, leading=17, textColor=colors.HexColor("#176B87"), spaceBefore=4, spaceAfter=7),
        "h3": ParagraphStyle("H3Academic", parent=ss["Heading3"], fontName="Arial-Bold", fontSize=11, leading=14, textColor=colors.HexColor("#24566E"), spaceBefore=3, spaceAfter=4),
        "body": ParagraphStyle("BodyAcademic", parent=ss["BodyText"], fontName="Arial", fontSize=8.5, leading=10.5, alignment=TA_JUSTIFY, textColor=colors.HexColor("#18262F"), spaceAfter=4),
        "quote": ParagraphStyle("QuoteAcademic", parent=ss["BodyText"], fontName="Arial-Italic", fontSize=9.1, leading=12, alignment=TA_LEFT, leftIndent=18, rightIndent=18, borderColor=colors.HexColor("#00A7C4"), borderWidth=0, borderPadding=8, backColor=colors.HexColor("#EDF6F8"), spaceAfter=7),
        "equation": ParagraphStyle("EquationAcademic", parent=ss["BodyText"], fontName="Arial-Italic", fontSize=9.3, leading=12, alignment=TA_CENTER, textColor=colors.HexColor("#12304A"), spaceBefore=5, spaceAfter=6),
        "code": ParagraphStyle("CodeAcademic", fontName="Courier", fontSize=7.2, leading=8.5, leftIndent=10, rightIndent=10, backColor=colors.HexColor("#F2F5F7"), borderPadding=6, spaceAfter=6),
        "caption": ParagraphStyle("CaptionAcademic", parent=ss["BodyText"], fontName="Arial-Italic", fontSize=7.5, leading=9, alignment=TA_LEFT, textColor=colors.HexColor("#52636D"), spaceBefore=3, spaceAfter=6),
        "small": ParagraphStyle("SmallAcademic", parent=ss["BodyText"], fontName="Arial", fontSize=7.4, leading=9, textColor=colors.HexColor("#52636D"), spaceAfter=3),
    }


def split_pages(md: str) -> list[str]:
    return [p.strip() for p in re.split(r'<div style="page-break-after: always;"></div>', md) if p.strip()]


def table_from_lines(lines: list[str], sty) -> Table:
    rows = []
    for line in lines:
        cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
        if all(re.fullmatch(r":?-{3,}:?", c) for c in cells):
            continue
        rows.append([Paragraph(inline_markup(c), sty["small"]) for c in cells])
    if not rows:
        return Table([[""]])
    count = max(len(row) for row in rows)
    for row in rows:
        row.extend([""] * (count - len(row)))
    widths = [6.5 * inch / count] * count
    table = Table(rows, colWidths=widths, repeatRows=1, hAlign="LEFT")
    table.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#DDECF1")),
        ("TEXTCOLOR", (0, 0), (-1, 0), colors.HexColor("#12304A")),
        ("FONTNAME", (0, 0), (-1, 0), "Arial-Bold"),
        ("GRID", (0, 0), (-1, -1), 0.35, colors.HexColor("#9BB3BF")),
        ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
        ("LEFTPADDING", (0, 0), (-1, -1), 4),
        ("RIGHTPADDING", (0, 0), (-1, -1), 4),
        ("TOPPADDING", (0, 0), (-1, -1), 3),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 3),
    ]))
    return table


def markdown_page_flowables(page: str, sty) -> list:
    out = []
    lines = page.splitlines()
    paragraph: list[str] = []
    in_code = False
    code_lines: list[str] = []
    in_math = False
    math_lines: list[str] = []
    bullets: list[str] = []
    numbered: list[str] = []

    def flush_para():
        nonlocal paragraph
        if paragraph:
            text = " ".join(x.strip() for x in paragraph if x.strip())
            if text:
                out.append(Paragraph(inline_markup(text), sty["body"]))
            paragraph = []

    def flush_lists():
        nonlocal bullets, numbered
        for values, kind in ((bullets, "bullet"), (numbered, "1")):
            if values:
                items = [ListItem(Paragraph(inline_markup(v), sty["body"]), leftIndent=8) for v in values]
                out.append(ListFlowable(items, bulletType=kind, leftIndent=22, bulletFontName="Arial", bulletFontSize=7.5, spaceAfter=4))
                values.clear()

    idx = 0
    while idx < len(lines):
        raw = lines[idx]
        stripped = raw.strip()
        if stripped.startswith("<!--") or re.match(r'<div align="right">', stripped):
            idx += 1
            continue
        if stripped.startswith("```"):
            flush_para(); flush_lists()
            if in_code:
                out.append(Preformatted("\n".join(code_lines), sty["code"]))
                code_lines = []
            in_code = not in_code
            idx += 1
            continue
        if in_code:
            code_lines.append(raw)
            idx += 1
            continue
        if stripped.startswith("$$") or stripped.startswith("\\["):
            flush_para(); flush_lists()
            if (stripped.endswith("$$") and len(stripped) > 4):
                out.append(Paragraph(inline_markup(stripped.strip("$").strip()), sty["equation"]))
            elif in_math:
                out.append(Paragraph(inline_markup(" ".join(math_lines)), sty["equation"]))
                math_lines = []
                in_math = False
            else:
                in_math = True
            idx += 1
            continue
        if in_math:
            if stripped.endswith("$$") or stripped.endswith("\\]"):
                math_lines.append(stripped.rstrip("$\\] "))
                out.append(Paragraph(inline_markup(" ".join(math_lines)), sty["equation"]))
                math_lines = []
                in_math = False
            else:
                math_lines.append(stripped)
            idx += 1
            continue
        image_match = re.search(r'!\[([^]]*)\]\(([^)]+)\)', stripped)
        html_images = re.findall(r'<img[^>]+src="([^"]+)"[^>]*>', stripped)
        if image_match or html_images:
            flush_para(); flush_lists()
            paths = [image_match.group(2)] if image_match else html_images
            for rel in paths[:2]:
                p = (OUT / rel).resolve() if not Path(rel).is_absolute() else Path(rel)
                if not p.exists():
                    p = (ROOT / rel).resolve()
                if p.exists():
                    im = Image(str(p))
                    max_w, max_h = 6.25 * inch, 3.55 * inch
                    scale = min(max_w / im.imageWidth, max_h / im.imageHeight)
                    im.drawWidth = im.imageWidth * scale
                    im.drawHeight = im.imageHeight * scale
                    im.hAlign = "CENTER"
                    out.append(im)
                    out.append(Spacer(1, 4))
            idx += 1
            continue
        if stripped.startswith("<") and stripped.endswith(">"):
            idx += 1
            continue
        if stripped.startswith("# "):
            flush_para(); flush_lists(); out.append(Spacer(1, 80)); out.append(Paragraph(inline_markup(stripped[2:]), sty["title"]))
        elif stripped.startswith("## "):
            flush_para(); flush_lists(); out.append(Paragraph(inline_markup(stripped[3:]), sty["h1"]))
        elif stripped.startswith("### "):
            flush_para(); flush_lists(); out.append(Paragraph(inline_markup(stripped[4:]), sty["h2"]))
        elif stripped.startswith(">"):
            flush_para(); flush_lists(); quote = stripped.lstrip("> ")
            out.append(Paragraph(inline_markup(quote), sty["quote"]))
        elif stripped.startswith("|") and idx + 1 < len(lines):
            flush_para(); flush_lists()
            table_lines = []
            while idx < len(lines) and lines[idx].strip().startswith("|"):
                table_lines.append(lines[idx].strip())
                idx += 1
            out.append(table_from_lines(table_lines, sty))
            out.append(Spacer(1, 5))
            continue
        elif re.match(r'^[-*] ', stripped):
            flush_para(); numbered.clear(); bullets.append(stripped[2:].strip())
        elif re.match(r'^\d+\. ', stripped):
            flush_para(); bullets.clear(); numbered.append(re.sub(r'^\d+\.\s*', '', stripped))
        elif not stripped:
            flush_para(); flush_lists()
        else:
            if bullets or numbered:
                flush_lists()
            paragraph.append(stripped)
        idx += 1
    flush_para(); flush_lists()
    return out


class AcademicDocTemplate(BaseDocTemplate):
    def __init__(self, filename: str, total_pages: int, short_title: str):
        self.total_pages = total_pages
        self.short_title = short_title
        super().__init__(filename, pagesize=LETTER, leftMargin=0.78 * inch, rightMargin=0.78 * inch, topMargin=0.67 * inch, bottomMargin=0.65 * inch, title=short_title, author="Ralf Krümmel")
        frame = Frame(self.leftMargin, self.bottomMargin, self.width, self.height, id="academic", leftPadding=0, rightPadding=0, topPadding=0, bottomPadding=0)
        self.addPageTemplates([PageTemplate(id="academic", frames=[frame], onPage=self._decorate)])

    def _decorate(self, canvas, doc):
        canvas.saveState()
        page = canvas.getPageNumber()
        if page > 1:
            canvas.setStrokeColor(colors.HexColor("#B7CAD3"))
            canvas.setLineWidth(0.5)
            canvas.line(self.leftMargin, LETTER[1] - 0.45 * inch, LETTER[0] - self.rightMargin, LETTER[1] - 0.45 * inch)
            canvas.setFont("Arial", 7.2)
            canvas.setFillColor(colors.HexColor("#52636D"))
            canvas.drawString(self.leftMargin, LETTER[1] - 0.34 * inch, self.short_title)
        canvas.setFont("Arial", 7.2)
        canvas.setFillColor(colors.HexColor("#52636D"))
        canvas.drawRightString(LETTER[0] - self.rightMargin, 0.34 * inch, f"Seite {page} von {self.total_pages}")
        canvas.restoreState()


def render_markdown(md_path: Path, pdf_path: Path, expected_pages: int, short_title: str) -> None:
    register_fonts()
    sty = styles()
    pages = split_pages(md_path.read_text(encoding="utf-8"))
    if len(pages) != expected_pages:
        raise RuntimeError(f"{md_path.name}: expected {expected_pages} source pages, got {len(pages)}")
    story = []
    for idx, page in enumerate(pages):
        story.extend(markdown_page_flowables(page, sty))
        if idx != len(pages) - 1:
            story.append(PageBreak())
    doc = AcademicDocTemplate(str(pdf_path), expected_pages, short_title)
    doc.build(story)


def copy_reproduction_material() -> None:
    for folder in ["source", "tests", "config", "protocols", "raw_results", "analysis", "snapshot_hashes", "figures"]:
        (REPRO / folder).mkdir(parents=True, exist_ok=True)
    copies = {
        "config/tatarus_llm.example.json": "Tatarus_LLM/config/tatarus_llm.example.json",
        "protocols/TATARUS_LLM_EXPERIMENT_PROTOCOL.md": "Tatarus_LLM/EXPERIMENT_PROTOCOL.md",
        "protocols/RUNENKRIEG_TATARUS_PROTOCOL.md": "Runenkrieg_Tatarus_10k_Benchmark/EXPERIMENT_PROTOCOL.md",
        "protocols/RUNENKRIEG_TF_PROTOCOL.md": "Runenkrieg_TensorFlow_Benchmark/EXPERIMENT_PROTOCOL.md",
        "raw_results/stage11_summary.json": "research/ag_signal_morpher_1ee27305a6aa/11_superiority_multiseed/summary.json",
        "raw_results/stage12_delayed_xor_summary.json": "research/ag_signal_morpher_1ee27305a6aa/12_delayed_xor_replication/summary.json",
        "raw_results/stage13_holdout_summary.json": "research/ag_signal_morpher_1ee27305a6aa/13_memory_readout_development/holdout_summary.json",
        "raw_results/stage18_confirmation.json": "research/ag_signal_morpher_1ee27305a6aa/06_temporal_classification/ui_cpp/stage18_release_confirmation/stage18_confirmation.json",
        "raw_results/stage19_results.json": "research/ag_signal_morpher_1ee27305a6aa/06_temporal_classification/ui_cpp/stage19_final_release/stage19_results.json",
        "raw_results/stage20_23_results.json": "research/ag_signal_morpher_1ee27305a6aa/06_temporal_classification/ui_cpp/stage23_final_release_v2/stage20_23_results.json",
        "raw_results/tatarus_learning_curves.csv": "Runenkrieg_Tatarus_10k_Benchmark/results_full/aggregate_learning_curves.csv",
        "raw_results/tensorflow_learning_curves.csv": "Runenkrieg_TensorFlow_Benchmark/results_full/aggregate_learning_curves.csv",
        "raw_results/tatarus_independent_replication.json": "Runenkrieg_Tatarus_10k_Benchmark/results_full/independent_replication.json",
        "raw_results/tensorflow_winner.json": "Runenkrieg_TensorFlow_Benchmark/results_full/winner.json",
        "snapshot_hashes/tatarus_frozen_winner.json": "Runenkrieg_Tatarus_10k_Benchmark/exports/tatarus_frozen_winner.json",
        "snapshot_hashes/tensorflow_frozen_winner.json": "Runenkrieg_TensorFlow_Benchmark/exports/runenkrieg_frozen_winner.json",
        "analysis/RUNENKRIEG_VERGLEICHSBERICHT.md": "RUNENKRIEG_VERGLEICHSBERICHT.md",
        "analysis/TATARUS_LLM_ARCHITECTURE.md": "Tatarus_LLM/ARCHITECTURE.md",
        "tests/TATARUS_LLM_TRACEABILITY.md": "Tatarus_LLM/REQUIREMENTS_TRACEABILITY.md",
    }
    for dest, source in copies.items():
        shutil.copy2(ROOT / source, REPRO / dest)
    for f in FIGURES.glob("*.png"):
        shutil.copy2(f, REPRO / "figures" / f.name)
    source_manifest = [
        "research/ag_signal_morpher_1ee27305a6aa/06_temporal_classification/ui_cpp/bio_core.hpp",
        "research/ag_signal_morpher_1ee27305a6aa/06_temporal_classification/ui_cpp/nervous_system.hpp",
        "research/ag_signal_morpher_1ee27305a6aa/06_temporal_classification/ui_cpp/cognitive_bridge.cpp",
        "Tatarus_LLM/include/tatarus_llm/episodic_memory.hpp",
        "Tatarus_LLM/src/episodic_memory.cpp",
        "Tatarus_LLM/src/tatarus_planner_host.cpp",
        "Tatarus_LLM/src/main.cpp",
    ]
    (REPRO / "source" / "SOURCE_MANIFEST.txt").write_text("\n".join(source_manifest) + "\n", encoding="utf-8")
    readme = """# TATARUS Reproduction Package

This directory accompanies the German scientific report and technical supplement.

1. Use a fresh clone of the repository and record the Git commit and dirty status.
2. Verify every copied artifact against `MANIFEST_SHA256.json`.
3. Read the three files in `protocols/` before running an experiment.
4. Treat development seeds, selection holdouts, and independent replication seeds as disjoint.
5. Do not silently discard failed provider calls, timeouts, build failures, or divergent replications.
6. The `source/` directory is a manifest, not a duplicate source tree; resolve paths against the repository root.

The package documents a private research preprint by Ralf Krümmel, Leipzig, Germany. It does not claim peer review or external replication.
"""
    (REPRO / "README_REPRODUCTION.md").write_text(readme, encoding="utf-8")


def write_manifests() -> None:
    entries = []
    for path in sorted(p for p in REPRO.rglob("*") if p.is_file() and p.name != "MANIFEST_SHA256.json"):
        entries.append({"path": path.relative_to(REPRO).as_posix(), "bytes": path.stat().st_size, "sha256": sha256(path)})
    (REPRO / "MANIFEST_SHA256.json").write_text(json.dumps({"protocol": "TATARUS-PUBLICATION-REPRO-1", "entries": entries}, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    publications = []
    for name in ["TATARUS_PAPER.pdf", "TATARUS_SUPPLEMENT.pdf", "TATARUS_PAPER_DE.md", "TATARUS_SUPPLEMENT_DE.md"]:
        path = OUT / name
        publications.append({"path": name, "bytes": path.stat().st_size, "sha256": sha256(path)})
    (OUT / "PUBLICATION_MANIFEST_SHA256.json").write_text(json.dumps({"protocol": "TATARUS-PUBLICATION-1", "entries": publications}, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    QA.mkdir(parents=True, exist_ok=True)
    generate_figures()
    paper_md = OUT / "TATARUS_PAPER_DE.md"
    supplement_md = OUT / "TATARUS_SUPPLEMENT_DE.md"
    paper_md.write_text(build_main_markdown(), encoding="utf-8")
    supplement_md.write_text(build_supplement_markdown(), encoding="utf-8")
    render_markdown(paper_md, OUT / "TATARUS_PAPER.pdf", 35, "TATARUS - Wissenschaftlicher Forschungsbericht")
    render_markdown(supplement_md, OUT / "TATARUS_SUPPLEMENT.pdf", 40, "TATARUS - Technisches Supplement")
    copy_reproduction_material()
    write_manifests()


if __name__ == "__main__":
    main()
