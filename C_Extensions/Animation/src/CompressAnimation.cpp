#include <CompressAnimation.h>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/System/Angle.hpp>
#include <SFML/System/Vector2.hpp>
#include <cmath>
#include <filesystem>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>

long double PI = std::acos(-1.0L);

std::optional<sf::Texture *>
getTexture(std::unordered_map<int, sf::Texture *> *textureCache,
           const std::vector<std::string> &assets,
           const std::string &assetsRoot, int assetIndex) {
  auto it = textureCache->find(assetIndex);
  if (it != textureCache->end()) {
    return it->second;
  }
  if (assetIndex >= 0 && assetIndex < assets.size()) {
    std::string assetName = assets[assetIndex];
    std::filesystem::path assetPath =
        std::filesystem::path(assetsRoot) / assetName;
    (*textureCache)[assetIndex] = new sf::Texture(assetPath);
    return (*textureCache)[assetIndex];
  }
  return std::nullopt;
}

std::optional<std::tuple<float, float, float, float, float>>
getSegmentTransform(py::dict segment, float frameTime) {
  py::dict startFrame = segment.contains("startFrame")
                            ? segment["startFrame"].cast<py::dict>()
                            : py::dict();
  py::dict endFrame = segment.contains("endFrame")
                          ? segment["endFrame"].cast<py::dict>()
                          : py::dict();
  float startTime =
      startFrame.contains("time") ? startFrame["time"].cast<float>() : 0.0f;
  float endTime =
      endFrame.contains("time") ? endFrame["time"].cast<float>() : 0.0f;
  if (frameTime < startTime || frameTime > endTime) {
    return std::nullopt;
  }
  float factor;
  if (endTime - startTime <= 1e-4) {
    factor = 0.0f;
  } else {
    factor = (frameTime - startTime) / (endTime - startTime);
  }
  auto startPos = startFrame.contains("position")
                      ? startFrame["position"].cast<std::pair<float, float>>()
                      : std::pair<float, float>(0.0f, 0.0f);
  auto endPos = endFrame.contains("position")
                    ? endFrame["position"].cast<std::pair<float, float>>()
                    : std::pair<float, float>(0.0f, 0.0f);
  float curX = startPos.first + (endPos.first - startPos.first) * factor;
  float curY = startPos.second + (endPos.second - startPos.second) * factor;

  float startRot = startFrame.contains("rotation")
                       ? startFrame["rotation"].cast<float>()
                       : 0.0f;
  float endRot =
      endFrame.contains("rotation") ? endFrame["rotation"].cast<float>() : 0.0f;
  float curRot = startRot + (endRot - startRot) * factor;

  auto startScale = startFrame.contains("scale")
                        ? startFrame["scale"].cast<std::pair<float, float>>()
                        : std::pair<float, float>(1.0f, 1.0f);
  auto endScale = endFrame.contains("scale")
                      ? endFrame["scale"].cast<std::pair<float, float>>()
                      : std::pair<float, float>(1.0f, 1.0f);
  float curScaleX =
      startScale.first + (endScale.first - startScale.first) * factor;
  float curScaleY =
      startScale.second + (endScale.second - startScale.second) * factor;

  return std::make_tuple(curX, curY, curRot, curScaleX, curScaleY);
}

std::pair<float, float> getRotatedSize(float width, float height,
                                       float rotation) {
  long double radians = static_cast<long double>(rotation) * (PI / 180.0L);
  long double cosValue = std::abs(std::cos(radians));
  long double sinValue = std::abs(std::sin(radians));
  return std::make_pair(width * cosValue + height * sinValue,
                        width * sinValue + height * cosValue);
}

