import sys
import numpy as np

# production structure: (left, (right1, right2, ...))
# item structure: (pro#, dot_location, succeeding_symbol_set)

START_SYMBOL = 'S'
END_SYMBOL = '#'

grm_code_file = "grammar"

syms = []
pros = []
stats = [] # list((pro#, dot_location) -> set(symbol#))
empty = set() # set(symbol#)
group = dict() # stat# -> (symbol#(-1 for reduction) -> set((pro#, dot_location))
shift = dict() # (stat#, symbol#) -> stat#
reduc = dict() # (stat#, symbol#) -> set(pro#)
FIRST = dict() # symbol# -> set(symbol#)
FOLLOW = dict() # symbol# -> set(symbol#)
smap = dict() # symbol -> symbol#
pmap = dict() # symbol# -> list(pro#)
imap = dict() # sorted_tuple((pro#, dot_location, sorted_succeeding_symbol_tuple)) -> stat#

grm_h = """/*
 * This file is generated automatically by the LR(1) grammar analyser.
 */

#ifndef __%s_H__
#define __%s_H__

#include <set>

#include "parser.h"

using namespace std;

const int STATE_N = %d;
const int SYMBOL_N = %d;
const int PRO_N = %d;
const int TERMINAL_N = %d;

const int INIT_STATE = %d;
const int END_SYMBOL = %d;

extern const int GOTO[STATE_N][SYMBOL_N];
extern const char ACTION[STATE_N][TERMINAL_N];
extern const int PRO_LEFT[PRO_N];
extern const int PRO_LENGTH[PRO_N];
extern const set<int> RECOVER_SYMBOL[STATE_N];
extern const set<int> FOLLOW[SYMBOL_N];
extern const char *(PRO[PRO_N]);
extern const char *(GRAMMA_ERROR_MESSAGE[STATE_N]);

extern int (*(semanticActions[PRO_N]))(GrammaSymbol &sym);
inline bool isTerminal(int label);

#endif
 """

grm_code = """/*
 * This file is generated automatically by the LR(1) grammar analyser.
 * You should solve the conflicts manually by compiling this file and check the compile errors.
 */

#include "%s.h"

const int GOTO[STATE_N][SYMBOL_N] = {
    {%s}
};
const char ACTION[STATE_N][TERMINAL_N] = {
    {%s}
};
const int PRO_LEFT[PRO_N] = {%s};
const int PRO_LENGTH[PRO_N] = {%s}; // length of right part of the production

const set<int> RECOVER_SYMBOL[STATE_N] = { // non-terminal symbols which are related to this state in GOTO
    set<int> {%s}
};

const set<int> FOLLOW[SYMBOL_N] = {
    set<int> {%s}
};

inline bool isTerminal(int label) {
    return label < TERMINAL_N;
}

const char *(PRO[PRO_N]) = {
    %s
};

const char *(GRAMMA_ERROR_MESSAGE[STATE_N]) = {
    %s
};
"""

def element(s): # get an arbitrary element from s
    for i in s:
        return i

def p2s(p):
    return syms[pros[p][0]] + " -> " + " ".join([syms[s] for s in pros[p][1]])

def i2s(i, suc):
    right = [syms[s] for s in pros[i[0]][1]]
    right = right[:i[1]] + ["@"] + right[i[1]:]
    return syms[pros[i[0]][0]] + " -> " + " ".join(right) + "; " + "/".join([syms[s] for s in suc])

def addId(line):
    global startGen, syms, pros, stats, FIRST, shift, reduc, smap, pmap, imap
    ids = line.split()
    for i in range(len(ids)):
        smap[ids[i]] = i
        syms += [ids[i]]

def read(f):
    global startGen, syms, pros, stats, FIRST, shift, reduc, smap, pmap, imap
    if START_SYMBOL not in smap:
        smap[START_SYMBOL] = len(syms)
        syms += [START_SYMBOL]
    if END_SYMBOL not in smap:
        smap[END_SYMBOL] = len(syms)
        syms += [END_SYMBOL]
    isFirst = True
    for line in f.readlines():
        if line.strip() == "":
            continue
        l = line.split()
        if l[0] != '|':
            first = l[0]
            if first not in smap:
                smap[first] = len(syms)
                syms += [first]
            start = 2
            pmap[smap[first]] = []
        else:
            start = 1
        fs = smap[first]
        if isFirst:
            isFirst = False
            pmap[smap[START_SYMBOL]] = [len(pros)]
            startGen = len(pros)
            pros += [(smap[START_SYMBOL], tuple([fs]))]
        if len(l) <= start: # empty-able production
            empty.add(fs)
            continue
        right = []
        for w in l[start:]:
            if w not in smap:
                smap[w] = len(syms)
                syms += [w]
            right += [smap[w]]
        pmap[fs] += [len(pros)]
        pros += [tuple([fs, tuple(right)])]

