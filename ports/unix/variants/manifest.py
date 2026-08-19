import os
from typing import TYPE_CHECKING

# The manifest API below (add_library/require/freeze/...) is not a real import:
# the µPy freeze tool exec()s this file with those names injected as globals
# (see tools/manifestfile.py _manifest_globals). Declaring them under
# TYPE_CHECKING keeps the type checker quiet without affecting the build, where
# the injected callables always shadow these stubs.
if TYPE_CHECKING:

    def add_library(library: str, library_path: str, prepend: bool = False) -> None: ...
    def require(
        name: str,
        version: str | None = None,
        pypi: str | None = None,
        library: str | None = None,
        **kwargs: object,
    ) -> None: ...
    def freeze(path: str, files: tuple[str, ...], opt: int | None = None) -> None: ...

add_library("unix-ffi", "$(MPY_LIB_DIR)/unix-ffi")
require("mip-cmdline")
require("ssl")

# Metal seat only: the page renderer and its precompiled templates. Frozen from
# their canonical home in the metal tree, and named from the file rather than the
# dotted path on purpose — `pymergetic.metal.*` is a card namespace, and a card
# module carries no __path__ for a Python submodule to be found under.
if os.environ.get("MICROPY_PY_METAL") == "1":
    _seat = "$(MPY_DIR)/extmod/metal/src/pymergetic/metal/inspect"
    freeze(_seat, ("catalog_render.py", "metal_packs.py", "artifacts.py", "openapi.py"))
    freeze(_seat + "/www/_compiled", ("package_html.py", "shell_html.py"))
