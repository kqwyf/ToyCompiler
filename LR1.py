import sys
import numpy as np

# gen structure: (left, (right1, right2, ...))
# item structure: (gen#, dot_location, succeeding_symbol_set)

START_SYMBOL = 'S'
END_SYMBOL = '#'

syms = []
gens = []
stats = [] # list((gen#, dot_location) -> set(symbol#))
empty = set() # set(symbol#)
group = dict() # stat# -> (symbol#(-1 for reduction) -> set((gen#, dot_location))
shift = dict() # (stat#, symbol#) -> stat#
reduc = dict() # (stat#, symbol#) -> set(gen#)
FIRST = dict() # symbol# -> set(symbol#)
FOLLOW = dict() # symbol# -> set(symbol#)
smap = dict() # symbol -> symbol#
gmap = dict() # symbol# -> list(gen#)
imap = dict() # sorted_tuple((gen#, dot_location, sorted_succeeding_symbol_tuple)) -> stat#

def element(s): # get an arbitrary element from s
    for i in s:
        return i

def g2s(g):
    return syms[gens[g][0]] + " -> " + " ".join([syms[s] for s in gens[g][1]])

def i2s(i, suc):
    right = [syms[s] for s in gens[i[0]][1]]
    right = right[:i[1]] + ["@"] + right[i[1]:]
    return syms[gens[i[0]][0]] + " -> " + " ".join(right) + "; " + "/".join([syms[s] for s in suc])

def addId(line):
    global startGen, syms, gens, stats, FIRST, shift, reduc, smap, gmap, imap
    ids = line.split()
    for i in range(len(ids)):
        smap[ids[i]] = i
        syms += [ids[i]]

def read(f):
    global startGen, syms, gens, stats, FIRST, shift, reduc, smap, gmap, imap
    if START_SYMBOL not in smap:
        smap[START_SYMBOL] = len(syms)
        syms += [START_SYMBOL]
    if END_SYMBOL not in smap:
        smap[END_SYMBOL] = len(syms)
        syms += [END_SYMBOL]
    isFirst = True
    for line in f.readlines():
        l = line.split()
        if l[0] != '|':
            first = l[0]
            if first not in smap:
                smap[first] = len(syms)
                syms += [first]
            start = 2
            gmap[smap[first]] = []
        else:
            start = 1
        fs = smap[first]
        if isFirst:
            isFirst = False
            gmap[smap[START_SYMBOL]] = [len(gens)]
            startGen = len(gens)
            gens += [(smap[START_SYMBOL], tuple([fs]))]
        if len(l) <= start: # empty-able gen
            empty.add(fs)
            continue
        right = []
        for w in l[start:]:
            if w not in smap:
                smap[w] = len(syms)
                syms += [w]
            right += [smap[w]]
        gmap[fs] += [len(gens)]
        gens += [tuple([fs, tuple(right)])]

def solveFIRST(sym, visited):
    global startGen, syms, gens, stats, FIRST, shift, reduc, smap, gmap, imap
    if sym in visited:
        return set()
    if sym in FIRST:
        return FIRST[sym]
    if sym not in gmap: # terminal
        FIRST[sym] = set([sym])
        return FIRST[sym]
    visited.add(sym)
    result = set()
    for g in gmap[sym]:
        for s in gens[g][1]:
            result |= solveFIRST(s, visited)
            if s not in empty:
                break
    FIRST[sym] = result
    visited.remove(sym)
    return result

def solveFOLLOW():
    adj = np.zeros((len(syms), len(syms)))
    for s in range(len(syms)):
        FOLLOW[s] = set()
    FOLLOW[smap[START_SYMBOL]].add(smap[END_SYMBOL])
    # build the FOLLOW dependency graph
    # the FIRST data can be regarded as the input to some nodes of this graph
    for gen in gens:
        for symi in range(len(gen[1]) - 1):
            a, b = gen[1][symi: symi + 2]
            FOLLOW[a] |= FIRST[b]
            if b in empty:
                adj[a, b] = 1 # FOLLOW[a] depends on FOLLOW[b]
        adj[gen[1][-1], gen[0]] = 1 # FOLLOW[last_symbol] depends on FOLLOW[left_symbol]
    # floyd algorithm
    for i in range(len(syms)):
        for j in range(len(syms)):
            for k in range(len(syms)):
                if adj[i, k] == 1 and adj[k, j] == 1:
                    adj[i, j] = 1
    # collect the FIRST data into FOLLOWs
    for i in range(len(syms)):
        for j in range(len(syms)):
            if adj[i, j] == 1:
                FOLLOW[i] |= FOLLOW[j]

