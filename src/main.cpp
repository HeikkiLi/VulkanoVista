
#include "Engine.h"

int main() 
{
    Engine engine;
   
    // Initialize the engine
    engine.init();
   
    // Run the main loop
    engine.run();
   
    // Cleanup resources
    engine.cleanup();

    return 0;
}
