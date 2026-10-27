"""Just enough YAML for dsd's flat config files (key: value maps, and one list of such maps)."""


def _value(v):
    v = v.strip()
    if v in ('null', '~', ''):
        return None
    if v in ('true', 'false'):
        return v == 'true'
    try:
        return int(v, 0)
    except ValueError:
        return v.strip('"\'')


def load(path):
    out = {}
    for line in open(path, encoding='utf-8'):
        if line.startswith((' ', '-', '#')) or ':' not in line:
            continue
        k, v = line.split(':', 1)
        out[k.strip()] = _value(v)
    return out


def load_list(path, key):
    items, cur, inside = [], None, False
    for line in open(path, encoding='utf-8'):
        if not line.strip() or line.lstrip().startswith('#'):
            continue
        if not line.startswith((' ', '-')):
            inside = line.split(':', 1)[0].strip() == key
            continue
        if not inside:
            continue
        s = line.strip()
        if s.startswith('- '):
            cur = {}
            items.append(cur)
            s = s[2:]
        if cur is not None and ':' in s:
            k, v = s.split(':', 1)
            cur[k.strip()] = _value(v)
    return items
