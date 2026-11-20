"""Verify the relinks and summarize diagnostic timings; do not evaluate G1."""
import csv
import re
import statistics
import subprocess
import sys
from pathlib import Path

SHIFTS = tuple(range(0, 113, 16))
KINDS = ('control', 'direct')
SCENES = ('uniform', 'clustered')
ENGINES = ('index', 'brute')
SYMBOLS = {
    'index_runner': r'Runner<bench::LibraryIndex<int, false>, int>::step\(',
    'brute_runner': r'Runner<bench::BruteForce<int>, int>::step\(',
    'index_pairs': r'^bench::Counters bench::LibraryIndex<int, false>::pairs<.*::PairSink>',
    'index_build': r'^QuadTree::Status .*::buildPrepared<false, false, std::vector<QuadTree::BuildEntry<unsigned int, int, QuadTree::Box<int> >',
}
SIGNATURE = ('scene_hash', 'pairs', 'checksum', 'candidates', 'node_visits', 'cell_tests', 'references', 'max_refs')


def get_symbols(path):
    symbols = {}
    for line in path.read_text().splitlines():
        m = re.fullmatch(r'([0-9a-f]+) ([0-9a-f]+) t (.*)', line)
        if not m or '[clone .cold]' in m[3]:
            continue
        for label, expr in SYMBOLS.items():
            if re.search(expr, m[3]):
                assert label not in symbols, (m[3], symbols)
                symbols[label] = int(m[1], 16), int(m[2], 16)
    assert symbols.keys() == SYMBOLS.keys(), path
    return symbols


def instruction_sequence(binary, start, size):
    asm = subprocess.check_output(['objdump', '-d', '--no-show-raw-insn',
                                   f'--start-address={start}', f'--stop-address={start + size}', binary], text=True)
    insns = []
    for line in asm.splitlines():
        m = re.match(r'^\s*([0-9a-f]+):\s*(.+)$', line)
        if not m:
            continue
        offset, insn = int(m[1], 16) - start, m[2]
        insn = re.sub(r'0x[0-9a-f]+\(%rip\)', 'DISP(%rip)', insn)
        insn = re.sub(r'(?<=\s)[0-9a-f]+ (?=<)', 'TARGET ', insn)
        insn = re.sub(r'\s+#\s+[0-9a-f]+', ' # TARGET', insn)
        insns.append((offset, insn))
    assert insns and insns[0][0] == 0
    return insns


def main(directory):
    with (directory / 'placement.csv').open('w', newline='') as file:
        out = csv.writer(file, lineterminator='\n')
        out.writerow(['kind', 'shift_bytes', 'symbol', 'address', 'size_bytes', 'offset_in_64b_line', 'offset_in_4k_page'])
        for kind in KINDS:
            stage = directory / kind
            origin = get_symbols(stage / 'bench-0.nm')
            code = {label: instruction_sequence(stage / 'bench-0', *symbol) for label, symbol in origin.items()}
            for shift in SHIFTS:
                for label, (addr, size) in get_symbols(stage / f'bench-{shift}.nm').items():
                    assert (addr - shift, size) == origin[label], (kind, shift, label)
                    assert instruction_sequence(stage / f'bench-{shift}', addr, size) == code[label], (kind, shift, label)
                    out.writerow([kind, shift, label, hex(addr), size, addr % 64, addr % 4096])

    timings, signatures = {}, {}
    with (directory / 'round-summary.csv').open('w', newline='') as file:
        out = csv.writer(file, lineterminator='\n')
        out.writerow(['replicate', 'kind', 'shift_bytes', 'scene', 'n', 'engine', 'estimate_ns', 'error_ns', 'round_error_ns'])
        for rep in range(1, 4):
            for kind in KINDS:
                for shift in SHIFTS:
                    run = directory / f'{rep}-{kind}-{shift}'
                    with (run / 'snapshot.csv').open() as f:
                        rows = {(r['scene'], int(r['n']), r['engine']): r for r in csv.DictReader(line for line in f if not line.startswith('#'))}
                    assert rows.keys() == {(s, n, e) for s in SCENES for n in (0, 1) for e in ENGINES}, run
                    for key, row in rows.items():
                        s, n, e = key
                        assert row['status'] == 'ok' and row['batch'] == '256' and row['rounds'] == '67', run
                        assert row['allocs_per_step'] == '0.000', (run, key)
                        sig = tuple(row[k] for k in SIGNATURE)
                        assert sig == signatures.setdefault(key, sig), (run, key)
                        assert sig[:3] == tuple(rows[s, n, 'brute'][k] for k in SIGNATURE[:3]), (run, key)
                        timings[rep, kind, shift, s, n, e] = float(row['estimate_ns'])
                        out.writerow([rep, kind, shift, *key, row['estimate_ns'], row['err_ns'], row['round_err_ns']])

    with (directory / 'placement-timings.csv').open('w', newline='') as file:
        out = csv.writer(file, lineterminator='\n')
        out.writerow(['kind', 'shift_bytes', 'scene', 'n', 'index_median_ns', 'brute_median_ns',
                      'median_same_run_index_over_brute', 'min_same_run_ratio', 'max_same_run_ratio'])
        for kind in KINDS:
            for shift in SHIFTS:
                for s in SCENES:
                    for n in (0, 1):
                        pair = [[timings[rep, kind, shift, s, n, e] for rep in range(1, 4)] for e in ENGINES]
                        ratios = [i / b for i, b in zip(*pair)]
                        out.writerow([kind, shift, s, n, *(f'{statistics.median(v):.1f}' for v in pair),
                                      *(f'{v:.3f}' for v in (statistics.median(ratios), min(ratios), max(ratios)))])


if __name__ == '__main__':
    main(Path(sys.argv[1]))
