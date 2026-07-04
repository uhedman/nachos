def parse_trace(trace_file):
    references = []
    with open(trace_file, "r") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            if not line.startswith("[V]"):
                continue  # ignora las líneas de estadísticas y todo lo demás
            parts = line.split()
            references.append(int(parts[-1]))
    return references


def opt_replace(frames, future_uses, current_idx):
    """Elige el frame cuya próxima referencia es la más lejana (o nunca)."""
    farthest = -1
    victim = frames[0]

    for page in frames:
        # Buscar próximo uso de esta página a partir de current_idx
        try:
            next_use = future_uses[page].index(
                next(x for x in future_uses[page] if x > current_idx)
            )
            # Convertir a índice absoluto
            next_use = next(x for x in future_uses[page] if x > current_idx)
        except StopIteration:
            # Esta página nunca más se usa — víctima ideal
            return page

        if next_use > farthest:
            farthest = next_use
            victim = page

    return victim


def simulate_opt(trace_file, num_frames):
    references = parse_trace(trace_file)

    # Precalcular índices futuros de cada página
    future_uses = {}
    for idx, page in enumerate(references):
        if page not in future_uses:
            future_uses[page] = []
        future_uses[page].append(idx)

    frames = []  # páginas actualmente en memoria
    hits = 0
    misses = 0

    for i, page in enumerate(references):
        if page in frames:
            hits += 1
        else:
            misses += 1
            if len(frames) < num_frames:
                frames.append(page)
            else:
                victim = opt_replace(frames, future_uses, i)
                frames[frames.index(victim)] = page

    total = hits + misses
    print(f"Hits:     {hits}")
    print(f"Misses:   {misses}")
    print(f"Hit rate: {hits / total:.2%}")
    print(f"Miss rate:{misses / total:.2%}")


simulate_opt("code/userland/trace_sort.txt", num_frames=32)
