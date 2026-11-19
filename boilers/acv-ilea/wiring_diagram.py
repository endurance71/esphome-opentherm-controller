#!/usr/bin/env python3
"""Generates the ACV ILEA -> OpenTherm adapter -> ESP32 wiring diagram (EN and PL)."""
import sys

T = {
    "en": dict(
        title="ACV ILEA – where to connect the OpenTherm adapter",
        box="Boiler electrical box\n(front panel off, box lowered)",
        selv="SELV terminal block (low voltage)",
        floor="Underfloor\nsafety", outdoor="Outdoor\nsensor 10 kΩ", z2="Z2\nzone 2", z1="Z1\nzone 1",
        jumper="jumper", remove="remove jumper /\nold relay wires",
        lv="230 V block (LV)\nDO NOT CONNECT ANYTHING HERE",
        cable="2-core cable 2×0.5–1.5 mm²\nany polarity",
        adapter="OpenTherm adapter", bside="boiler side", eside="ESP32 side", iso="isolation gap",
        esp="ESP32 DevKit", usb="USB", psu="5 V USB power supply",
        steps=[
            "1  Switch off the boiler (and its power).",
            "2  Remove the front panel, unlock and lower the electrical box.",
            "3  On Z1 (rightmost pair): disconnect the old relay wires or remove the jumper. Keep them for fallback.",
            "4  Connect adapter J1 to Z1. Polarity does not matter (bridge D1–D4).",
            "5  Connect J2 to the ESP32: 1 GND · 2 GPIO22 · 3 3V3 · 4 GPIO21.",
            "6  Power the ESP32 from USB, then switch the boiler on. Check info 22 = 4.",
        ],
        note="Own drawing based on the ILEA installer manual (fig. 21). Terminal order may differ on other revisions – check the labels on your boiler.",
    ),
    "pl": dict(
        title="ACV ILEA – gdzie podłączyć adapter OpenTherm",
        box="Skrzynka elektryczna kotła\n(płyta przednia zdjęta, skrzynka opuszczona)",
        selv="Listwa SELV (bardzo niskie napięcie)",
        floor="Zabezp.\npodłogówki", outdoor="Czujnik\nzewn. 10 kΩ", z2="Z2\nstrefa 2", z1="Z1\nstrefa 1",
        jumper="mostek", remove="zdjąć mostek /\nprzewody przekaźnika",
        lv="Listwa 230 V (LV)\nNICZEGO TU NIE PODŁĄCZAĆ",
        cable="przewód 2-żyłowy 2×0,5–1,5 mm²\npolaryzacja dowolna",
        adapter="Adapter OpenTherm", bside="strona kotła", eside="strona ESP32", iso="przerwa izolacyjna",
        esp="ESP32 DevKit", usb="USB", psu="zasilacz USB 5 V",
        steps=[
            "1  Wyłącz kocioł (i jego zasilanie).",
            "2  Zdejmij płytę przednią, odblokuj i opuść skrzynkę elektryczną.",
            "3  Na Z1 (skrajna para po prawej): odepnij przewody przekaźnika albo zdejmij mostek. Zachowaj je na powrót.",
            "4  Podłącz J1 adaptera do Z1. Polaryzacja dowolna (mostek D1–D4).",
            "5  Połącz J2 z ESP32: 1 GND · 2 GPIO22 · 3 3V3 · 4 GPIO21.",
            "6  Zasil ESP32 z USB, potem włącz kocioł. Sprawdź info 22 = 4.",
        ],
        note="Rysunek własny na podstawie instrukcji instalatora ILEA (rys. 21). W innych wersjach kolejność zacisków może być inna – sprawdź opisy na kotle.",
    ),
}

FONT = "font-family=\"Inter,Segoe UI,Helvetica,Arial,sans-serif\""
MONO = "font-family=\"JetBrains Mono,Menlo,Consolas,monospace\""


def esc(s):
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def text(x, y, s, size=13, anchor="middle", weight=400, fill="#1f2a24", mono=False, lh=None):
    lines = s.split("\n")
    lh = lh or size * 1.25
    fam = MONO if mono else FONT
    out = [f'<text x="{x}" y="{y}" font-size="{size}" text-anchor="{anchor}" font-weight="{weight}" fill="{fill}" {fam}>']
    for i, ln in enumerate(lines):
        out.append(f'<tspan x="{x}" dy="{0 if i == 0 else lh}">{esc(ln)}</tspan>')
    out.append("</text>")
    return "".join(out)