def solveFIRST(sym, visited):
    global startGen, syms, pros, stats, FIRST, shift, reduc, smap, pmap, imap
    if sym in visited:
        return set()
    if sym in FIRST:
        return FIRST[sym]
    if sym not in pmap: # terminal
        FIRST[sym] = set([sym])
        return FIRST[sym]
    visited.add(sym)
    result = set()
    for p in pmap[sym]:
        for s in pros[p][1]:
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
    for pro in pros:
        for symi in range(len(pro[1]) - 1):
            a, b = pro[1][symi: symi + 2]
            FOLLOW[a] |= FIRST[b]
            if b in empty:
                adj[a, b] = 1 # FOLLOW[a] depends on FOLLOW[b]
        adj[pro[1][-1], pro[0]] = 1 # FOLLOW[last_symbol] depends on FOLLOW[left_symbol]
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
    global startGen, syms, pros, stats, FIRST, shift, reduc, smap, pmap, imap
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
        pro = pros[tmp[i][0]]
        if tmp[i][1] == len(pro[1]): # reduction
            if -1 not in group[si]:
                group[si][-1] = set()
            group[si][-1] |= set([tmp[i][:2]])
            i += 1
            continue
        sym = pro[1][tmp[i][1]] # the symbol to be expanded
        if sym not in group[si]:
            group[si][sym] = set()
        group[si][sym] |= set([tmp[i][:2]])
        if sym not in pmap: # terminal symbol
            i += 1
            continue
        if tmp[i][1] + 1 < len(pro[1]):
            suc = set()
            for j in range(tmp[i][1] + 1, len(pro[1])):
                suc |= FIRST[pro[1][j]]
                if pro[1][j] not in empty:
                    break
            else:
                suc |= tmp[i][2]
        else:
            suc = set(tmp[i][2])
        for p in pmap[sym]:
            newItem = (p, 0, suc)
            if newItem[:2] not in stats[si]:
                stats[si][newItem[:2]] = set(newItem[2])
                tmp += [newItem]
            elif not newItem[2] <= stats[si][newItem[:2]]:
                stats[si][newItem[:2]] |= newItem[2]
                tmp += [newItem]
        if sym in empty: # deal with the empty production
            newItem = (tmp[i][0], tmp[i][1] + 1, set(tmp[i][2]))
            if newItem[:2] not in stats[si]:
                stats[si][newItem[:2]] = set(newItem[2])
                tmp += [newItem]
            elif not newItem[2] <= stats[si][newItem[:2]]:
                stats[si][newItem[:2]] |= newItem[2]
                tmp += [newItem]
        i += 1

def dfs(si):
    global startGen, syms, pros, stats, FIRST, shift, reduc, smap, pmap, imap
    for sym in group[si]:
        if sym == -1: # reduction
            for p, dot in group[si][sym]:
                for t in stats[si][p, dot]:
                    key = (si, t)
                    if key not in reduc:
                        reduc[key] = set()
                    reduc[key] |= set([p])
        else: # non-reduction
            key = (si, sym)
            tmp = tuple(sorted([(p, d, tuple(sorted(list(stats[si][p, d])))) for p,d in group[si][sym]]))
            if tmp in imap:
                shift[key] = imap[tmp]
            else:
                shift[key] = len(stats)
                stats += [dict()]
                for p, dot in group[si][sym]:
                    newItem = (p, dot + 1, set(stats[si][p, dot]))
                    add(shift[key], newItem)
                imap[tmp] = shift[key]
                dfs(shift[key])

def process():
    global startStat, startGen, syms, pros, stats, FIRST, shift, reduc, smap, pmap, imap
    assert len(pmap[smap[START_SYMBOL]]) == 1
    for s in range(len(syms)):
        solveFIRST(s, set())
    solveFOLLOW()
    startStat = len(stats)
    stats += [dict()]
    add(startStat, tuple([pmap[smap[START_SYMBOL]][0], 0, tuple([smap[END_SYMBOL]])])); ### S -> .A; #
    dfs(startStat)

