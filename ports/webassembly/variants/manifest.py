# The asyncio package is built from the standard implementation but with the
# core scheduler replaced with a custom scheduler that uses the JavaScript
# runtime (with setTimeout an Promise's) to contrtol the scheduling.

import os
from typing import TYPE_CHECKING

# The manifest API below (package/freeze/require/add_library/...) is not a real
# import: the µPy freeze tool exec()s this file with those names injected as
# globals (see tools/manifestfile.py _manifest_globals). Declaring them under
# TYPE_CHECKING keeps the type checker quiet without affecting the build, where
# the injected callables always shadow these stubs.
if TYPE_CHECKING:

    def package(
        package_path: str,
        files: tuple[str, ...] | None = None,
        base_path: str = ".",
        opt: int | None = None,
    ) -> None: ...
    def module(module_path: str, base_path: str = ".", opt: int | None = None) -> None: ...
    def freeze(path: str, script: object = None, opt: int | None = None) -> None: ...
    def require(
        name: str,
        version: str | None = None,
        pypi: str | None = None,
        library: str | None = None,
        **kwargs: object,
    ) -> None: ...
    def add_library(library: str, library_path: str, prepend: bool = False) -> None: ...

package(
    "asyncio",
    (
        "event.py",
        "funcs.py",
        "lock.py",
    ),
    base_path="$(MPY_DIR)/extmod",
    opt=3,
)

package(
    "asyncio",
    (
        "__init__.py",
        "core.py",
    ),
    base_path="$(PORT_DIR)",
    opt=3,
)

# Metal seat only: the page renderer and its precompiled templates. Same set the
# unix seat freezes — the httpd's /packs/<fqn> pages are rendered by Python on
# every µPy seat, so the renderer cannot be a unix extra. Named from the file
# rather than the dotted path: `pymergetic.metal.*` is a card namespace, and a
# card module carries no __path__ for a Python submodule to be found under.
if os.environ.get("MICROPY_PY_METAL") == "1":
    _seat = "$(MPY_DIR)/extmod/metal/src/pymergetic/metal/inspect"
    freeze(_seat, ("catalog_render.py", "metal_packs.py", "artifacts.py", "openapi.py"))
    freeze(_seat + "/www/_compiled", ("package_html.py", "shell_html.py"))
