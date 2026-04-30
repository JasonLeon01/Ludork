project(EngineExt)

pybind11_add_module(EngineExt
    "main.cpp"
    "src/Particles/ParticleBase.cpp"
    "src/Particles/Particle.cpp"
    "src/Particles/TextParticle.cpp"
    "src/Particles/ParticleSystem.cpp"
    "src/Particles.cpp"
    "src/Graphics/CompressAnimation.cpp"
    "src/Graphics/RectBase.cpp"
    "src/Graphics/Color.cpp"
    "src/Graphics/TilemapGraphics.cpp"
)

ConfigureTarget(EngineExt)
ConfigureSFMLTarget(EngineExt)
