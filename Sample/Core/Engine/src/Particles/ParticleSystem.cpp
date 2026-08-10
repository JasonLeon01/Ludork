#include "Particles/ParticleSystem.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <memory>
#include <utility>

ParticleSystem::ParticleSystem() = default;

ParticleSystem::~ParticleSystem() = default;

void ParticleSystem::addParticle(const std::shared_ptr<Particle>& particle) {
    assert(particle != nullptr);
    if (particle == nullptr) {
        return;
    }
    const std::shared_ptr<ParticleSystem> parent = weak_from_this().lock();
    assert(parent != nullptr);
    if (parent == nullptr) {
        return;
    }

    auto it = resourceDict_.find(particle->resourcePath);
    if (it == resourceDict_.end()) {
        std::unique_ptr<sf::Texture> texture =
            std::make_unique<sf::Texture>(particle->resourcePath);
        const sf::Vector2u textureSize = texture->getSize();
        resourceDict_.emplace(particle->resourcePath, std::move(texture));
        particles_[particle->resourcePath] =
            std::vector<std::shared_ptr<Particle>>();
        vertexArrays_[particle->resourcePath] =
            sf::VertexArray(sf::PrimitiveType::Triangles, 0);
        const unsigned int width = textureSize.x;
        const unsigned int height = textureSize.y;
        textureUV_[particle->resourcePath] = std::make_tuple(
            width, height, sf::Vector2f(0, 0), sf::Vector2f(width, 0),
            sf::Vector2f(width, height), sf::Vector2f(0, height));
    }
    particle->setParent(parent);
    const ParticleInfo info = particle->info;
    particles_[particle->resourcePath].push_back(particle);
    auto [width, height, uv_tl, uv_tr, uv_br, uv_bl] =
        textureUV_[particle->resourcePath];
    auto [tl_tr, tr_tr, br_tr, bl_tr] = getUpdateParticleInfo(particle.get());

    sf::Transform t = sf::Transform();
    t.translate(info.position);
    t.rotate(info.rotation);
    t.scale(info.scale);

    sf::Vector2f tl = t.transformPoint(tl_tr);
    sf::Vector2f tr = t.transformPoint(tr_tr);
    sf::Vector2f br = t.transformPoint(br_tr);
    sf::Vector2f bl = t.transformPoint(bl_tr);

    sf::Vertex vertex0 = sf::Vertex();
    vertex0.position = tl;
    vertex0.texCoords = uv_tl;
    vertex0.color = info.color;
    sf::Vertex vertex1 = sf::Vertex();
    vertex1.position = tr;
    vertex1.texCoords = uv_tr;
    vertex1.color = info.color;
    sf::Vertex vertex2 = sf::Vertex();
    vertex2.position = br;
    vertex2.texCoords = uv_br;
    vertex2.color = info.color;
    sf::Vertex vertex3 = sf::Vertex();
    vertex3.position = tl;
    vertex3.texCoords = uv_tl;
    vertex3.color = info.color;
    sf::Vertex vertex4 = sf::Vertex();
    vertex4.position = br;
    vertex4.texCoords = uv_br;
    vertex4.color = info.color;
    sf::Vertex vertex5 = sf::Vertex();
    vertex5.position = bl;
    vertex5.texCoords = uv_bl;
    vertex5.color = info.color;

    vertexArrays_[particle->resourcePath].append(vertex0);
    vertexArrays_[particle->resourcePath].append(vertex1);
    vertexArrays_[particle->resourcePath].append(vertex2);
    vertexArrays_[particle->resourcePath].append(vertex3);
    vertexArrays_[particle->resourcePath].append(vertex4);
    vertexArrays_[particle->resourcePath].append(vertex5);
}

void ParticleSystem::addText(const std::shared_ptr<TextParticle>& text) {
    assert(text != nullptr);
    if (text == nullptr) {
        return;
    }
    const std::shared_ptr<ParticleSystem> parent = weak_from_this().lock();
    assert(parent != nullptr);
    if (parent == nullptr) {
        return;
    }
    text->setParent(parent);
    texts_.push_back(text);
}

