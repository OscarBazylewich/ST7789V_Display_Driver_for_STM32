#!/usr/bin/env python3
"""The checker examines the text of a file and shows possible STE problems
(ASD-STE100 Simplified Technical English, Issue 9).

The checker reads the dictionary from ASD-STE100_ISSUE9.txt and the project
terms from ste-glossary.md. These two files are in the same folder as the
checker. In .md files, the checker examines the text that is not code. In
.py, .ps1, .sh, .txt and .cmake files, it examines only the comments after #.
In all other files, for example C files, it examines only the comments
between /* and */ and after //.

The checker shows these possible problems:
  - Words that are not approved and that are not in the project glossary
  - Sentences with more than 20 words (procedures) or 25 words (descriptions)
  - Paragraphs with more than 6 sentences
  - Semicolons, contractions, and words with "-ing" at the end
  - Possible verbs in the passive voice

The checker cannot find the part of speech or the meaning of a word. Thus,
examine each result. To find an approved word, use the dictionary.

To use the checker:
  python Docs/ste_check.py FILE...          all results
  python Docs/ste_check.py --words FILE...  only the words, with the number
                                            of results for each word
  python Docs/ste_check.py --lookup WORD... dictionary data for words
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DICT_TXT = os.path.join(HERE, 'ASD-STE100_ISSUE9.txt')
GLOSSARY = os.path.join(HERE, 'ste-glossary.md')

POS = r'(n|v|adj|adv|prep|conj|pron|art|TN|TV)'


# ---- dictionary ---------------------------------------------------------

def load_dictionary(path):
    """This function reads the dictionary from the text copy of ASD-STE100
    and returns (approved, forms, not_approved)."""
    lines = open(path, encoding='utf-8', errors='replace').read().split('\n')
    entry_re = re.compile(r"^([A-Za-z][A-Za-z\-' ,/.]*?)\s*\(" + POS + r"\)\s*,?\s*$")
    approved, forms, not_approved = {}, {}, {}
    offset, pending, base = None, None, None
    try:
        start = next(i for i, l in enumerate(lines) if re.match(r'^Word\s+Approved meaning/', l))
    except StopIteration:
        sys.exit('ste_check: there is no dictionary table in ' + path)
    for line in lines[start:]:
        m = re.match(r'^Word(\s+)Approved meaning/', line)
        if m:
            offset, base = len('Word') + len(m.group(1)), None
            continue
        if line.startswith('Issue 9') or re.match(r'^Page \d', line) or 'Simplified Technical English' in line:
            base = None
            continue
        if offset is None:
            continue
        col = line[:offset].strip()
        if not col or col.startswith('(part of speech)'):
            continue
        if pending and re.match(r'^\(' + POS + r'\)', col):
            col, pending = pending + ' ' + col, None
        m = entry_re.match(col)
        if not m and not line.startswith(' '):
            # In some rows, the meaning or the alternative starts only one space
            # after the word and its part of speech: "ADJUSTMENT (n) The effect"
            m2 = re.match(r"^([A-Za-z][A-Za-z\-' ,/.]*?)\s*\(" + POS + r"\)(,?)(\s|$)", line)
            if m2 and m2.end() <= offset + 4:
                m = entry_re.match(line[:m2.end()].strip())
        if m:
            word, pos = m.group(1).strip(), m.group(2)
            key = word.lower()
            if word.upper() == word:
                approved.setdefault(key, set()).add(pos)
                for part in key.split():
                    approved.setdefault(part, set()).add(pos)
                base = key
            else:
                not_approved.setdefault(key, set()).add(pos)
                base = None
            continue
        # the verb forms of an approved word in the rows below it (also FASTER
        # for FAST): "SENDS," "IS, WAS" "(also ARE, WERE)" "(FASTER,"
        f = re.sub(r'^\((also\s+)?|\)$', '', col).strip()
        if base and re.match(r"^[A-Z][A-Z\-']*(,\s*[A-Z][A-Z\-']*)*,?\)?$", f):
            for form in re.findall(r"[A-Z][A-Z\-']*", f):
                forms[form.lower()] = base
            continue
        if re.match(r"^[A-Z][A-Z\-' ]+$", col):         # "IN PROGRESS" with "(adv)" in the next row
            pending = col
            continue
        if re.match(r'^[a-z]', col):      # text that gives more information, for example "No other verb forms."
            continue
        base = None
    return approved, forms, not_approved


def load_glossary(path):
    """This function returns {word: (type, term)} for each term in the glossary
    tables and for each name in the "Other forms" part of its row. The result
    also contains each word of these names."""
    terms = {}
    if not os.path.exists(path):
        return terms
    for line in open(path, encoding='utf-8'):
        cells = [c.strip() for c in line.strip().strip('|').split('|')]
        if len(cells) < 3 or cells[1] not in ('TN', 'TV'):
            continue
        kind, names = cells[1], [cells[0]] + [x for x in re.split(r'[,;]', cells[2]) if x.strip()]
        for name in names:
            name = name.strip().strip('`').lower()
            for word in re.findall(r"[a-z][a-z'\-]*", name):
                terms.setdefault(word, (kind, cells[0].strip('`')))
            if name:
                terms.setdefault(name, (kind, cells[0].strip('`')))
    return terms


# ---- the text that the checker examines ---------------------------------

CODE_LINE = re.compile(r'(;\s*$|[{}]\s*$|^\s*(#\s*(if|ifdef|ifndef|else|endif|define|include)|static |const |uint|int |void |for \(|while \(|return )|->|\w+\[[^\]]*\]\s*=|^\s*[A-Za-z_]\w*\s*\(.*\)\s*;?\s*$)')


def comment_blocks_c(text):
    """This function gives, one at a time, (number of the first row, [rows])
    for each comment in a C file."""
    lines = text.split('\n')
    i = 0
    while i < len(lines):
        line = lines[i]
        m = re.search(r'/\*', line)
        n = re.search(r'//', line)
        if n and (not m or n.start() < m.start()) and not re.search(r'"[^"]*//', line):
            block, start = [], i
            while i < len(lines) and re.match(r'^\s*//', lines[i]) or (i == start):
                part = lines[i][lines[i].find('//') + 2:]
                block.append(part.strip())
                i += 1
                if i < len(lines) and not re.match(r'^\s*//', lines[i]):
                    break
            yield start + 1, block
            continue
        if m:
            start, block = i, []
            rest = line[m.end():]
            while True:
                end = rest.find('*/')
                if end >= 0:
                    block.append(rest[:end])
                    break
                block.append(rest)
                i += 1
                if i >= len(lines):
                    break
                rest = lines[i]
            cleaned = [re.sub(r'^\s*\*(?!/)\s?', '', b).rstrip() for b in block]
            yield start + 1, cleaned
            i += 1
            continue
        i += 1


def comment_blocks_hash(text):
    lines = text.split('\n')
    i = 0
    while i < len(lines):
        if re.match(r'^\s*#', lines[i]):
            start, block = i, []
            while i < len(lines) and re.match(r'^\s*#', lines[i]):
                block.append(re.sub(r'^\s*#+\s?', '', lines[i]))
                i += 1
            yield start + 1, block
            continue
        m = re.search(r'\s#\s', lines[i])
        if m:
            yield i + 1, [lines[i][m.end():]]
        i += 1


def markdown_blocks(text):
    lines = text.split('\n')
    i, fence = 0, False
    block, start = [], 0
    for i, line in enumerate(lines):
        if re.match(r'^\s*```', line):
            if block:
                yield start, block
                block = []
            fence = not fence
            continue
        if fence:
            continue
        if not line.strip():
            if block:
                yield start, block
                block = []
            continue
        if not block:
            start = i + 1
        block.append(line)
    if block:
        yield start, block


def prose_lines(path):
    """This function gives (row number, text) for the rows of a file that the
    checker examines. It gives the rows one at a time and in sequence. A text
    value of None shows the end of a paragraph."""
    text = open(path, encoding='utf-8', errors='replace').read()
    ext = os.path.splitext(path)[1].lower()
    name = os.path.basename(path).lower()
    if ext == '.md':
        blocks = markdown_blocks(text)
    elif ext in ('.txt', '.cmake') or name == 'cmakelists.txt' or ext in ('.py', '.ps1', '.sh'):
        blocks = comment_blocks_hash(text)
    else:
        blocks = comment_blocks_c(text)
    for start, block in blocks:
        for k, line in enumerate(block):
            if ext != '.md' and CODE_LINE.search(line):
                yield start + k, None
                continue
            if ext == '.md':
                line = re.sub(r'^\s*\|?\s*-{3,}.*$', '', line)          # remove the "|---|---|" row of a table
                line = line.replace('|', '. ')                           # each item of a table row starts a new sentence
            if not line.strip():
                yield start + k, None
                continue
            yield start + k, line
        yield None, None


# ---- sentences and words ------------------------------------------------

def sentences(path):
    """This function gives, one at a time, (row number, sentence, paragraph
    number) for each sentence of a file."""
    para, buf, buf_line = 0, [], None

    def flush():
        nonlocal buf, buf_line
        if buf:
            text = ' '.join(buf)
            for s, off in split_sentences(text):
                yield buf_line, s, para
        buf, buf_line = [], None

    for ln, line in prose_lines(path):
        if line is None:
            yield from flush()
            para += 1
            continue
        # a row that starts with "#", "-", "*", "+", a number or ">" starts a new sentence
        if re.match(r'^\s*(#{1,6}\s|[-*+]\s|\d+[.)]\s|>\s?)', line):
            yield from flush()
            para += 1                     # rule 6.6 gives a limit for the sentences of a paragraph, not for a list
            line = re.sub(r'^\s*(#{1,6}\s|[-*+]\s|\d+[.)]\s|>\s?)', '', line)
        if buf_line is None:
            buf_line = ln
        buf.append(line.strip())
    yield from flush()


def split_sentences(text):
    # Replace each `...` and each (...) with X characters. Then a "." in that
    # text does not show the end of a sentence.
    masked = re.sub(r'`[^`]*`', lambda m: 'X' * len(m.group(0)), text)
    masked = re.sub(r'\([^()]*\)', lambda m: 'X' * len(m.group(0)), masked)
    masked = re.sub(r'\b(e\.g|i\.e|etc|vs|approx|Fig|No)\.', lambda m: m.group(0).replace('.', 'X'), masked)
    masked = re.sub(r'(\d)\.(\d)', r'\1X\2', masked)
    out, start = [], 0
    for m in re.finditer(r'[.!?:](?=\s+|$)', masked):
        out.append((text[start:m.end()].strip(), start))
        start = m.end()
    if text[start:].strip():
        out.append((text[start:].strip(), start))
    return [o for o in out if re.search(r'[A-Za-z]', o[0])]


def word_count(sentence):
    """This function counts the words of a sentence. It counts each of these
    items as one word (rules 8.5 to 8.7): text in ( ) or in "...", code, a
    number with its unit, and a word with a "-" character."""
    s = re.sub(r'`[^`]*`', ' W ', sentence)
    s = re.sub(r'\([^()]*\)', ' W ', s)
    s = re.sub(r'"[^"]*"', ' W ', s)
    s = re.sub(r'\b\d[\d.,x]*\s*(%|[a-zA-Zµ]{1,4}\b)?', ' W ', s)
    return len(re.findall(r"[A-Za-z0-9µ][\w'\-./]*", s))


IDENT = re.compile(r'(_|\d|[a-z][A-Z]|^[A-Z]{2,}|\(\))')
# Units and their symbols, for example ms and MHz, are technical nouns (rule 1.5)
UNITS = {'ms', 'us', 'µs', 'ns', 's', 'mhz', 'khz', 'hz', 'ghz', 'kb', 'mb', 'kib', 'mib', 'b',
         'fps', 'px', 'v', 'ma', 'mv', 'mw', 'mm', 'cm', 'bps', 'kbps', 'mbps', 'cycles'}
CONTRACTION = re.compile(r"\b\w+(n't|'re|'ll|'ve|'d|'m)\b|\b(it's|that's|there's|here's|what's|let's)\b", re.I)
PASSIVE = re.compile(r"\b(is|are|was|were|be|been|being)\s+(\w+ly\s+)?(\w+ed|built|done|made|sent|set|kept|given|taken|shown|known|read|written|drawn|found|held|left|lost|put|run|seen|told|thrown)\b", re.I)


def words_of(sentence):
    s = re.sub(r'`[^`]*`', ' ', sentence)
    # text in "..." or “...”: a name, a label or a word that the text gives as
    # an example (rule 8.6)
    s = re.sub(r'"[^"]*"|“[^”]*”', ' ', s)
    s = re.sub(r'https?://\S+', ' ', s)
    s = re.sub(r'\[([^\]]*)\]\([^)]*\)', r'\1', s)
    s = re.sub(r'<[^>]+>', ' ', s)
    for tok in re.findall(r"[A-Za-z][A-Za-z0-9_'’\-]*(?:\(\))?", s):
        tok = tok.replace('’', "'")
        if IDENT.search(tok) or len(tok) == 1 and tok not in ('a', 'A', 'I') or tok.lower() in UNITS:
            continue
        yield tok


class Checker:
    def __init__(self):
        self.approved, self.forms, self.not_approved = load_dictionary(DICT_TXT)
        self.glossary = load_glossary(GLOSSARY)

    def lookup(self, word):
        """This function returns (status, detail). The value of status is 'ok',
        'glossary', 'not-approved' or 'unknown'."""
        w = word.lower().strip("'-")
        if w.endswith("'s"):
            w = w[:-2]
        candidates = [w]
        if w.endswith('ies'):
            candidates.append(w[:-3] + 'y')
        if w.endswith('es'):
            candidates.append(w[:-2])
        if w.endswith('s'):
            candidates.append(w[:-1])
        for c in candidates:
            if c in self.approved or c in self.forms:
                return 'ok', ''
            if c in self.glossary:
                return 'glossary', self.glossary[c][0]
        if w in self.not_approved:
            return 'not-approved', '/'.join(sorted(self.not_approved[w]))
        for c in candidates[1:]:
            if c in self.not_approved:
                return 'not-approved', '/'.join(sorted(self.not_approved[c]))
        return 'unknown', ''

    def check_word(self, tok):
        parts = [p for p in tok.split('-') if p] if '-' in tok else [tok]
        out = []
        for p in parts:
            status, detail = self.lookup(p)
            if status in ('ok', 'glossary'):
                continue
            if p.lower().endswith('ing') and status != 'not-approved':
                out.append(('3.5', p, '"-ing" verb form: use it only in a technical noun'))
            elif status == 'not-approved':
                out.append(('1.1', p, 'not approved (%s): find the approved alternative in the dictionary' % detail))
            else:
                out.append(('1.1', p, 'not in the dictionary or the glossary. Use an approved word. If the word is a technical noun or a technical verb, add it to ste-glossary.md'))
        return out

    def check_file(self, path):
        results = []
        para_count = {}
        for line, sent, para in sentences(path):
            para_count.setdefault(para, []).append(line)
            n = word_count(sent)
            if n > 25:
                results.append((line, '6.3', 'the sentence has %d words (maximum 25 in a description, 20 in a procedure): %s' % (n, sent[:70])))
            elif n > 20:
                results.append((line, '5.1', 'the sentence has %d words (maximum 20 in a procedure): %s' % (n, sent[:70])))
            plain = re.sub(r'"[^"]*"|“[^”]*”', ' ', re.sub(r'`[^`]*`', '', sent))   # without code (`...`) and without text in "..." or “...”
            if ';' in plain:
                results.append((line, '8.1', 'semicolon: %s' % sent[:70]))
            for m in CONTRACTION.finditer(plain):
                results.append((line, '4.2', 'contraction: ' + m.group(0)))
            for m in PASSIVE.finditer(plain):
                results.append((line, '3.6', 'possible verb in the passive voice: "%s"' % m.group(0)))
            for tok in words_of(sent):
                for rule, word, msg in self.check_word(tok):
                    results.append((line, rule, '"%s" %s' % (word, msg)))
        for para, lines in para_count.items():
            if len(lines) > 6:
                results.append((lines[0], '6.6', 'the paragraph has %d sentences (maximum 6)' % len(lines)))
        return sorted(results, key=lambda r: (r[0] or 0))


def main(argv):
    if not argv or argv[0] in ('-h', '--help'):
        print(__doc__)
        return 0
    chk = Checker()
    if argv[0] == '--lookup':
        for w in argv[1:]:
            status, detail = chk.lookup(w)
            extra = ''
            if w.lower() in chk.approved:
                extra = ' approved as ' + '/'.join(sorted(chk.approved[w.lower()]))
            if w.lower() in chk.forms:
                extra += ' from the dictionary word ' + chk.forms[w.lower()]
            if w.lower() in chk.not_approved:
                extra += ' not approved as ' + '/'.join(sorted(chk.not_approved[w.lower()]))
            print('%-16s %s%s' % (w, status, extra))
        return 0
    only_words = argv[0] == '--words'
    files = argv[1:] if only_words else argv
    total = 0
    counts = {}
    for path in files:
        res = chk.check_file(path)
        total += len(res)
        for line, rule, msg in res:
            if only_words:
                m = re.match(r'"([^"]+)" (not approved|not in the dictionary|"-ing")', msg)
                if m:
                    counts.setdefault(m.group(1).lower(), [0, msg.split('" ', 1)[1][:40], set()])
                    counts[m.group(1).lower()][0] += 1
                    counts[m.group(1).lower()][2].add(os.path.basename(path))
            else:
                print('%s:%s: [rule %s] %s' % (path, line, rule, msg))
    if only_words:
        for w, (n, why, fs) in sorted(counts.items(), key=lambda x: (-x[1][0], x[0])):
            print('%4d  %-20s %-42s %s' % (n, w, why, ','.join(sorted(fs))))
    else:
        print('%d results' % total, file=sys.stderr)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
