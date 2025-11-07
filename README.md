# arithmetic-compiler

Took project inspirations here, go learn and build: [https://github.com/dexter-xD/project-box](https://github.com/dexter-xD/project-box)

## Usage

```bash
gcc -o main main.c && ./main "14 ^ 4 + 100 * 37 - 50 / 25 % 2 + (-47)" -a

AST: (+ (- (+ (expt 14 4) (* 100 37)) (mod (/ 50 25) 2)) -47)
VM Result: 42069
Eval Result: 42069
```

TODOs

- [x] bytecode generation and stack vm
- [ ] ???
