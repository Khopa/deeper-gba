#!/usr/bin/env python3
"""Count the puzzles of every committed bank, per size and per difficulty.

usage: bank_report.py [data/puzzles] [--markdown] [-o docs/banks.md]

Reads the DPZ1 files (docs/puzzle_bank.md) and prints one table per family
plus a summary; --markdown formats Markdown tables, -o writes the report
(UTF-8) to a file instead of the console.
"""
import os
import struct
import sys

FAMILIES = {0: "dig", 1: "vein", 2: "block", 3: "tunnel", 4: "ledger", 5: "nugget", 6: "heart"}
HEADER = ("# Contenu des banques de puzzles\n\n"
          "Généré par `python tools/bank_report.py --markdown -o docs/banks.md` après `make puzzles` "
          "(graine 20260913). Lignes = taille de grille, colonnes = difficulté 1 à 10.\n\n")


def read_bank(path):
    data = open(path, "rb").read()
    if data[:4] != b"DPZ1":
        raise SystemExit(f"{path}: not a DPZ1 bank")
    family = data[4]
    count = struct.unpack_from("<H", data, 6)[0]
    records = []
    for i in range(count):
        off = struct.unpack_from("<I", data, 32 + 4 * i)[0]
        fam, size, diff, flags = data[off:off + 4]
        records.append((size, diff, flags))
    return family, records, len(data)


def report(folder, markdown):
    banks = []
    for f in sorted(os.listdir(folder)):
        if f.endswith(".bin"):
            banks.append((f, *read_bank(os.path.join(folder, f))))
    out = []
    total = 0
    diffs = list(range(1, 11))
    for name, family, records, nbytes in banks:
        sizes = sorted({r[0] for r in records})
        total += len(records)
        title = f"{FAMILIES.get(family, family)} - {len(records)} puzzles, {nbytes} bytes, sizes {', '.join(map(str, sizes))}"
        if markdown:
            out.append(f"### {title}\n")
            out.append("| size \\ difficulty | " + " | ".join(map(str, diffs)) + " | total |")
            out.append("|---|" + "---|" * (len(diffs) + 1))
        else:
            out.append(title)
            out.append("  size  " + " ".join(f"d{d:<3}" for d in diffs) + " total")
        for s in sizes:
            row = [sum(1 for r in records if r[0] == s and r[1] == d) for d in diffs]
            if markdown:
                out.append(f"| {s}x{s} | " + " | ".join(str(v) if v else "." for v in row) + f" | {sum(row)} |")
            else:
                out.append(f"  {s:>2}x{s:<2} " + " ".join(f"{v:<4}" for v in row) + f" {sum(row)}")
        col = [sum(1 for r in records if r[1] == d) for d in diffs]
        if markdown:
            out.append("| all | " + " | ".join(str(v) if v else "." for v in col) + f" | {len(records)} |\n")
        else:
            out.append("  all   " + " ".join(f"{v:<4}" for v in col) + f" {len(records)}")
            out.append("")
    summary = f"{total} puzzles in {len(banks)} banks (+ the nugget rooms, generated at run time)"
    out.append(("**" + summary + "**") if markdown else summary)
    text = "\n".join(out) + "\n"
    return (HEADER + text) if markdown else text, summary


def main():
    argv = sys.argv[1:]
    out_path = None
    if "-o" in argv:
        i = argv.index("-o")
        out_path = argv[i + 1]
        del argv[i:i + 2]
    markdown = "--markdown" in argv
    args = [a for a in argv if not a.startswith("--")]
    folder = args[0] if args else os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "data", "puzzles")
    text, summary = report(folder, markdown)
    if out_path:
        with open(out_path, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
        print(f"{summary} -> {out_path}")
    else:
        print(text, end="")
    return 0


if __name__ == "__main__":
    sys.exit(main())