def screw(cx, cy):
    return (f'<rect x="{cx-17}" y="{cy-22}" width="34" height="44" rx="3" fill="#2f7d4f" stroke="#1d5234"/>'
            f'<circle cx="{cx}" cy="{cy}" r="11" fill="#d9dde2" stroke="#6b7280" stroke-width="1.5"/>'
            f'<line x1="{cx-7}" y1="{cy-7}" x2="{cx+7}" y2="{cy+7}" stroke="#6b7280" stroke-width="2"/>')


def build(lang):
    t = T[lang]
    W, H = 1040, 720
    o = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}" role="img" aria-label="{esc(t["title"])}">',
         f'<rect width="{W}" height="{H}" fill="#ffffff"/>',
         text(30, 40, t["title"], 20, "start", 700)]

    # boiler electrical box
    o.append('<rect x="30" y="70" width="470" height="420" rx="12" fill="#f4f5f7" stroke="#9aa3ad" stroke-width="2"/>')
    o.append(text(50, 94, t["box"], 12, "start", 600, "#4b5563", lh=15))

    # SELV block
    o.append(text(60, 140, t["selv"], 13, "start", 700, "#1d5234"))
    o.append('<rect x="60" y="210" width="370" height="74" rx="6" fill="#e3efe7" stroke="#2f7d4f" stroke-width="1.5"/>')
    xs = [100, 140, 180, 220, 280, 320, 370, 410]
    for x in xs:
        o.append(screw(x, 247))
    labels = [(120, t["floor"], "#4b5563"), (200, t["outdoor"], "#4b5563"), (300, t["z2"], "#1f2a24"), (390, t["z1"], "#b42318")]
    for x, s, c in labels:
        o.append(text(x, 160, s, 11, "middle", 700 if s.startswith("Z") else 500, c))
    # pair dividers
    for x in (160, 250, 345):
        o.append(f'<line x1="{x}" y1="214" x2="{x}" y2="280" stroke="#2f7d4f" stroke-dasharray="3 3"/>')
    # Z2 jumper (stays)
    o.append('<path d="M280,236 C280,198 320,198 320,236" fill="none" stroke="#6b7280" stroke-width="4" stroke-linecap="round"/>')
    o.append(text(300, 302, t["jumper"], 11, "middle", 500, "#6b7280"))
    # Z1 highlight + removed jumper
    o.append('<rect x="349" y="204" width="82" height="86" rx="8" fill="none" stroke="#b42318" stroke-width="3"/>')
    o.append('<path d="M370,320 C370,350 410,350 410,320" fill="none" stroke="#9aa3ad" stroke-width="4" stroke-dasharray="5 4" stroke-linecap="round"/>')
    o.append('<line x1="378" y1="326" x2="402" y2="350" stroke="#b42318" stroke-width="3"/><line x1="402" y1="326" x2="378" y2="350" stroke="#b42318" stroke-width="3"/>')
    o.append(text(390, 372, t["remove"], 11, "middle", 600, "#b42318"))

    # LV block (danger)
    o.append('<defs><pattern id="hatch" width="10" height="10" patternUnits="userSpaceOnUse" patternTransform="rotate(45)">'
             '<rect width="10" height="10" fill="#fdecea"/><line x1="0" y1="0" x2="0" y2="10" stroke="#f5b5ad" stroke-width="4"/></pattern></defs>')
    o.append('<rect x="60" y="410" width="370" height="56" rx="6" fill="url(#hatch)" stroke="#b42318" stroke-width="1.5"/>')
    o.append('<path d="M84,452 l12,-22 l12,22 z" fill="#b42318"/><text x="96" y="449" font-size="13" text-anchor="middle" font-weight="800" fill="#fff" ' + FONT + '>!</text>')
    o.append(text(122, 433, t["lv"], 12, "start", 700, "#b42318", lh=16))

    # wires Z1 -> adapter J1
    w1 = "M370,226 L370,190 L540,190 L540,262 L606,262"
    w2 = "M410,226 L410,200 L520,200 L520,302 L606,302"
    o.append(f'<path d="{w1}" fill="none" stroke="#7a4a1f" stroke-width="5" stroke-linejoin="round" stroke-linecap="round"/>')
    o.append(f'<path d="{w2}" fill="none" stroke="#3b82f6" stroke-width="5" stroke-linejoin="round" stroke-linecap="round"/>')
    o.append(text(545, 146, t["cable"], 11.5, "middle", 500, "#374151"))

    # adapter
    o.append('<rect x="600" y="220" width="210" height="130" rx="8" fill="#ffffff" stroke="#374151" stroke-width="2"/>')
    o.append('<rect x="602" y="222" width="98" height="126" rx="6" fill="#fff2e5"/>')
    o.append('<rect x="712" y="222" width="96" height="126" rx="6" fill="#e8f0fb"/>')
    o.append('<line x1="706" y1="222" x2="706" y2="348" stroke="#374151" stroke-dasharray="4 4"/>')
    o.append(text(705, 210, t["adapter"], 14, "middle", 700))
    o.append(text(651, 340, t["bside"], 10.5, "middle", 500, "#9a4a0b"))
    o.append(text(760, 340, t["eside"], 10.5, "middle", 500, "#1e4f99"))
    o.append(text(706, 368, t["iso"], 10, "middle", 500, "#6b7280"))
    # J1
    o.append('<rect x="604" y="248" width="30" height="68" rx="3" fill="#2f6fd6" stroke="#1d4ea0"/>')
    for y in (262, 302):
        o.append(f'<circle cx="619" cy="{y}" r="9" fill="#d9dde2" stroke="#6b7280"/>')
    o.append(text(650, 287, "J1", 12, "start", 700, mono=True))
    # optocouplers hint
    for y in (252, 292):
        o.append(f'<rect x="686" y="{y}" width="40" height="26" rx="3" fill="#1f2937"/>')
    o.append(text(706, 246, "PC817", 9.5, "middle", 600, "#374151", mono=True))
    # J2 header
    pins = [(250, "1 GND", "#111827"), (270, "2 GPIO22", "#16a34a"), (290, "3 3V3", "#dc2626"), (310, "4 GPIO21", "#eab308")]
    o.append('<rect x="790" y="240" width="14" height="80" rx="2" fill="#111827"/>')
    o.append(text(782, 236, "J2", 12, "end", 700, mono=True))
    for y, lab, col in pins:
        o.append(f'<circle cx="797" cy="{y}" r="4" fill="#d4af37"/>')

    # ESP32
    o.append('<rect x="900" y="170" width="120" height="250" rx="8" fill="#1f3b2a" stroke="#0f2418" stroke-width="2"/>')
    o.append('<rect x="922" y="182" width="76" height="52" rx="4" fill="#c9ced6" stroke="#8a9097"/>')
    o.append(text(960, 213, "WROOM", 11, "middle", 700, "#374151", mono=True))
    o.append(text(960, 160, t["esp"], 14, "middle", 700))
    esp_pins = [(250, "GND"), (270, "D22"), (290, "3V3"), (310, "D21")]
    for (y, lab), (_, _, col) in zip(esp_pins, pins):
        o.append(f'<circle cx="908" cy="{y}" r="4" fill="#d4af37"/>')
        o.append(text(918, y + 4, lab, 11, "start", 700, "#e5f3ea", mono=True))
    for (y, lab, col) in pins:
        o.append(f'<path d="M797,{y} L908,{y}" stroke="{col}" stroke-width="4" stroke-linecap="round"/>')
        o.append(text(852, y - 4, lab, 10, "middle", 600, "#374151", mono=True))
    # USB
    o.append('<rect x="944" y="410" width="32" height="18" rx="3" fill="#9aa3ad" stroke="#4b5563"/>')
    o.append('<path d="M960,428 L960,470" stroke="#111827" stroke-width="5" stroke-linecap="round"/>')
    o.append('<rect x="878" y="470" width="156" height="40" rx="8" fill="#ffffff" stroke="#374151" stroke-width="1.5"/>')
    o.append(text(956, 494, t["psu"], 11.5, "middle", 600))

    # steps
    o.append('<rect x="30" y="530" width="980" height="148" rx="10" fill="#f7f8fa" stroke="#d6dbe1"/>')
    for i, s in enumerate(t["steps"]):
        o.append(text(50, 556 + i * 22, s, 13, "start", 500))
    o.append(text(30, 704, t["note"], 11, "start", 400, "#6b7280"))
    o.append("</svg>")
    return "\n".join(o)


if __name__ == "__main__":
    lang, out = sys.argv[1], sys.argv[2]
    with open(out, "w", encoding="utf-8") as f:
        f.write(build(lang))
