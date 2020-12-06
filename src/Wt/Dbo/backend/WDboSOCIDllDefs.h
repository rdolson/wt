// This may look like C code, but it's really -*- C++ -*-
/*
* Copyright (C) 2017 Emweb bv, Herent, Belgium.
*
* See the LICENSE file for terms of use.
*/
#ifndef WDBOSOCISERVERDLLDEFS_H_
#define WDBOSOCISERVERDLLDEFS_H_

#include <Wt/WConfig.h>

// Source: http://www.nedprod.com/programs/gccvisibility.html

#ifdef WT_WIN32
  #define WTDBOSOCISERVER_IMPORT __declspec(dllimport)
  #define WTDBOSOCISERVER_EXPORT __declspec(dllexport)
  #define WTDBOSOCISERVER_DLLLOCAL
  #define WTDBOSOCISERVER_DLLPUBLIC
#else
  #define WTDBOSOCISERVER_IMPORT __attribute__ ((visibility("default")))
  #define WTDBOSOCISERVER_EXPORT __attribute__ ((visibility("default")))
  #define WTDBOSOCISERVER_DLLLOCAL __attribute__ ((visibility("hidden")))
  #define WTDBOSOCISERVER_DLLPUBLIC __attribute__ ((visibility("default")))
#endif

#ifdef wtdbomssqlserver_EXPORTS
  #define WTDBOSOCISERVER_API WTDBOSOCISERVER_EXPORT
#else
  #ifdef WTDBOSOCISERVER_STATIC
    #define WTDBOSOCISERVER_API
  #else
    #define WTDBOSOCISERVER_API WTDBOSOCISERVER_IMPORT
  #endif
#endif

#endif // WDBOSOCISERVERDLLDEFS_H_
