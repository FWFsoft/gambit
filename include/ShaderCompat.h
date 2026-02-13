#pragma once

#ifdef __EMSCRIPTEN__
#define GAMBIT_GLSL_VERSION "#version 300 es\n"
#define GAMBIT_GLSL_PRECISION "precision mediump float;\n"
#define GAMBIT_GLSL_VERSION_IMGUI "#version 300 es"
#else
#define GAMBIT_GLSL_VERSION "#version 330 core\n"
#define GAMBIT_GLSL_PRECISION ""
#define GAMBIT_GLSL_VERSION_IMGUI "#version 330"
#endif
