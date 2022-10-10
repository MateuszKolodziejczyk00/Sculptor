#pragma once

#pragma warning(disable : 4251) // [Class] needs to have dll-interface to be used by clients of class
							    // Disabled as it generates lots of warnings when using header-only libraries

#pragma warning(disable : 4100) // [argument] unreferenced formal parameter
							    // Disabled as it generates lots of warnings when delegates etc.

#pragma warning(disable : 4324) // [struct] structure was padded due to alignment specifier
							    // Disabled as we may want to have padding to match shader struct layout