std::tuple<float, std::vector<py::bytes>, std::vector<py::dict>>
C_CompressAnimation(
    py::object zlibModule, int frameCount, float frameStep, int frameRate,
    const std::vector<std::unordered_map<std::string, std::vector<py::dict>>>
        &timeLines,
    const std::vector<std::string> &assets, const std::string &assetsRoot,
    const std::string &imageFormat) {
  std::unordered_map<int, sf::Texture *> textureCache;
  float minX = std::numeric_limits<float>::infinity();
  float minY = std::numeric_limits<float>::infinity();
  float maxX = -std::numeric_limits<float>::infinity();
  float maxY = -std::numeric_limits<float>::infinity();

  for (int frameIndex = 0; frameIndex < frameCount; ++frameIndex) {
    float frameTime = frameIndex * frameStep;
    for (auto &timeLine : timeLines) {
      auto &segments = timeLine.at("timeSegments");
      for (auto &segment : segments) {
        if (!segment.contains("type") ||
            segment["type"].cast<std::string>() != "frame") {
          continue;
        }
        int assetIndex = segment["asset"].cast<int>();
        auto textureObj =
            getTexture(&textureCache, assets, assetsRoot, assetIndex);
        if (!textureObj.has_value()) {
          continue;
        }
        auto texture = textureObj.value();
        auto transformDataObj = getSegmentTransform(segment, frameTime);
        if (!transformDataObj.has_value()) {
          continue;
        }
        auto transformData = transformDataObj.value();
        auto [curX, curY, curRot, curScaleX, curScaleY] = transformData;
        sf::Vector2u size = texture->getSize();
        float scaledWidth = size.x * std::abs(curScaleX);
        float scaledHeight = size.y * std::abs(curScaleY);
        auto [rotatedWidth, rotatedHeight] =
            getRotatedSize(scaledWidth, scaledHeight, curRot);
        float left = curX - rotatedWidth / 2.0f;
        float right = curX + rotatedWidth / 2.0f;
        float top = curY - rotatedHeight / 2.0f;
        float bottom = curY + rotatedHeight / 2.0f;
        minX = std::min(minX, left);
        minY = std::min(minY, top);
        maxX = std::max(maxX, right);
        maxY = std::max(maxY, bottom);
      }
    }
  }
  if (minX == std::numeric_limits<float>::infinity()) {
    minX = 0.0f;
    minY = 0.0f;
    maxX = 1.0f;
    maxY = 1.0f;
  }
  unsigned int canvasWidth =
      std::max(1, static_cast<int>(std::ceil(maxX - minX)));
  unsigned int canvasHeight =
      std::max(1, static_cast<int>(std::ceil(maxY - minY)));
  sf::RenderTexture renderTexture(sf::Vector2u(canvasWidth, canvasHeight));
  if (!renderTexture.setActive(true)) {
    throw std::runtime_error("Failed to set active render texture");
  }
  std::vector<py::bytes> frames;
  auto targetTexture = renderTexture.getTexture();
  sf::Sprite sprite(targetTexture);
  for (int frameIndex = 0; frameIndex < frameCount; ++frameIndex) {
    float frameTime = frameIndex * frameStep;
    renderTexture.clear(sf::Color::Transparent);
    for (auto &timeLine : timeLines) {
      auto &segments = timeLine.at("timeSegments");
      for (auto &segment : segments) {
        if (!segment.contains("type") ||
            segment["type"].cast<std::string>() != "frame") {
          continue;
        }
        int assetIndex = segment["asset"].cast<int>();
        auto textureObj =
            getTexture(&textureCache, assets, assetsRoot, assetIndex);
        if (!textureObj.has_value()) {
          continue;
        }
        auto texture = textureObj.value();
        auto transformDataObj = getSegmentTransform(segment, frameTime);
        if (!transformDataObj.has_value()) {
          continue;
        }
        auto transformData = transformDataObj.value();
        auto [curX, curY, curRot, curScaleX, curScaleY] = transformData;
        sprite.setTexture(*texture, true);
        sf::Vector2u size = texture->getSize();
        sprite.setOrigin({size.x / 2.0f, size.y / 2.0f});
        sprite.setPosition({curX - minX, curY - minY});
        sprite.setRotation(sf::degrees(curRot));
        sprite.setScale({curScaleX, curScaleY});
        renderTexture.draw(sprite);
      }
    }
    renderTexture.display();
    sf::Image image = renderTexture.getTexture().copyToImage();
    auto memoryData = image.saveToMemory(imageFormat);
    if (!memoryData.has_value()) {
      throw std::runtime_error("Failed to save image to memory");
    }
    const auto &buf = memoryData.value();
    const char *ptr = reinterpret_cast<const char *>(buf.data());
    std::size_t bytes_size = buf.size();
    py::bytes py_data(ptr, bytes_size);
    frames.push_back(zlibModule.attr("compress")(py_data));
  }
  std::vector<py::dict> sounds;
  for (auto &timeLine : timeLines) {
    auto &segments = timeLine.at("timeSegments");
    for (auto &segment : segments) {
      if (!segment.contains("type") ||
          segment["type"].cast<std::string>() != "sound") {
        continue;
      }
      int assetIndex = segment["asset"].cast<int>();
      std::string assetName = "";
      if (assetIndex >= 0 && assetIndex < assets.size()) {
        assetName = assets[assetIndex];
      }
      float startTime = segment["startFrame"]["time"].cast<float>();
      float endTime = segment["endFrame"]["time"].cast<float>();
      int startFrameIndex = std::max(
          0, static_cast<int>(std::floor(startTime * frameRate + 1e-5)));
      int endFrameIndex =
          std::max(0, static_cast<int>(std::ceil(endTime * frameRate - 1e-5)));
      py::dict soundEntry;
      soundEntry["asset"] = assetName;
      soundEntry["startFrame"] = startFrameIndex;
      soundEntry["endFrame"] = endFrameIndex;
      if (segment.contains("originalDuration")) {
        soundEntry["originalDuration"] = segment["originalDuration"];
      }
      sounds.push_back(soundEntry);
    }
  }
  float duration = frameRate > 0 ? (1.0 * frameCount / frameRate) : 0.0f;
  return std::make_tuple(duration, frames, sounds);
}
