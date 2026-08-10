#include <Application.hpp>
#include <SFML/Config.hpp>

#include <cstdlib>

#if defined(SFML_SYSTEM_IOS) || defined(SFML_SYSTEM_HARMONY)
#include <SFML/Main.hpp>
#endif

int main(int argc, char** argv) {
    const int result = ludork::application::run(argc, argv);
#if defined(SFML_SYSTEM_IOS)
    std::exit(result);
#else
    return result;
#endif
}
