#include "Particles/ParticleSystem.hpp"

#include <iterator>

void ParticleSystem::addParticle(Particle* particle) {
    auto it = resourceDict_.find(particle->resourcePath);
    if (it == resourceDict_.end()) {
        auto texture = new sf::Texture(particle->resourcePath);
        resourceDict_[particle->resourcePath] = texture;
        particles_[particle->resourcePath] = std::vector<Particle*>();
        vertexArrays_[particle->resourcePath] = sf::VertexArray(sf::PrimitiveType::Triangles, 0);
        unsigned int width = texture->getSize().x;
        unsigned int height = texture->getSize().y;
        textureUV_[particle->resourcePath] = std::make_tuple(width, height, sf::Vector2f(0, 0), sf::Vector2f(width, 0),
                                                             sf::Vector2f(width, height), sf::Vector2f(0, height));
    }
    particle->setParent(this);
    auto info = particle->info;
    particles_[particle->resourcePath].push_back(particle);
    auto [width, height, uv_tl, uv_tr, uv_br, uv_bl] = textureUV_[particle->resourcePath];
    auto [tl_tr, tr_tr, br_tr, bl_tr] = getUpdateParticleInfo(particle);

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

void ParticleSystem::addText(TextParticle* text) {
    text->setParent(this);
    texts_.push_back(text);
}

void ParticleSystem::removeParticle(Particle* particle) {
    auto it = particles_.find(particle->resourcePath);
    assert(it != particles_.end());
    auto plist = it->second;
    auto it2 = std::find(plist.begin(), plist.end(), particle);
    assert(it2 != plist.end());
    int index = std::distance(plist.begin(), it2);
    removeParticleAt(particle->resourcePath, index);
}

void ParticleSystem::removeText(TextParticle* text) {
    auto it = std::find(texts_.begin(), texts_.end(), text);
    assert(it != texts_.end());
    texts_.erase(it);
    text->setParent(nullptr);
}

void ParticleSystem::removeParticleAt(const std::string& resourcePath, int index) {
    auto& plist = particles_.at(resourcePath);
    auto& va = vertexArrays_.at(resourcePath);
    auto particle = plist[index];

    int n_before = plist.size();
    bool result = (n_before == 1);
    if (index != n_before - 1) {
        for (int i = index; i < n_before - 1; ++i) {
            int src = (i + 1) * 6;
            int dst = i * 6;
            for (int k = 0; k < 6; ++k) {
                va[dst + k] = va[src + k];
            }
        }
        plist.erase(plist.begin() + index);
        va.resize((n_before - 1) * 6);
    }

    if (result) {
        delete resourceDict_[resourcePath];
        particles_.erase(resourcePath);
        vertexArrays_.erase(resourcePath);
        resourceDict_.erase(resourcePath);
        textureUV_.erase(resourcePath);
    }
    particle->setParent(nullptr);
}

void ParticleSystem::addUpdateFlag(Particle* particle) { updateFlags_.push_back(particle); }

void ParticleSystem::updateParticlesInfo() {
    for (auto& particle : updateFlags_) {
        auto resourcePath = particle->resourcePath;
        const auto& particleList = particles_.at(resourcePath);
        int idx = std::distance(particleList.begin(), std::find(particleList.begin(), particleList.end(), particle));
        if (idx >= static_cast<int>(particleList.size())) {
            continue;
        }

        sf::Transform t = sf::Transform();
        t.translate(particle->info.position);
        t.rotate(particle->info.rotation);
        t.scale(particle->info.scale);
        auto infoColor = particle->info.color;
        auto [tl_tr, tr_tr, br_tr, bl_tr] = getUpdateParticleInfo(particle);
        sf::Vector2f tl = t.transformPoint(tl_tr);
        sf::Vector2f tr = t.transformPoint(tr_tr);
        sf::Vector2f br = t.transformPoint(br_tr);
        sf::Vector2f bl = t.transformPoint(bl_tr);

        auto it = vertexArrays_.find(resourcePath);
        if (it == vertexArrays_.end()) {
            continue;
        }
        auto& vertexArray = it->second;
        vertexArray[idx * 6 + 0].position = tl;
        vertexArray[idx * 6 + 1].position = tr;
        vertexArray[idx * 6 + 2].position = br;
        vertexArray[idx * 6 + 3].position = tl;
        vertexArray[idx * 6 + 4].position = br;
        vertexArray[idx * 6 + 5].position = bl;
        for (int i = 0; i < 6; ++i) {
            vertexArray[idx * 6 + i].color = infoColor;
        }
    }
}

void ParticleSystem::onTick(float deltaTime) {
    for (auto& [_, plist] : particles_) {
        for (auto& particle : plist) {
            particle->onTick(deltaTime);
        }
    }
    for (auto& text : texts_) {
        text->onTick(deltaTime);
    }
    if (!updateFlags_.empty()) {
        updateParticlesInfo();
        updateFlags_.clear();
    }
}

void ParticleSystem::onLateTick(float deltaTime) {
    for (auto& [_, plist] : particles_) {
        for (auto& particle : plist) {
            particle->onLateTick(deltaTime);
        }
    }
    for (auto& text : texts_) {
        text->onLateTick(deltaTime);
    }
}

void ParticleSystem::onFixedTick(float fixedDelta) {
    for (auto& [_, plist] : particles_) {
        for (auto& particle : plist) {
            particle->onFixedTick(fixedDelta);
        }
    }
    for (auto& text : texts_) {
        text->onFixedTick(fixedDelta);
    }
}

void ParticleSystem::draw(sf::RenderTarget& target, sf::RenderStates states) const {
    auto originTexture = states.texture;
    for (auto& [resourcePath, vertexArray] : vertexArrays_) {
        states.texture = resourceDict_.at(resourcePath);
        target.draw(vertexArray, states);
    }
    states.texture = originTexture;
    for (auto& text : texts_) {
        target.draw(*text, states);
    }
}

std::tuple<sf::Vector2f, sf::Vector2f, sf::Vector2f, sf::Vector2f> ParticleSystem::getUpdateParticleInfo(
    Particle* particle) {
    auto [width, height, uv_tl, uv_tr, uv_br, uv_bl] = textureUV_[particle->resourcePath];
    auto halfSize = sf::Vector2f(width / 2, height / 2);
    auto tl_tr = sf::Vector2f(-halfSize.x, -halfSize.y);
    auto tr_tr = sf::Vector2f(halfSize.x, -halfSize.y);
    auto br_tr = sf::Vector2f(halfSize.x, halfSize.y);
    auto bl_tr = sf::Vector2f(-halfSize.x, halfSize.y);
    return std::make_tuple(tl_tr, tr_tr, br_tr, bl_tr);
}

ParticleSystem::~ParticleSystem() {
    for (auto& [_, texture] : resourceDict_) {
        delete texture;
    }
}