def add(si, item):
    global startGen, syms, gens, stats, FIRST, shift, reduc, smap, gmap, imap
    if item[:2] in stats[si] and item[2].issubset(stats[si][item[:2]]):
        return
    if item[:2] in stats[si]:
        item[2] |= stats[si][item[:2]]
    else:
        stats[si][item[:2]] = item[2]
    tmp = [item]
    i = 0
    if si not in group:
        group[si] = dict()
    while i < len(tmp):
        gen = gens[tmp[i][0]]
        if tmp[i][1] == len(gen[1]): # reduction
            if -1 not in group[si]:
                group[si][-1] = set()
            group[si][-1] |= set([tmp[i][:2]])
            i += 1
            continue
        sym = gen[1][tmp[i][1]] # the symbol to be expanded
        if sym not in group[si]:
            group[si][sym] = set()
        group[si][sym] |= set([tmp[i][:2]])
        if sym not in gmap: # terminal symbol
            i += 1
            continue
        if tmp[i][1] + 1 < len(gen[1]):
            suc = set()
            for j in range(tmp[i][1] + 1, len(gen[1])):
                suc |= FIRST[gen[1][j]]
                if gen[1][j] not in empty:
                    break
            else:
                suc |= tmp[i][2]
        else:
            suc = set(tmp[i][2])
        for g in gmap[sym]:
            newItem = (g, 0, suc)
            if newItem[:2] not in stats[si]:
                stats[si][newItem[:2]] = set(newItem[2])
                tmp += [newItem]
            elif not newItem[2] <= stats[si][newItem[:2]]:
                stats[si][newItem[:2]] |= newItem[2]
                tmp += [newItem]
        i += 1

def dfs(si):
    global startGen, syms, gens, stats, FIRST, shift, reduc, smap, gmap, imap
    for sym in group[si]:
        if sym == -1: # reduction
            for g, dot in group[si][sym]:
                for t in stats[si][g, dot]:
                    key = (si, t)
                    if key not in reduc:
                        reduc[key] = set()
                    reduc[key] |= set([g])
        else: # non-reduction
            key = (si, sym)
            tmp = tuple(sorted([(g, d, tuple(sorted(list(stats[si][g, d])))) for g,d in group[si][sym]]))
            if tmp in imap:
                shift[key] = imap[tmp]
            else:
                shift[key] = len(stats)
                stats += [dict()]
                for g, dot in group[si][sym]:
                    newItem = (g, dot + 1, set(stats[si][g, dot]))
                    add(shift[key], newItem)
                imap[tmp] = shift[key]
                dfs(shift[key])

def process():
    global startGen, syms, gens, stats, FIRST, shift, reduc, smap, gmap, imap
    assert len(gmap[smap[START_SYMBOL]]) == 1
    for s in range(len(syms)):
        solveFIRST(s, set())
    solveFOLLOW()
    startStat = len(stats)
    stats += [dict()]
    add(startStat, tuple([gmap[smap[START_SYMBOL]][0], 0, tuple([smap[END_SYMBOL]])])); ### S -> .A; #
    dfs(startStat)

def show_human():
    global startGen, syms, gens, stats, FIRST, shift, reduc, smap, gmap, imap
    print("Symbol total: " + str(len(syms)))
    print("Status total: " + str(len(stats)))
    corruption = 0
    for key in shift.keys() | reduc.keys():
        l1 = 1 if key in shift else 0
        l2 = len(reduc[key]) if key in reduc else 0
        corruption += 1 if l1 + l2 > 1 else 0
    print("Corruption total: %d"%corruption)
    print("\nSymbols:\n")
    for i in range(len(syms)):
        print("'%s': %d"%(syms[i], i))
    print("\nFIRST:\n")
    for s in range(len(syms)):
        print(syms[s] + ": " + str([syms[i] for i in FIRST[s]]))
    print("\nStatus:\n")
    for i in range(len(stats)):
        print("I%d:"%(i))
        for item in stats[i]:
            print(i2s(item, stats[i][item]))
        print("")
    print("\nTable:\n")
    term = [i for i in range(len(syms)) if i not in gmap]
    nterm = [i for i in range(len(syms)) if i in gmap]
    print("      ", end='')
    for s in term:
        print(syms[s][:3] + " "*(5-min(len(syms[s]), 3)), end='')
    for s in nterm:
        if s == smap[START_SYMBOL]:
            continue
        print(syms[s][:3] + " "*(5-min(len(syms[s]), 3)), end='')
    print("")
    for i in range(len(stats)):
        print("I%-3d  "%i, end='')
        for s in term:
            if (i,s) in shift and (i,s) in reduc:
                print("*    ", end='')
            elif (i,s) in shift and (i,s) not in reduc:
                print("s%-3d "%shift[i,s], end='')
            elif (i,s) not in shift and (i,s) in reduc:
                if len(reduc[i,s]) > 1:
                    print("*    ", end='')
                else:
                    if element(reduc[i,s]) == startGen:
                        print("acc  ", end='')
                    else:
                        print("r%-3d "%element(reduc[i,s]), end='')
            else:
                print("     ", end='')
        for s in nterm:
            if s == smap[START_SYMBOL]:
                continue
            if (i,s) in shift:
                print("%-3d  "%shift[i,s], end='')
            else:
                print("     ", end='')
        print("")

def show_c():
    global startGen, syms, gens, stats, FIRST, shift, reduc, smap, gmap, imap
    pass # TODO

def main(argv):
    filename = None
    human = False
    cstyle = False
    readId = False
    for arg in argv:
        if arg == '-h':
            human = True
        elif arg == '-c':
            cstyle = True
        elif arg == '-i':
            readId = True
        elif arg[0] != '-':
            filename = arg
    if filename is None:
        print("No input.")
        return
    with open(filename, "r") as f:
        if readId:
            addId(f.readline())
        read(f)
    process()
    if human:
        show_human()
    elif cstyle:
        show_c()
    else:
        show()

if __name__ == "__main__":
    main(sys.argv)
