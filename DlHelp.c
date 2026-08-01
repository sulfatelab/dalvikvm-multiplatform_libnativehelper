/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "DlHelp.h"

#include <stdbool.h>

#if defined(_WIN32)
#include <stdio.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mdvm_windows_utf8.h>
#else
#include <dlfcn.h>
#endif

DlLibrary DlOpenLibrary(const char* filename) {
#ifdef _WIN32
  wchar_t* wide_filename = mdvm_utf8_to_utf16_alloc(filename);
  if (wide_filename == NULL) {
    return NULL;
  }
  HMODULE library = LoadLibraryW(wide_filename);
  DWORD error = library != NULL ? ERROR_SUCCESS : GetLastError();
  free(wide_filename);
  SetLastError(error);
  return (DlLibrary)library;
#else
  // Load with RTLD_NODELETE in order to ensure that libart.so is not unmapped when it is closed.
  // This is due to the fact that it is possible that some threads might have yet to finish
  // exiting even after JNI_DeleteJavaVM returns, which can lead to segfaults if the library is
  // unloaded.
  return dlopen(filename, RTLD_NOW | RTLD_NODELETE);
#endif
}

bool DlCloseLibrary(DlLibrary library) {
#ifdef _WIN32
  return (FreeLibrary((HMODULE)library) == TRUE);
#else
  return (dlclose(library) == 0);
#endif
}

DlSymbol DlGetSymbol(DlLibrary handle, const char* symbol) {
#ifdef _WIN32
  return (DlSymbol) GetProcAddress((HMODULE)handle, symbol);
#else
  return dlsym(handle, symbol);
#endif
}

const char* DlGetError() {
#ifdef _WIN32
  static char buffer[256];
  wchar_t wide_buffer[256];

  DWORD cause = GetLastError();
  DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
  DWORD length = FormatMessageW(
      flags, NULL, cause, 0, wide_buffer,
      (DWORD)(sizeof(wide_buffer) / sizeof(wide_buffer[0])), NULL);
  if (length == 0) {
    snprintf(buffer, sizeof(buffer),
             "Error %lu while retrieving message for error %lu",
             GetLastError(), cause);
    return buffer;
  }

  // Trim trailing whitespace.
  while (length > 0 && (wide_buffer[length - 1] == L'\r' ||
                        wide_buffer[length - 1] == L'\n' ||
                        wide_buffer[length - 1] == L' ' ||
                        wide_buffer[length - 1] == L'\t')) {
    wide_buffer[--length] = L'\0';
  }
  if (!mdvm_utf16_to_utf8_buffer(wide_buffer, buffer, sizeof(buffer))) {
    snprintf(buffer, sizeof(buffer),
             "Error %lu while converting message for error %lu",
             GetLastError(), cause);
  }

  return buffer;
#else
  return dlerror();
#endif
}
