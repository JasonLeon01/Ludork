#pragma once

#include <System/GraphicsTypes.hpp>

#include <SFML/Graphics.hpp>

#include <memory>
#include <optional>
#include <string>

namespace ludork::global::system_frame_pipeline_impl {

void initCanvas(const sf::Vector2u& size);
void draw(const sf::Drawable& drawable, sf::Shader* shader);
void composeFrame(float deltaTime);
void present();
void completeFrame();
void addGraphicsShader(const std::shared_ptr<sf::Shader>& shader,
                       std::optional<ShaderUniforms> uniforms = std::nullopt);
void removeGraphicsShader(const std::shared_ptr<sf::Shader>& shader);
void removeAllGraphicsShaders();
void removeGraphicsShaderAt(int index);
void applyGraphicsShadersLength();
void setShaderUniform(sf::Shader& shader, const std::string& name,
                      const ShaderUniformValue& value);
bool shadersAvailable();

}  // namespace ludork::global::system_frame_pipeline_impl
