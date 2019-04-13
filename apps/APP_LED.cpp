//
// Created by tkhamvilai on 4/9/19.
//

#include "APP_LED.hpp"

void APP_LED(int color)
{
#if defined(__x86_64__)
    if(color == LED_OFF)
    {
        std::cout << "OFF" << std::endl;
    }
    else if(color == LED_WHITE)
    {
        std::cout << "WHITE" << std::endl;
    }
    else if(color == LED_RED)
    {
        std::cout << "RED" << std::endl;
    }
    else if(color == LED_GREEN)
    {
        std::cout << "GREEN" << std::endl;
    }
    else if(color == LED_BLUE)
    {
        std::cout << "BLUE" << std::endl;
    }
#else
    int RED_LED   = 26;
    int GREEN_LED = 27;
    int BLUE_LED  = 28;
    pinMode (RED_LED, OUTPUT);
    pinMode (GREEN_LED, OUTPUT);
    pinMode (BLUE_LED, OUTPUT);
    if(color == LED_OFF)
    {
        digitalWrite (RED_LED,  LOW);
        digitalWrite (GREEN_LED,  LOW);
        digitalWrite (BLUE_LED,  LOW);
    }
    else if(color == LED_WHITE)
    {
        digitalWrite (RED_LED,  HIGH);
        digitalWrite (GREEN_LED,  HIGH);
        digitalWrite (BLUE_LED,  HIGH);
    }
    else if(color == LED_RED)
    {
        digitalWrite (RED_LED,  HIGH);
        digitalWrite (GREEN_LED,  LOW);
        digitalWrite (BLUE_LED,  LOW);
    }
    else if(color == LED_GREEN)
    {
        digitalWrite (RED_LED,  LOW);
        digitalWrite (GREEN_LED,  HIGH);
        digitalWrite (BLUE_LED,  LOW);
    }
    else if(color == LED_BLUE)
    {
        digitalWrite (RED_LED,  LOW);
        digitalWrite (GREEN_LED,  LOW);
        digitalWrite (BLUE_LED,  HIGH);
    }
#endif
}