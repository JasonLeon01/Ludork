#include "Particles.hpp"

void ApplyParticlesBinding(py::module &m) {
    py::class_<ParticleInfo> ParticleInfoClass(m, "ParticleInfo");
    ParticleInfoClass.def(py::init<>());
    ParticleInfoClass.def_readwrite("position", &ParticleInfo::position);
    ParticleInfoClass.def_readwrite("color", &ParticleInfo::color);
    ParticleInfoClass.def_readwrite("rotation", &ParticleInfo::rotation);
    ParticleInfoClass.def_readwrite("scale", &ParticleInfo::scale);

    py::class_<ParticleBase> ParticleBaseClass(m, "ParticleBase");
    ParticleBaseClass.def(py::init<ParticleSystem *, std::function<void(float, float, ParticleBase *)>, float>(),
                          py::arg("parent"), py::arg("moveFunction"), py::arg("countTime"));
    ParticleBaseClass.def("onTick", &ParticleBase::onTick, py::arg("deltaTime"));
    ParticleBaseClass.def("onLateTick", &ParticleBase::onLateTick, py::arg("deltaTime"));
    ParticleBaseClass.def("onFixedTick", &ParticleBase::onFixedTick, py::arg("fixedDelta"));
    ParticleBaseClass.def("getCountTime", &ParticleBase::getCountTime);
    ParticleBaseClass.def("getParent", &ParticleBase::getParent, py::return_value_policy::reference_internal);

    py::class_<Particle, ParticleBase> ParticleClass(m, "Particle");
    ParticleClass.def(py::init<ParticleSystem *, std::function<void(float, float, ParticleBase *)>, float,
                               const std::string &, ParticleInfo>(),
                      py::arg("parent"), py::arg("moveFunction"), py::arg("countTime"), py::arg("resourcePath"),
                      py::arg("info"));
    ParticleClass.def("onTick", &Particle::onTick, py::arg("deltaTime"));
    ParticleClass.def("onLateTick", &Particle::onLateTick, py::arg("deltaTime"));
    ParticleClass.def("onFixedTick", &Particle::onFixedTick, py::arg("fixedDelta"));
    ParticleClass.def("destroy", &Particle::destroy);
    ParticleClass.def_readwrite("resourcePath", &Particle::resourcePath);
    ParticleClass.def_readwrite("info", &Particle::info);

    py::class_<TextParticle, ParticleBase, sf::Text> TextParticleClass(m, "TextParticle");
    TextParticleClass.def(py::init<ParticleSystem *, std::function<void(float, float, ParticleBase *)>, float,
                                   const std::string &, const sf::Font &, unsigned int>(),
                          py::arg("parent"), py::arg("moveFunction"), py::arg("countTime"), py::arg("text"),
                          py::arg("font"), py::arg("characterSize"));

    py::class_<ParticleSystem, sf::Drawable> ParticleSystemClass(m, "ParticleSystem");
    ParticleSystemClass.def(py::init<>());
    ParticleSystemClass.def("addParticle", &ParticleSystem::addParticle, py::arg("particle"));
    ParticleSystemClass.def("addText", &ParticleSystem::addText, py::arg("text"));
    ParticleSystemClass.def("removeParticle", &ParticleSystem::removeParticle, py::arg("particle"));
    ParticleSystemClass.def("removeText", &ParticleSystem::removeText, py::arg("text"));
    ParticleSystemClass.def("removParticleAt", &ParticleSystem::removeParticleAt, py::arg("resourcePath"),
                            py::arg("index"));
    ParticleSystemClass.def("addUpdateFlag", &ParticleSystem::addUpdateFlag, py::arg("particle"));
    ParticleSystemClass.def("updateParticlesInfo", &ParticleSystem::updateParticlesInfo);
    ParticleSystemClass.def("onTick", &ParticleSystem::onTick, py::arg("deltaTime"));
    ParticleSystemClass.def("onLateTick", &ParticleSystem::onLateTick, py::arg("deltaTime"));
    ParticleSystemClass.def("onFixedTick", &ParticleSystem::onFixedTick, py::arg("fixedDelta"));
}