def show_human():
    global startGen, syms, pros, stats, FIRST, shift, reduc, smap, pmap, imap
    conflicts = dict()
    print("Symbol total: " + str(len(syms)))
    print("State total: " + str(len(stats)))
    conflict = 0
    for key in shift.keys() | reduc.keys():
        l1 = 1 if key in shift else 0
        l2 = len(reduc[key]) if key in reduc else 0
        conflict += 1 if l1 + l2 > 1 else 0
    print("Conflict total: %d"%conflict)
    print("\nSymbols:\n")
    for i in range(len(syms)):
        print("'%s': %d"%(syms[i], i))
    print("\nFIRST:\n")
    for s in range(len(syms)):
        print(syms[s] + ": " + str([syms[i] for i in FIRST[s]]))
    print("\nState:\n")
    for i in range(len(stats)):
        print("I%d:"%(i))
        for item in stats[i]:
            print(i2s(item, stats[i][item]))
        print("")
    print("\nTable:\n")
    term = [i for i in range(len(syms)) if i not in pmap]
    nterm = [i for i in range(len(syms)) if i in pmap]
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
                conflicts[i,s] = ["s%d"%shift[i,s]] + ["r%d"%t for t in reduc[i,s]]
            elif (i,s) in shift and (i,s) not in reduc:
                print("s%-3d "%shift[i,s], end='')
            elif (i,s) not in shift and (i,s) in reduc:
                if len(reduc[i,s]) > 1:
                    conflicts[i,s] = ["s%d"%t for t in shift[i,s]] + ["r%d"%t for t in reduc[i,s]]
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
    print("\nConflicts:\n")
    for i,s in conflicts:
        print("I%d ('%s') -> %s"%(i, syms[s], ", ".join(conflicts[i,s])))

def show_c():
    global startGen, syms, pros, stats, FIRST, shift, reduc, smap, pmap, imap
    termN = len([i for i in range(len(syms)) if i not in pmap])
    ntermN = len([i for i in range(len(syms)) if i in pmap])
    GOTO = [["-1" for _ in range(len(syms))] for __ in range(len(stats))]
    ACTION = [["'\\0'" for _ in range(termN)] for __ in range(len(stats))]
    for i in range(len(stats)):
        for j in range(len(syms)):
            s = j
            if j not in pmap: # terminal
                if (i,s) in shift and (i,s) in reduc:
                    hint = "/* " + ",".join(["s%d"%shift[i,s]] + ["r%d"%t for t in reduc[i,s]]) + " */"
                    ACTION[i][s] = hint
                    GOTO[i][s] = hint
                elif (i,s) in shift and (i,s) not in reduc:
                    ACTION[i][s] = "'s'"
                    GOTO[i][s] = str(shift[i,s])
                elif (i,s) not in shift and (i,s) in reduc:
                    if len(reduc[i,s]) > 1:
                        hint = "/* " + ",".join(["s%d"%shift[i,s]] + ["r%d"%t for t in reduc[i,s]]) + " */"
                        ACTION[i][s] = hint
                        GOTO[i][s] = hint
                    else:
                        if element(reduc[i,s]) == startGen:
                            ACTION[i][s] = "'a'"
                        else:
                            ACTION[i][s] = "'r'"
                            GOTO[i][s] = str(element(reduc[i,s]))
            else: # non-terminal
                if s == smap[START_SYMBOL]:
                    continue
                if (i,s) in shift:
                    GOTO[i][s] = str(shift[i,s])
    GOTO = "},\n    {".join([", ".join(l) for l in GOTO])
    ACTION = "},\n    {".join([", ".join(l) for l in ACTION])
    PRO_LEFT = ", ".join([str(pro[0]) for pro in pros])
    PRO_LENGTH = ", ".join([str(len(pro[1])) for pro in pros])
    RECOVER_SYMBOL = "},\n    set<int> {".join([", ".join([str(sym) for sym in range(len(syms)) if sym in pmap and (s,sym) in shift]) for s in range(len(stats))])
    FOLLOW_CODE = "},\n    set<int> {".join([", ".join([str(sym) for sym in FOLLOW[s]]) for s in range(len(syms))])
    PRO = ",\n    ".join(['"' + p2s(p) + '"' for p in range(len(pros))])
    ERROR_MESSAGE = ",\n    ".join(["\"Line %d, Col %d: Unexpected token: %s\\n\""]*len(stats))
    grm_h_info = (grm_code_file.upper(),
                  grm_code_file.upper(),
                  len(stats),
                  len(syms),
                  len(pros),
                  termN,
                  startStat,
                  smap[END_SYMBOL])
    grm_info = (grm_code_file,
                GOTO,
                ACTION,
                PRO_LEFT,
                PRO_LENGTH,
                RECOVER_SYMBOL,
                FOLLOW_CODE,
                PRO,
                ERROR_MESSAGE)
    with open(grm_code_file + ".h", "w") as f:
        f.write(grm_h%grm_h_info)
    with open(grm_code_file + ".cpp", "w") as f:
        f.write(grm_code%grm_info)

def main(argv):
    filename = None
    human = False
    cstyle = False
    readId = False
    readingGrammarFileName = False
    for arg in argv:
        if arg == '-h':
            human = True
        elif arg == '-c':
            cstyle = True
        elif arg == '-i':
            readId = True
        elif arg == "-g":
            readingGrammarFileName = True
        elif arg[0] != '-':
            if readingGrammarFileName:
                grm_code_file = arg
                readingGrammarFileName = False
            else:
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
