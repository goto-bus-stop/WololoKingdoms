#pragma once
#include <fs.h>
#include <string>
#include <vector>

void convert_maps(const fs::path& input_dir, const fs::path& output_dir,
                  const std::map<std::string, fs::path>& terrain_graphics,
                  bool replace = false);
