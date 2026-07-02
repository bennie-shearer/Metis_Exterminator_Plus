/*
 * Windows include-order fix for SQLite amalgamation.
 * SQLite includes <windows.h> internally which pulls in wincrypt.h -> ncrypt.h.
 * ncrypt.h references BCryptBuffer/BCryptBufferDesc from bcrypt.h (Windows CNG),
 * but only if bcrypt.h has not been included first.
 * This header, included via -include or forced first-include, ensures bcrypt.h
 * (the Windows CNG header, NOT our bcrypt wrapper) is included before windows.h.
 *
 * Used only for the sqlite3.c compilation unit on MinGW-w64 Windows.
 */
#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#  define NOMINMAX
#  endif
#  include <bcrypt.h>
#endif
