#define STB_IMAGE_IMPLEMENTATION

#include "Engine.h"

int main() 
{
    Engine engine;
   
    // Initialize the engine
    if (engine.init() == EXIT_FAILURE)
    {
        return EXIT_FAILURE;
    }
   
    // Run the main loop
    engine.run();
   
    // Cleanup resources
    engine.cleanup();

    return 0;
}
