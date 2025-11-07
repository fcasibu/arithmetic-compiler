# arithmetic-compiler

Took project inspirations here, go learn and build: [https://github.com/dexter-xD/project-box](https://github.com/dexter-xD/project-box)

## Usage

```bash
gcc -o main main.c && ./main "23 + 57 * 8 - 42 / 7 % 19 ^ 4" -a

AST: (- (+ 23 (* 57 8)) (mod (/ 42 7) (expt 19 4)))
Result: 473
```

TODOs

- [ ] bytecode generation and stack vm
- [ ] ???
