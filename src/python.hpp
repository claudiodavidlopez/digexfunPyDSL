#pragma once

// Include Python.h without requiring a debug build of Python. With _DEBUG
// defined (MSVC Debug configurations), pyconfig.h links against
// python3XX_d.lib, which regular Python installations don't ship.
#if defined(_DEBUG)
#    undef _DEBUG
#    include <Python.h>
#    define _DEBUG
#else
#    include <Python.h>
#endif
