# Toy Compiler

This is a toy compiler frontend written in C++ for a C-like language, with a generic LR(1) grammar analysis table generator written in Python as the by-product.

All the codes are tested with g++ 7.4.0 and Python 3.6.7.

## Usage

- To compile it, run
  ```bash
  make
  ```

  and the executive will be outputted as `build/main`.

- To run a lexical analysis on a source file, run
  ```bash
  ./build/main -l <filename>
  ```

- To run a gramma analysis on a source file, run
  ```bash
  ./build/main -g <filename>
  ```

- To run a semantic analysis on a source file, run
  ```bash
  ./build/main -s <filename>
  ```
  or
  ```bash
  ./build/main <filename>
  ```

## Language Definition

The grammar of this language is defined in the file `lab.grm`. The semantic of this language is basically the same as C.

## Tests and Examples

The tests (including the tests for the LR(1) grammar analysis table generator) and code examples can be found in the `tests/` directory. There are both positive tests (with well-defined grammar and semantic) and negative tests (with grammar errors or semantic errors). To test the program, build it firstly and run
```bash
make test
```

## LR(1) Grammar Analysis Table Generator

`LR1.py` is a generic analysis table generator for LR(1) grammars, while it can also handle grammars with conflicts in LR(1).

### Usage

1. Write a grammar definition file. The format of the grammar definition file is simple:
   - In the first line, list the symbols (no matter terminal or not) to which you want to assign specific integer IDs separated by space(s). The symbols listed in the first line will be numbered from 0 to n-1 ('n' is the number of listed symbols). The symbols which are not listed in the first line will be numbered by the presented order in the grammar file. **This line is optional.**
   - In the following lines, write down the productions of your grammar. The format of each production should be:
     ```
     LEFT_SYMBOL -> RIGHT_SYMBOL1 RIGHT_SYMBOL2 ...
                  | RIGHT_SYMBOL3 RIGHT_SYMBOL4 ...
                  | ...
     ```
     Note that there should be at least 1 space between any gramma symbols. There should be at least 1 space between a gramma symbol and the rightarrow symbol (->) or or symbol (|), too. It is okay for a line to be empty or to be with some extra spaces at the beginning.
     For an empty production, just remain empty for the right side of the rightarrow symbol.
     **Note that you should write all the productions for the same left symbol at once, separated by '|'.**
   - Example:
     ```
     b c A B C
     A -> B b
         B -> B b c
            | C b
            | C
            |
         C -> c
     ```
   - Notice
     Symbols `S`, `#`, and `|` are reserved. To use them in your grammar, modify the definitions of `START_SYMBOL`, `END_SYMBOL` and `OR_SYMBOL` in `LR1.py`.
2. Run the generator.
   - Basic usage:
     ```bash
     python3 ./LR1.py <filename>
     ```
   - To output a human-readable result to standard output, add argument `-h`. However, you can do nothing and the program will be set to this mode by default.
   - To output a C++ source file and a header file, add argument `-c`. If `-h` and `-c` are both set, `-h` will be used. However, it's not recommended to do so.
   - To set the output file name for `-c`, add argument `-g <filename>`. Note that you don't need to specify the extension here. Example: `-g grammar`
   - If you present the optional first line in the grammar file, add argument `-i`.
3. Clear the conflicts.
   You can check the conflicts in your grammar according to the output of the generator.
   - In human-readable mode, you can check the last few lines to get the locations of the conflicts.
   - In C++ code mode, you can check the compile error of the generated cpp file. You can also search for `/*` in the generated cpp file, and the comment will give you all the choices of the conflict.

   You have 2 ways to clear the conflicts if they exist.
   - Modify your grammar file and re-generate.
   - Modify the generated cpp file by choosing one of the choices for every conflict and remove the comment.

   It is recommended to check the conflicts in human-readable mode and clear them in the generated cpp file.