void ParticleSystem::removeParticle(Particle* particle) {
    assert(particle != nullptr);
    if (particle == nullptr) {
        return;
    }
    auto it = particles_.find(particle->resourcePath);
    assert(it != particles_.end());
    if (it == particles_.end()) {
        return;
    }
    const auto& plist = it->second;
    auto it2 =
        std::find_if(plist.begin(), plist.end(),
                     [particle](const std::shared_ptr<Particle>& current) {
                         return current.get() == particle;
                     });
    assert(it2 != plist.end());
    if (it2 == plist.end()) {
        return;
    }
    const int index = static_cast<int>(std::distance(plist.begin(), it2));
    removeParticleAt(particle->resourcePath, index);
}

void ParticleSystem::removeText(TextParticle* text) {
    assert(text != nullptr);
    if (text == nullptr) {
        return;
    }
    auto it =
        std::find_if(texts_.begin(), texts_.end(),
                     [text](const std::shared_ptr<TextParticle>& current) {
                         return current.get() == text;
                     });
    assert(it != texts_.end());
    if (it == texts_.end()) {
        return;
    }
    const std::shared_ptr<TextParticle> removed = *it;
    texts_.erase(it);
    removed->setParent(nullptr);
}

void ParticleSystem::removeParticleAt(const std::string& resourcePath,
                                      int index) {
    auto particleGroup = particles_.find(resourcePath);
    assert(particleGroup != particles_.end());
    if (particleGroup == particles_.end()) {
        return;
    }
    auto& plist = particleGroup->second;
    assert(index >= 0 && index < static_cast<int>(plist.size()));
    if (index < 0 || index >= static_cast<int>(plist.size())) {
        return;
    }
    auto& va = vertexArrays_.at(resourcePath);
    const std::size_t particleIndex = static_cast<std::size_t>(index);
    const std::shared_ptr<Particle> particle = plist[particleIndex];

    const std::size_t previousCount = plist.size();
    if (particleIndex + 1 < previousCount) {
        for (std::size_t i = particleIndex; i + 1 < previousCount; ++i) {
            const std::size_t src = (i + 1) * 6;
            const std::size_t dst = i * 6;
            for (std::size_t k = 0; k < 6; ++k) {
                va[dst + k] = va[src + k];
            }
        }
    }
    plist.erase(plist.begin() + index);
    va.resize((previousCount - 1) * 6);
    particle->setParent(nullptr);

    if (plist.empty()) {
        particles_.erase(resourcePath);
        vertexArrays_.erase(resourcePath);
        resourceDict_.erase(resourcePath);
        textureUV_.erase(resourcePath);
    }
}

void ParticleSystem::addUpdateFlag(Particle* particle) {
    if (particle == nullptr) {
        return;
    }
    const auto group = particles_.find(particle->resourcePath);
    if (group == particles_.end()) {
        return;
    }
    const auto item =
        std::find_if(group->second.begin(), group->second.end(),
                     [particle](const std::shared_ptr<Particle>& current) {
                         return current.get() == particle;
                     });
    if (item != group->second.end()) {
        updateFlags_.push_back(*item);
    }
}

void ParticleSystem::updateParticlesInfo() {
    for (const std::weak_ptr<Particle>& particleReference : updateFlags_) {
        const std::shared_ptr<Particle> particle = particleReference.lock();
        if (particle == nullptr) {
            continue;
        }
        const std::string& resourcePath = particle->resourcePath;
        const auto group = particles_.find(resourcePath);
        if (group == particles_.end()) {
            continue;
        }
        const auto& particleList = group->second;
        const auto particleItem =
            std::find_if(particleList.begin(), particleList.end(),
                         [&particle](const std::shared_ptr<Particle>& current) {
                             return current == particle;
                         });
        if (particleItem == particleList.end()) {
            continue;
        }
        const std::size_t index = static_cast<std::size_t>(
            std::distance(particleList.begin(), particleItem));

        sf::Transform t = sf::Transform();
        t.translate(particle->info.position);
        t.rotate(particle->info.rotation);
        t.scale(particle->info.scale);
        const sf::Color infoColor = particle->info.color;
        auto [tl_tr, tr_tr, br_tr, bl_tr] =
            getUpdateParticleInfo(particle.get());
        sf::Vector2f tl = t.transformPoint(tl_tr);
        sf::Vector2f tr = t.transformPoint(tr_tr);
        sf::Vector2f br = t.transformPoint(br_tr);
        sf::Vector2f bl = t.transformPoint(bl_tr);

        const auto it = vertexArrays_.find(resourcePath);
        if (it == vertexArrays_.end()) {
            continue;
        }
        auto& vertexArray = it->second;
        vertexArray[index * 6 + 0].position = tl;
        vertexArray[index * 6 + 1].position = tr;
        vertexArray[index * 6 + 2].position = br;
        vertexArray[index * 6 + 3].position = tl;
        vertexArray[index * 6 + 4].position = br;
        vertexArray[index * 6 + 5].position = bl;
        for (std::size_t i = 0; i < 6; ++i) {
            vertexArray[index * 6 + i].color = infoColor;
        }
    }
}

