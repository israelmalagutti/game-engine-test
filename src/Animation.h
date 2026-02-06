#pragma once

#include <string>

struct Animation {
  std::string name;
  int row;

  int frameCount;
  float frameDuration;

  bool loop;
};
