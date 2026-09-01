import sys
import os

def main():
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} <path-to-shader> <output>")
        return 1

    shader = sys.argv[1]
    output = sys.argv[2]

    basename = os.path.basename(shader)
    t1, t2 = basename.split('.', maxsplit=1)

    header_guard = '_'.join(map(str.upper, ['CUTS'] + os.path.dirname(output).split('/') + [t1, t2] + ['H']))

    with open(output, "w") as f:
        f.write(f"#ifndef {header_guard}\n")
        f.write(f"#define {header_guard}\n\n")

        f.write(f"static const char *{t1}_{t2} = \n")
        with open(shader, "r") as ff:
            for line in ff.readlines():
                f.write(f'  \"{line.rstrip()}\\n\"\n')
        f.write(f";\n\n")

        f.write(f"#endif")


exit(main())
