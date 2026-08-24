#pragma once

#include <BindAnnotations.hpp>

#include <string>

BIND_FUNCTION()
bool exists(const std::string& path);

BIND_FUNCTION()
std::string currentPath();

BIND_FUNCTION()
void createDirectories(const std::string& path);

BIND_FUNCTION()
void removeFile(const std::string& path);

BIND_FUNCTION()
std::string compress(const std::string& value);

BIND_FUNCTION()
std::string decompress(const std::string& value);

BIND_FUNCTION()
std::string encodeBase64(const std::string& value);

BIND_FUNCTION()
std::string decodeBase64(const std::string& value);
