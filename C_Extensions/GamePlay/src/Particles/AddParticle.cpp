#include <Particles/AddParticle.h>
#include <SFML/Graphics/Transform.hpp>
#include <SFML/Graphics/Vertex.hpp>
#include <pybind11/stl.h>

void C_AddParticle(const sf::Vector2f &infoPosition,
                   const sf::Angle &infoRotation, const sf::Vector2f &infoScale,
                   const sf::Color &infoColor, const sf::Vector2f &uv_tl,
                   const sf::Vector2f &uv_tr, const sf::Vector2f &uv_br,
                   const sf::Vector2f &uv_bl, const sf::Vector2f &tl_tr,
                   const sf::Vector2f &tr_tr, const sf::Vector2f &br_tr,
                   const sf::Vector2f &bl_tr, sf::VertexArray &vertexArray) {
  sf::Transform t = sf::Transform();
  t.translate(infoPosition);
  t.rotate(infoRotation);
  t.scale(infoScale);

  sf::Vector2f tl = t.transformPoint(tl_tr);
  sf::Vector2f tr = t.transformPoint(tr_tr);
  sf::Vector2f br = t.transformPoint(br_tr);
  sf::Vector2f bl = t.transformPoint(bl_tr);

  sf::Vertex vertex0 = sf::Vertex();
  vertex0.position = tl;
  vertex0.texCoords = uv_tl;
  vertex0.color = infoColor;
  sf::Vertex vertex1 = sf::Vertex();
  vertex1.position = tr;
  vertex1.texCoords = uv_tr;
  vertex1.color = infoColor;
  sf::Vertex vertex2 = sf::Vertex();
  vertex2.position = br;
  vertex2.texCoords = uv_br;
  vertex2.color = infoColor;
  sf::Vertex vertex3 = sf::Vertex();
  vertex3.position = tl;
  vertex3.texCoords = uv_tl;
  vertex3.color = infoColor;
  sf::Vertex vertex4 = sf::Vertex();
  vertex4.position = br;
  vertex4.texCoords = uv_br;
  vertex4.color = infoColor;
  sf::Vertex vertex5 = sf::Vertex();
  vertex5.position = bl;
  vertex5.texCoords = uv_bl;
  vertex5.color = infoColor;

  vertexArray.append(vertex0);
  vertexArray.append(vertex1);
  vertexArray.append(vertex2);
  vertexArray.append(vertex3);
  vertexArray.append(vertex4);
  vertexArray.append(vertex5);
}

void ApplyAddParticleBinding(py::module &m) {
  m.def("C_AddParticle", &C_AddParticle, py::arg("infoPosition"),
        py::arg("infoRotation"), py::arg("infoScale"), py::arg("infoColor"),
        py::arg("uv_tl"), py::arg("uv_tr"), py::arg("uv_br"), py::arg("uv_bl"),
        py::arg("tl_tr"), py::arg("tr_tr"), py::arg("br_tr"), py::arg("bl_tr"),
        py::arg("vertexArray"));
}