void ParticleSystem::onTick(float deltaTime) {
    std::vector<std::shared_ptr<Particle>> particles;
    for (const auto& [resourcePath, group] : particles_) {
        static_cast<void>(resourcePath);
        particles.insert(particles.end(), group.begin(), group.end());
    }
    for (const std::shared_ptr<Particle>& particle : particles) {
        if (particle != nullptr && particle->getParent().get() == this) {
            particle->onTick(deltaTime);
        }
    }
    const std::vector<std::shared_ptr<TextParticle>> texts = texts_;
    for (const std::shared_ptr<TextParticle>& text : texts) {
        if (text != nullptr && text->getParent().get() == this) {
            text->onTick(deltaTime);
        }
    }
    if (!updateFlags_.empty()) {
        updateParticlesInfo();
        updateFlags_.clear();
    }
}

void ParticleSystem::onLateTick(float deltaTime) {
    std::vector<std::shared_ptr<Particle>> particles;
    for (const auto& [resourcePath, group] : particles_) {
        static_cast<void>(resourcePath);
        particles.insert(particles.end(), group.begin(), group.end());
    }
    for (const std::shared_ptr<Particle>& particle : particles) {
        if (particle != nullptr && particle->getParent().get() == this) {
            particle->onLateTick(deltaTime);
        }
    }
    const std::vector<std::shared_ptr<TextParticle>> texts = texts_;
    for (const std::shared_ptr<TextParticle>& text : texts) {
        if (text != nullptr && text->getParent().get() == this) {
            text->onLateTick(deltaTime);
        }
    }
}

void ParticleSystem::onFixedTick(float fixedDelta) {
    std::vector<std::shared_ptr<Particle>> particles;
    for (const auto& [resourcePath, group] : particles_) {
        static_cast<void>(resourcePath);
        particles.insert(particles.end(), group.begin(), group.end());
    }
    for (const std::shared_ptr<Particle>& particle : particles) {
        if (particle != nullptr && particle->getParent().get() == this) {
            particle->onFixedTick(fixedDelta);
        }
    }
    const std::vector<std::shared_ptr<TextParticle>> texts = texts_;
    for (const std::shared_ptr<TextParticle>& text : texts) {
        if (text != nullptr && text->getParent().get() == this) {
            text->onFixedTick(fixedDelta);
        }
    }
}

void ParticleSystem::draw(sf::RenderTarget& target,
                          sf::RenderStates states) const {
    auto originTexture = states.texture;
    for (const auto& [resourcePath, vertexArray] : vertexArrays_) {
        states.texture = resourceDict_.at(resourcePath).get();
        target.draw(vertexArray, states);
    }
    states.texture = originTexture;
    for (const std::shared_ptr<TextParticle>& text : texts_) {
        if (text != nullptr) {
            target.draw(*text, states);
        }
    }
}

std::tuple<sf::Vector2f, sf::Vector2f, sf::Vector2f, sf::Vector2f>
ParticleSystem::getUpdateParticleInfo(Particle* particle) {
    auto [width, height, uv_tl, uv_tr, uv_br, uv_bl] =
        textureUV_[particle->resourcePath];
    auto halfSize = sf::Vector2f(width / 2, height / 2);
    auto tl_tr = sf::Vector2f(-halfSize.x, -halfSize.y);
    auto tr_tr = sf::Vector2f(halfSize.x, -halfSize.y);
    auto br_tr = sf::Vector2f(halfSize.x, halfSize.y);
    auto bl_tr = sf::Vector2f(-halfSize.x, halfSize.y);
    return std::make_tuple(tl_tr, tr_tr, br_tr, bl_tr);
}
