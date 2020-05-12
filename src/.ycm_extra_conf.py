import os


# --------------------------------------------------------------------
INCLUDES = [".", "./include"]
SOURCE_EXTENSIONS = [".c", ".h"]
FLAGS = [
    "-Wall",
    "-Wextra",
    "-Werror",
    "-fexceptions",
    "-ferror-limit=10000",
    "-DNDEBUG",
    "-DUSE_CLANG_COMPLETER",
    "-xc",
]


# --------------------------------------------------------------------
def relative(filename):
    return os.path.join(os.path.dirname(__file__), filename)


# --------------------------------------------------------------------
def Settings(**kwargs):
    flags = [*FLAGS]
    for include in INCLUDES:
        flags.append("-I")
        flags.append(relative(include))

    return {"flags": flags, "do_cache": True}
