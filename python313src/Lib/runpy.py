"""runpy.py - locating and running Python code using the module namespace

Provides support for locating and running Python scripts using the Python
module namespace instead of the native filesystem.

This allows Python code to play nicely with non-filesystem based PEP 302
importers when locating support scripts as well as when importing modules.
"""
# Written by Nick Coghlan <ncoghlan at gmail.com>
#    to implement PEP 338 (Executing Modules as Scripts)

import sys
import imp
try:
    from imp import get_loader
except ImportError:
    from pkgutil import get_loader

__all__ = [
    "run_module",
]


def _run_code(code, run_globals, init_globals=None,
              mod_name=None, mod_fname=None,
              mod_loader=None, pkg_name=None):
    """Helper for _run_module_code"""
    if init_globals is not None:
        run_globals.update(init_globals)
    run_globals.update(__name__ = mod_name,
                       __file__ = mod_fname,
                       __loader__ = mod_loader,
                       __package__ = pkg_name)
    exec(code, run_globals)
    return run_globals

def _run_module_code(code, init_globals=None,
                    mod_name=None, mod_fname=None,
                    mod_loader=None, pkg_name=None):
    """Helper for run_module"""
    # Set up the top level namespace dictionary
    temp_module = imp.new_module(mod_name)
    mod_globals = temp_module.__dict__
    # Modify sys.argv[0] and sys.module[mod_name]
    saved_argv0 = sys.argv[0]
    restore_module = mod_name in sys.modules
    if restore_module:
        saved_module = sys.modules[mod_name]
    sys.argv[0] = mod_fname
    sys.modules[mod_name] = temp_module
    try:
        _run_code(code, mod_globals, init_globals,
                    mod_name, mod_fname,
                    mod_loader, pkg_name)
    finally:
        sys.argv[0] = saved_argv0
        if restore_module:
            sys.modules[mod_name] = saved_module
        else:
            del sys.modules[mod_name]
    # Copy the globals of the temporary module, as they
    # may be cleared when the temporary module goes away
    return mod_globals.copy()


# This helper is needed due to a missing component in the PEP 302
# loader protocol (specifically, "get_filename" is non-standard)
# Since we can't introduce new features in maintenance releases,
# support was added to zipimporter under the name '_get_filename'
def _get_filename(loader, mod_name):
    for attr in ("get_filename", "_get_filename"):
        meth = getattr(loader, attr, None)
        if meth is not None:
            return meth(mod_name)
    return None

# Helper to get the loader, code and filename for a module
def _get_module_details(mod_name):
    loader = get_loader(mod_name)
    if loader is None:
        raise ImportError("No module named %s" % mod_name)
    if loader.is_package(mod_name):
        if mod_name == "__main__" or mod_name.endswith(".__main__"):
            raise ImportError(("Cannot use package as __main__ module"))
        try:
            pkg_main_name = mod_name + ".__main__"
            return _get_module_details(pkg_main_name)
        except ImportError as e:
            raise ImportError(("%s; %r is a package and cannot " +
                               "be directly executed") %(e, mod_name))
    code = loader.get_code(mod_name)
    if code is None:
        raise ImportError("No code object available for %s" % mod_name)
    filename = _get_filename(loader, mod_name)
    return mod_name, loader, code, filename


# XXX ncoghlan: Should this be documented and made public?
# (Current thoughts: don't repeat the mistake that lead to its
# creation when run_module() no longer met the needs of
# mainmodule.c, but couldn't be changed because it was public)
def _run_module_as_main(mod_name, set_argv0=True):
    """Runs the designated module in the __main__ namespace

       These __*__ magic variables will be overwritten:
           __file__
           __loader__
    """
    try:
        mod_name, loader, code, fname = _get_module_details(mod_name)
    except ImportError as exc:
        # Try to provide a good error message
        # for directories, zip files and the -m switch
        if set_argv0:
            # For -m switch, just display the exception
            info = str(exc)
        else:
            # For directories/zipfiles, let the user
            # know what the code was looking for
            info = "can't find '__main__.py' in %r" % sys.argv[0]
        msg = "%s: %s" % (sys.executable, info)
        sys.exit(msg)
    pkg_name = mod_name.rpartition('.')[0]
    main_globals = sys.modules["__main__"].__dict__
    if set_argv0:
        sys.argv[0] = fname
    return _run_code(code, main_globals, None,
                     "__main__", fname, loader, pkg_name)

def run_module(mod_name, init_globals=None,
               run_name=None, alter_sys=False):
    """Execute a module's code without importing it

       Returns the resulting top level namespace dictionary
    """
    mod_name, loader, code, fname = _get_module_details(mod_name)
    if run_name is None:
        run_name = mod_name
    pkg_name = mod_name.rpartition('.')[0]
    if alter_sys:
        return _run_module_code(code, init_globals, run_name,
                                fname, loader, pkg_name)
    else:
        # Leave the sys module alone
        return _run_code(code, {}, init_globals, run_name,
                         fname, loader, pkg_name)


if __name__ == "__main__":
    # Run the module specified as the next command line argument
    if len(sys.argv) < 2:
        print("No module specified for execution", file=sys.stderr)
    else:
        del sys.argv[0] # Make the requested module sys.argv[0]
        _run_module_as_main(sys.argv[0])
