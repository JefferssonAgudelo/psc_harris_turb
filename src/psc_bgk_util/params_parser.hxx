#pragma once

#include <unordered_map>
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>

// ======================================================================
// ParsedParams
// A simple parser intended for reading run parameters from a file, rather than
// hard-coding them. See psc_bgk_params.txt for an example.

class ParsedParams
{
private:
  std::unordered_map<std::string, std::string> params;

public:
  ParsedParams(const std::string file_path)
  {
    // iterate over each line
    std::ifstream ifs(file_path);

    if (ifs.is_open()) {
      for (std::string line; std::getline(ifs, line);) {

        // parse first two words within line
        std::istringstream iss(line);
        std::string paramName, paramVal;
        if (iss >> paramName >> paramVal)
          params[paramName] = paramVal;
      }

      ifs.close();
    } else {
      std::cout << "Failed to open params file: " << file_path << std::endl;
      exit(EXIT_FAILURE);
    }
  }

  template <typename T>
  T get(const std::string paramName);

  template <typename T>
  T getOrDefault(const std::string paramName, T deflt);

  // return true and display warning iff paramName is present
  bool warnIfPresent(const std::string paramName, const std::string advice);
};

// implementations

template <typename T>
T ParsedParams::getOrDefault(const std::string paramName, T deflt)
{
  if (params.count(paramName) == 1)
    return get<T>(paramName);
  std::cout << "Warning: using default value for parameter '" << paramName
            << "': " << deflt << std::endl;
  return deflt;
}

bool ParsedParams::warnIfPresent(const std::string paramName,
                                 const std::string advice)
{
  if (params.count(paramName) == 1) {

    std::cout << "Warning: parameter " << paramName << " is deprecated."
              << std::endl;
    std::cout << advice << std::endl;
    return true;
  }
  return false;
}

template <>
bool ParsedParams::get<bool>(const std::string paramName)
{
  bool b;
  std::istringstream(params[paramName]) >> std::boolalpha >> b;
  return b;
}

template <>
double ParsedParams::get<double>(const std::string paramName)
{
  return std::stod(params[paramName]);
}

template <>
int ParsedParams::get<int>(const std::string paramName)
{
  return std::stoi(params[paramName]);
}

template <>
float ParsedParams::get<float>(const std::string paramName)
{
  return std::stof(params[paramName]);
}

template <>
std::string ParsedParams::get<std::string>(const std::string paramName)
{
  return params[paramName];
}